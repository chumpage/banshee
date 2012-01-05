#ifndef common_h
#define common_h

#include <vector>
#include <string>
#include <ui/GraphicBuffer.h>

const bool g_stream = true;
const std::string g_socket_path = "ipc_socket";
const bool g_print_ipc = true;

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

message recv_message(int socket);
void send_message(int socket, const message& msg);

#endif
