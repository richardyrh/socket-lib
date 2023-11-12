#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <vector>
#include <cstring>

typedef uint32_t msg_size_t;
typedef uint8_t endpoint_id_t;
typedef uint16_t func_id_t;

#define NOSERV 255

void init_server(const uint32_t port);
void init_server_file(const char *socket_path);
void init_client(const uint32_t port);
void init_client(const uint32_t port, const uint8_t endpoint_id);
void init_client_file(const char *socket_path);
void init_client_file(const char *socket_path, const uint8_t endpoint_id);

int socket_receive(const func_id_t func_id, const bool blocking, std::vector<char> &dest_buf);

int socket_send(const endpoint_id_t endpoint_id, const func_id_t func_id, const std::vector<char> &args, const std::vector<char> &payload);

typedef struct message_header {
  msg_size_t size;
  endpoint_id_t src_id;
  endpoint_id_t dst_id;
  func_id_t func_id;
} message_header_t;

typedef struct message_packet {
  message_header_t header;
  char *payload;
} message_packet_t;

template <typename T>
std::vector<char> serialize_args(const T &args) {
  const char* data = reinterpret_cast<const char*>(&args);
  std::vector<char> result(data, data + sizeof(T));
  return result;
}

template <typename TT>
void deserialize_args(const std::vector<char> &source_buf, TT &result) {
  memcpy(&result, source_buf.data(), sizeof(TT));
}

static const std::vector<char> empty_vec;
