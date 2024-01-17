#include <stdio.h>
#include "socketlib.h"
#include <stdbool.h>
#include <stdint.h>
#ifndef C_ONLY
#include <cstring>
#else
#include <string.h>
#endif

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int server_socket, new_socket;
uint64_t socket_cycles;

#define MAX_DEQUE_SIZE 64
#define HEAP_SPACE MAX_DEQUE_SIZE * 2048

char *received_messages[HEAP_SPACE]; // 2M of "heap"
uint32_t heap_head = 0;

void *hmalloc(int size) {
  if ((HEAP_SPACE - (int) heap_head) < size) {
    heap_head = 0; // wrap around;
  }

  void *ret = &received_messages[heap_head];
  heap_head += size;
  return ret;
}

typedef struct {
    message_packet_t items[MAX_DEQUE_SIZE];
    int front;
    int rear;
    int size;
} deque_t;

deque_t received_packets;

uint64_t mmio_call(const uint64_t fid, const uint64_t a1, const uint64_t a2,
    const uint64_t a3, const uint64_t a4, const char* payload, const size_t len) {
  mmio_req_t *mreq = (mmio_req_t *) (0x90000000ULL);
  mmio_rsp_t *mrsp = (mmio_rsp_t *) (0x91000000ULL);
  // assert(!mreq->valid && !mrsp->valid);
  mreq->func_id = fid;
  mreq->a1 = a1;
  mreq->a2 = a2;
  mreq->a3 = a3;
  mreq->a4 = a4;
  if (payload != NULL) {
    memcpy(&(mreq->payload), payload, len);
  }
  asm volatile ("fence");
  mreq->valid = true;
  asm volatile ("fence");
  int counter = 0;
  while (!mrsp->valid) {
    asm volatile ("fence");
    if (++counter > 100000) {
      printf("still waiting\n");
      counter = 0;
    }
  }
  uint64_t retval = mrsp->ret;
  mrsp->valid = false;
  return retval;
}

inline static uint64_t sock_read_cycles() {
  uint64_t cycles;
  asm volatile ("rdcycle %0" : "=r" (cycles));
  return cycles;
}

#ifdef C_ONLY
#define CYCLES_START() \
  uint64_t ent_cycles = sock_read_cycles();
#define CYCLES_END() \
  socket_cycles = socket_cycles + (sock_read_cycles() - ent_cycles);
#else
#define CYCLES_START() \
  uint64_t ent_cycles = sock_read_cycles();
#define CYCLES_END() \
  socket_cycles = socket_cycles + (sock_read_cycles() - ent_cycles);
// #define CYCLES_START()
// #define CYCLES_END()
#endif

#ifndef C_ONLY
void init_client_file(const char *socket_path) {
  init_client_file(socket_path, NOSERV);
}
#endif

void init_client_file(const char *socket_path, const endpoint_id_t endpoint_id) {
  CYCLES_START()
  received_packets.front = -1;
  received_packets.rear = -1;
  received_packets.size = 0;
  printf("init client file %p\n", socket_path);
  new_socket = mmio_call(M_CLIENT_FILE, (uint64_t) socket_path, (uint64_t) endpoint_id, 0, 0, NULL, 0);
  CYCLES_END()
}

void init_client(const uint32_t port, const endpoint_id_t endpoint_id) {
  CYCLES_START()
  received_packets.front = -1;
  received_packets.rear = -1;
  received_packets.size = 0;
  printf("init client port %d\n", port);
  new_socket = mmio_call(M_CLIENT_PORT, (uint64_t) port, (uint64_t) endpoint_id, 0, 0, NULL, 0);
  CYCLES_END()
}

inline size_t recv_mmio(const int sockfd, const void* buf, const int len, const int flags) {
  return mmio_call(M_RECV, sockfd, (uint64_t) buf, len, flags, NULL, 0);
}

inline size_t send_mmio(const int sockfd, const void* buf, const int len, const int flags) {
  return mmio_call(M_SEND, sockfd, (uint64_t) buf, len, flags, NULL, 0);
}

void fetch_packets() {
  message_header_t header;
  while ((int64_t) recv_mmio(new_socket, &header, sizeof(header), _MSG_PEEK | _MSG_DONTWAIT) > 0) { // more data to come
    if (received_packets.size >= MAX_DEQUE_SIZE) {
      printf("deque full, stop fetch\n");
      break;
    }
#ifdef SOCKETLIB_VERBOSE
    printf("DEBUG: new data is arriving\n");
#endif
    // fcntl(new_socket, F_SETFL, 0);
    ssize_t header_bytes_received = recv_mmio(new_socket, &header, sizeof(message_header_t), 0);
    if (header_bytes_received != sizeof(message_header_t)) {
      printf("ERROR: Error receiving header; received %d bytes\n", header_bytes_received);
    }
    char *message_data = (char *) hmalloc(header.size - sizeof(message_header_t));
#ifdef SOCKETLIB_VERBOSE
    printf("DEBUG: start receiving message with size %d\n", header.size);
#endif
    size_t bytes_received = 0;
    while (bytes_received < header.size - sizeof(message_header_t)) {
      size_t chunk_size = MIN(1024ul, header.size - sizeof(message_header_t) - bytes_received);
      ssize_t chunk_bytes_received = recv_mmio(new_socket, message_data + bytes_received, chunk_size, 0);
      if (chunk_bytes_received == -1) {
        printf("ERROR: Error receiving message data\n");
        return;
      }
      bytes_received += chunk_bytes_received;
    }
    // fcntl(new_socket, F_SETFL, O_NONBLOCK);
#ifdef SOCKETLIB_VERBOSE
    printf("DEBUG: added packet with func id %d\n", header.func_id);
#endif
    message_packet_t new_packet;
    new_packet.header = header;
    new_packet.payload = message_data;
    if (received_packets.size >= MAX_DEQUE_SIZE) {
      printf("ERROR: Receive deque is full, cannot add more items\n");
      return;
    }
    if (received_packets.front == -1) {
      received_packets.front = 0;
    }
    received_packets.rear = (received_packets.rear + 1) % MAX_DEQUE_SIZE;
    received_packets.items[received_packets.rear] = new_packet;
    received_packets.size++;
  }
}

int socket_receive_c(const func_id_t func_id, const bool blocking, char **dest_buf) {
  CYCLES_START()
  // check for qualifying messages
  // if not found, fetch from socket
  // if blocking, fetch & check in a loop
  endpoint_id_t src_id = 0;
  if (blocking) {
    printf("Blocking is unsupported for performance reasons\n");
  } else {
    if (received_packets.size == 0) fetch_packets();
    if (received_packets.size > 0) {
      message_packet_t front = received_packets.items[received_packets.front];
      if (front.header.func_id == func_id) {
        received_packets.size--;
        received_packets.front = (received_packets.front + 1) % MAX_DEQUE_SIZE;
        src_id = front.header.src_id;
        *dest_buf = front.payload;
      }
    }
  }
  CYCLES_END()
  return (int) src_id;
}

int socket_send_c(const endpoint_id_t endpoint_id, const func_id_t func_id,
    const char *args, const int args_len, const char *payload, const int payload_len) {
  CYCLES_START()
  message_header_t header;
  header.size = sizeof(message_header_t) + args_len + payload_len;
  header.src_id = NOSERV; // will be overriden by server
  header.dst_id = endpoint_id;
  header.func_id = func_id;

  if ((uint64_t) send_mmio(new_socket, &header, sizeof(header), 0) != sizeof(header)) {
    printf("ERROR: DEBUG: failed to send header\n");
    CYCLES_END()
    return -1;
  }
  if ((uint64_t) send_mmio(new_socket, args, args_len, 0) != (uint64_t) args_len) {
    printf("ERROR: DEBUG: failed to send args\n");
    CYCLES_END()
    return -1;
  }

  for (size_t i = 0; i < (size_t) payload_len;) {
    size_t chunk_size = MIN(1024ul, payload_len - i);
    const char* data_ptr = payload + i;
    ssize_t bytes_sent = send_mmio(new_socket, data_ptr, chunk_size, 0);
#ifdef SOCKETLIB_VERBOSE
    printf("DEBUG: sent %d bytes\n", bytes_sent);
#endif
    i += bytes_sent;
    if (bytes_sent == -1) {
      printf("ERROR: DEBUG: failed to send everything\n");
      CYCLES_END()
      return -1;
    }
  }
  CYCLES_END()
  return 0;
}
