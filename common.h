#ifndef common_h
#define common_h

#include <vector>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <ui/GraphicBuffer.h>

const std::string g_host_socket_path = "ipc_host";
const std::string g_renderer_socket_path = "ipc_renderer";
const bool g_print_ipc = true;

struct unix_socket_address {
  unix_socket_address();
  unix_socket_address(const std::string& path);
  unix_socket_address(const unix_socket_address& addr);

  const unix_socket_address& operator=(const unix_socket_address& addr);

  sockaddr* sock_addr() const;
  sockaddr_un* sock_addr_un() const;
  socklen_t len() const;

  sockaddr_un addr;
};

struct message {
  message();
  message(const std::string& type,
          const std::vector<std::string>& args = std::vector<std::string>());

  std::string type;
  std::vector<std::string> args;
  std::vector<int> fds;
};

message parse_message(const std::string& raw_msg);
std::string serialize_message(const message& msg);

message form_connect_message();
message form_terminate_message();

void graphic_buffer_to_message(const android::GraphicBuffer& gb, message& msg);
android::sp<android::GraphicBuffer> message_to_graphic_buffer(
  const message& msg,
  int arg_offset,
  int& args_read);

message recv_message(int socket, unix_socket_address* from_addr = NULL);
void send_message(int socket,
                  const message& msg,
                  const unix_socket_address& to_addr);

#endif
