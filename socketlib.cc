#include <stdlib.h>
#include <vector>
#include <queue>
// #include <fcntl.h>
// #include <unistd.h>
// #include <sys/socket.h>
// #include <sys/un.h>
// #include <arpa/inet.h>
#include <iostream>
#include "socketlib.h"
#include <stdbool.h>
#include <stdint.h>
#include <cstring>
#include <algorithm>
#include <cassert>

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int server_socket, new_socket;
mmio_req_t *mreq = (mmio_req_t *) (0x90000000ULL);
mmio_rsp_t *mrsp = (mmio_rsp_t *) (0x91000000ULL);

/* void init_server(const uint32_t port) {
  struct sockaddr_in server_addr;
  struct sockaddr_storage server_storage;
  socklen_t addr_size;
  server_socket = socket(PF_INET, SOCK_STREAM, 0);

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  server_addr.sin_addr.s_addr = INADDR_ANY;
  memset(server_addr.sin_zero, '\0', sizeof server_addr.sin_zero);
  bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr));

  // Listen for incoming connections
  if (listen(server_socket, 5) == 0) {
    std::cout << "Listening on port " << port << std::endl;
    fcntl(new_socket, F_SETFL, O_NONBLOCK);
  } else {
    std::cerr << "Error in listening." << std::endl;
    exit(1);
  }

  addr_size = sizeof server_storage;
  fcntl(server_socket, F_SETFL, O_NONBLOCK);

  while ((new_socket = accept(server_socket, (struct sockaddr *) &server_storage, &addr_size)) == -1) {}
  std::cout << "Connection accepted" << std::endl;
  // TODO: re-accept once client dropped
}

void init_server_file(const char *socket_path) {
  struct sockaddr_un addr;
  server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_socket == -1) {
      perror("socket");
      exit(EXIT_FAILURE);
  }
  unlink(socket_path);
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
  if (bind(server_socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
      perror("bind");
      close(server_socket);
      exit(EXIT_FAILURE);
  }
  if (listen(server_socket, 5) == -1) {
      perror("listen");
      close(server_socket);
      exit(EXIT_FAILURE);
  }
  std::cout << "Listening on file " << socket_path << std::endl;
  // fcntl(server_socket, F_SETFL, O_NONBLOCK);

  while ((new_socket = accept(server_socket, NULL, NULL)) == -1) {
      perror("accept");
      close(server_socket);
      exit(EXIT_FAILURE);
  }
  uint8_t endpoint_req;
  recv(new_socket, &endpoint_req, 1, 0);
  // fcntl(new_socket, F_SETFL, O_NONBLOCK);
  std::cout << "Connection accepted" << std::endl;
}


void init_client(const uint32_t port) {
  init_client(port, NOSERV);
}

void init_client(const uint32_t port, const endpoint_id_t endpoint_id) {
  struct sockaddr_in server_addr;
  new_socket = socket(PF_INET, SOCK_STREAM, 0);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

  if (connect(new_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) == 0) {
    std::cout << "Connected to port " << port << std::endl;
    if ((endpoint_id != NOSERV) && (send(new_socket, &endpoint_id, sizeof(endpoint_id_t), 0) < 0)) {
      close(new_socket);
      std::cerr << "Failed to register endpoint ID." << std::endl;
      exit(1);
    }
    // fcntl(new_socket, F_SETFL, O_NONBLOCK);
  } else {
    std::cerr << "Failed to connect." << std::endl;
    exit(1);
  }
}
*/

static uint64_t mmio_call(const uint64_t fid, const uint64_t a1, const uint64_t a2,
    const uint64_t a3, const uint64_t a4, const char* payload, const size_t len) {
  assert(!mreq->valid && !mrsp->valid);
  mreq->func_id = fid;
  mreq->a1 = a1;
  mreq->a2 = a2;
  mreq->a3 = a3;
  mreq->a4 = a4;
  if (payload != NULL) {
    std::memcpy(&(mreq->payload), payload, len);
  }
  mreq->valid = true;
  while (!mrsp->valid);
  uint64_t retval = mrsp->ret;
  mrsp->valid = false;
  return retval;
}

void init_client_file(const char *socket_path) {
  init_client_file(socket_path, NOSERV);
}

void init_client_file(const char *socket_path, const endpoint_id_t endpoint_id) {
  mmio_call(M_CLIENT_FILE, (uint64_t) socket_path, (uint64_t) endpoint_id, 0, 0, NULL, 0);
  /* struct sockaddr_un addr;
  new_socket = socket(AF_UNIX, SOCK_STREAM, 0);
  if (new_socket == -1) {
    std::cerr << "Failed to connect." << std::endl;
    exit(1);
  }
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
  if (connect(new_socket, (struct sockaddr *) &addr, sizeof(addr)) == 0) {
    std::cout << "Connected to file " << socket_path << std::endl;
    if ((endpoint_id != NOSERV) && (send(new_socket, &endpoint_id, sizeof(endpoint_id_t), 0) < 0)) {
      close(new_socket);
      std::cerr << "Failed to register endpoint ID." << std::endl;
      exit(1);
    }
    // fcntl(new_socket, F_SETFL, O_NONBLOCK);
  } else {
    std::cerr << "Failed to connect." << std::endl;
    exit(1);
  } */
}

inline size_t recv_mmio(const int sockfd, const void* buf, const int len, const int flags) {
  return mmio_call(M_RECV, sockfd, (uint64_t) buf, len, flags, NULL, 0);
}

inline size_t send_mmio(const int sockfd, const void* buf, const int len, const int flags) {
  return mmio_call(M_SEND, sockfd, (uint64_t) buf, len, flags, NULL, 0);
}

std::deque<message_packet_t> received_packets;
void fetch_packets() {
  message_header_t header;
  while (recv_mmio(new_socket, &header, sizeof(header), _MSG_PEEK | _MSG_DONTWAIT) > 0) { // more data to come
#ifdef SOCKETLIB_VERBOSE
    std::cout << "DEBUG: new data is arriving" << std::endl;
#endif
    // fcntl(new_socket, F_SETFL, 0);
    ssize_t header_bytes_received = recv_mmio(new_socket, &header, sizeof(message_header_t), 0);
    if (header_bytes_received != sizeof(message_header_t)) {
      // TODO: maybe put this in a loop to ensure we receive the header
      std::cerr << "Error receiving header: " << errno << std::endl;
    }
    char *message_data = (char *) malloc(header.size - sizeof(message_header_t));
    if (message_data == nullptr) {
      std::cerr << "Error allocating memory for message data" << std::endl;
      exit(-1);
    }
#ifdef SOCKETLIB_VERBOSE
    std::cout << "DEBUG: start receiving message with size " << header.size << std::endl;
#endif
    size_t bytes_received = 0;
    while (bytes_received < header.size - sizeof(message_header_t)) {
        size_t chunk_size = MIN(1024ul, header.size - sizeof(message_header_t) - bytes_received);
        ssize_t chunk_bytes_received = recv_mmio(new_socket, message_data + bytes_received, chunk_size, 0);
        if (chunk_bytes_received == -1) {
          std::cerr << "Error receiving message data: " << errno << " " << strerror(errno) << std::endl;
          exit(-1);
        }
        bytes_received += chunk_bytes_received;
    }
    // fcntl(new_socket, F_SETFL, O_NONBLOCK);
#ifdef SOCKETLIB_VERBOSE
    std::cout << "DEBUG: added packet with func id " << header.func_id << std::endl;
#endif
    message_packet_t new_packet;
    new_packet.header = header;
    new_packet.payload = message_data;
    received_packets.push_back(new_packet);
  }
}

int socket_receive(const func_id_t func_id, const bool blocking, std::vector<char> &dest_buf) {
  // check for qualifying messages
  // if not found, fetch from socket
  // if blocking, fetch & check in a loop
  bool found = false;
  endpoint_id_t src_id = 0;
  dest_buf.clear();
  if (blocking) {
#ifdef SOCKETLIB_VERBOSE
    std::cout << "DEBUG: trying to find packet with id " << func_id << std::endl;
#endif
    do {
      fetch_packets();
      for (auto it = received_packets.begin(); it != received_packets.end(); ++it) {
#ifdef SOCKETLIB_VERBOSE
      if (blocking) std::cout << "DEBUG: examining packet with id " << it->header.func_id << std::endl;
#endif
        if (it->header.func_id == func_id) {
           found = true;
           src_id = it->header.src_id;
           dest_buf.insert(dest_buf.end(), it->payload, it->payload + it->header.size - sizeof(message_header_t));
           free(it->payload);
           received_packets.erase(it);
           break;
        }
      }
    } while (blocking && !found);
  } else {
    if (received_packets.size() == 0) fetch_packets();
    if (received_packets.size() > 0) {
      auto it = received_packets.begin();
      if (it->header.func_id == func_id) {
         src_id = it->header.src_id;
         dest_buf.insert(dest_buf.end(), it->payload, it->payload + it->header.size - sizeof(message_header_t));
         free(it->payload);
         received_packets.pop_front();
      }
    }
  }
  return (int) src_id;
}

int socket_send(const endpoint_id_t endpoint_id, const func_id_t func_id, const std::vector<char> &args, const std::vector<char> &payload) {
  message_header_t header;
  header.size = sizeof(message_header_t) + args.size() + payload.size();
  header.src_id = NOSERV; // will be overriden by server
  header.dst_id = endpoint_id;
  header.func_id = func_id;

  // fcntl(new_socket, F_SETFL, 0);
  if ((size_t) send_mmio(new_socket, &header, sizeof(header), 0) != sizeof(header)) {
    std::cout << "DEBUG: failed to send header" << std::endl;
    return -1;
  }

  if ((size_t) send_mmio(new_socket, args.data(), args.size(), 0) != args.size()) {
    std::cout << "DEBUG: failed to send args" << std::endl;
    return -1;
  }

  for (size_t i = 0; i < payload.size();) {
    size_t chunk_size = MIN(1024ul, payload.size() - i);
    const char* data_ptr = payload.data() + i;
    ssize_t bytes_sent = send_mmio(new_socket, data_ptr, chunk_size, 0);
#ifdef SOCKETLIB_VERBOSE
    std::cout << "DEBUG: sent " << bytes_sent << " bytes" << std::endl;
#endif
    i += bytes_sent;
    if (bytes_sent == -1) {
      std::cout << "DEBUG: failed to send everything" << std::endl;
      return -1;
    }
  }
  // fcntl(new_socket, F_SETFL, O_NONBLOCK);
  return 0;
}
