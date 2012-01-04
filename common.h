#ifndef common_h
#define common_h

#include <vector>
#include <string>
#include <ui/GraphicBuffer.h>

const bool g_stream = true;
const std::string g_socket_path = "ipc_socket";

struct message {
  message();
  message(const std::string& type,
          const std::vector<std::string>& args = std::vector<std::string>());

  std::string type;
  std::vector<std::string> args;
};

message parse_message(const std::string& raw_msg);
std::string serialize_message(const message& msg);

message form_connect_message();
message form_terminate_message();
// message form_new_surface_msg();

message recv_message(int socket);
void send_message(int socket, const message& msg);

#endif
