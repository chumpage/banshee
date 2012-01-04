#include "common.h"
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
#include <vector>
#include <string>
#include <cassert>

using namespace std;

namespace {
vector<string> split(const string& str, char delim = ' ') {
  vector<string> tokens;
  stringstream ss(str);
  string item;
  while(getline(ss, item, delim)) {
    tokens.push_back(item);
  }
  return tokens;
}
}

message::message() {
}

message::message(const string& type_, const vector<string>& args_)
  : type(type_), args(args_) {
}

message parse_message(const string& raw_msg) {
  vector<string> tokens = split(raw_msg);
  assert(!tokens.empty());
  message msg;
  return message(tokens[0], vector<string>(tokens.begin()+1, tokens.end()));
}

string serialize_message(const message& msg) {
  ostringstream ss;
  ss << msg.type;
  for(int i = 0; i < msg.args.size(); i++)
    ss << ' ' << msg.args[i];
  return ss.str();
}

message form_connect_message() {
  return message("connect");
}

message form_terminate_message() {
  return message("terminate");
}

message recv_message(int socket) {
  const int buffer_size = 1024;
  char buffer[buffer_size];
  int msg_size = read(socket, buffer, buffer_size-1);
  buffer[msg_size] = '\0';
  return parse_message(buffer);
}

void send_message(int socket, const message& msg) {
  string serialized_msg = serialize_message(msg);
  write(socket, serialized_msg.c_str(), serialized_msg.length());
}
