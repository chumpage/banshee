#include "common.h"
#include <sys/socket.h>
#include <unistd.h>
#include <sstream>
#include <vector>
#include <string>
#include <cassert>

using namespace std;
using namespace android;

namespace {

template<typename T>
T parse_str(const string& str) {
  T val;
  istringstream ss(str);
  ss >> val;
  return val;
}

template<typename T>
string to_str(const T& val) {
  ostringstream ss;
  ss << val;
  return ss.str();
}

vector<string> split(const string& str, char delim = ' ') {
  vector<string> tokens;
  stringstream ss(str);
  string item;
  while(getline(ss, item, delim)) {
    tokens.push_back(item);
  }
  return tokens;
}

vector<string> serialize_ints(const vector<int>& vals) {
  vector<string> str_vals;
  for(int i = 0; i < vals.size(); i++)
    str_vals.push_back(to_str(vals[i]));
  return str_vals;
}

vector<int> parse_ints(const vector<string>& args) {
  vector<int> vals;
  for(int i = 0; i < args.size(); i++)
    vals.push_back(parse_str<int>(args[i]));
  return vals;
}

string debug_print_message(const message& msg, const string& header) {
  ostringstream ss;
  ss << header << " " << msg.type;
  if(!msg.args.empty()) {
    ss << ", args(" << msg.args.size() << ") =";
    for(int i = 0; i < msg.args.size(); i++)
      ss << " " << msg.args[i];
  }
  if(!msg.fds.empty()) {
    ss << ", fds(" << msg.fds.size() << ") =";
    for(int i = 0; i < msg.fds.size(); i++)
      ss << " " << msg.fds[i];
  }
  return ss.str();
}

} // namespace {


unix_socket_address::unix_socket_address() {
  memset(&addr, 0, sizeof(addr));
}

unix_socket_address::unix_socket_address(const string& path) {
  assert(path.length()+1 <= UNIX_PATH_MAX);
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  int chars_written = snprintf(addr.sun_path, UNIX_PATH_MAX, path.c_str());
  assert(chars_written >= 0 && chars_written < UNIX_PATH_MAX);
}

unix_socket_address::unix_socket_address(const unix_socket_address& addr_) {
  *this = addr_;
}

const unix_socket_address& unix_socket_address::operator=(
  const unix_socket_address& addr_) {
  if(this != &addr_)
    memcpy(&addr, &addr_.addr, sizeof(addr));
  return *this;
}

sockaddr* unix_socket_address::sock_addr() const {
  return (sockaddr*)&addr;
}

sockaddr_un* unix_socket_address::sock_addr_un() const {
  return const_cast<sockaddr_un*>(&addr);
}

socklen_t unix_socket_address::len() const {
  return sizeof(addr);
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

message form_request_surfaces_message(int width, int height) {
  message msg("request-surfaces");
  msg.args.push_back(to_str(width));
  msg.args.push_back(to_str(height));
  return msg;
}

void unpack_request_surfaces_message(const message& msg, int* width, int* height) {
  assert(width && height);
  assert(msg.type == "request-surfaces");
  assert(msg.args.size() == 2);
  *width = parse_str<int>(msg.args[0]);
  *height = parse_str<int>(msg.args[1]);
}

namespace {

void graphic_buffer_to_message(const GraphicBuffer& gb, message& msg) {
  assert(gb.getFlattenedSize()%sizeof(int) == 0);
  vector<int> buffer(gb.getFlattenedSize()/sizeof(int));
  vector<int> fds(gb.getFdCount());
  check_android(gb.flatten(&buffer[0], buffer.size()*sizeof(int),
                           &fds[0], fds.size()));
  vector<string> vals = serialize_ints(buffer);
  msg.args.push_back(to_str(fds.size()));
  msg.args.push_back(to_str(vals.size()));
  msg.args.insert(msg.args.end(), vals.begin(), vals.end());
  msg.fds.insert(msg.fds.end(), fds.begin(), fds.end());
}

sp<GraphicBuffer> message_to_graphic_buffer(const message& msg,
                                            int& arg_offset,
                                            int& fd_offset)
{
  assert(arg_offset < msg.args.size());
  int num_fds = parse_str<int>(msg.args[arg_offset]);
  arg_offset++;
  int num_ints = parse_str<int>(msg.args[arg_offset]);
  arg_offset++;
  assert(fd_offset+num_fds <= msg.fds.size());
  assert(arg_offset+num_ints <= msg.args.size());
  vector<int> buffer = parse_ints(
    vector<string>(msg.args.begin()+arg_offset,
                   msg.args.begin()+arg_offset+num_ints));
  arg_offset += num_ints;
  vector<int> fds = vector<int>(msg.fds.begin()+fd_offset,
                                msg.fds.begin()+fd_offset+num_fds);
  fd_offset += num_fds;
  sp<GraphicBuffer> gb = new GraphicBuffer;
  assert(gb != NULL);
  check_android(gb->unflatten(&buffer[0],
                              buffer.size()*sizeof(int),
                              const_cast<int*>(&fds[0]),
                              fds.size()));
  return gb;
}

} // namespace {

message form_surfaces_message(const GraphicBuffer& front_gbuf,
                              const GraphicBuffer& back_gbuf) {
  message msg("surfaces");
  graphic_buffer_to_message(front_gbuf, msg);
  graphic_buffer_to_message(back_gbuf, msg);
  return msg;
}

void unpack_surfaces_message(const message& msg,
                             sp<GraphicBuffer>* front_gbuf,
                             sp<GraphicBuffer>* back_gbuf) {
  assert(front_gbuf && back_gbuf);
  int arg_offset = 0, fd_offset = 0;
  *front_gbuf = message_to_graphic_buffer(msg, arg_offset, fd_offset);
  *back_gbuf = message_to_graphic_buffer(msg, arg_offset, fd_offset);
}

bool is_address_bound(const unix_socket_address& addr) {
  int sock = socket(PF_UNIX, SOCK_DGRAM, 0);
  check_unix(sock);
  int rc = bind(sock, addr.sock_addr(), addr.len());
  close(sock);
  if(rc != 0 && rc != EADDRINUSE) {
    check_unix(rc);
  }
  return rc == 0 ? false : true;
}

message recv_message(int sock, unix_socket_address* from_addr) {
  unix_socket_address from_addr_tmp;
  if(!from_addr)
    from_addr = &from_addr_tmp;

  msghdr socket_message;
  memset(&socket_message, 0, sizeof(socket_message));
  socket_message.msg_name = from_addr->sock_addr();
  socket_message.msg_namelen = from_addr->len();

  const int msg_buffer_size = 1024;
  char msg_buffer[msg_buffer_size];
  iovec io_vec;
  io_vec.iov_base = msg_buffer;
  io_vec.iov_len = msg_buffer_size-1;
  socket_message.msg_iov = &io_vec;
  socket_message.msg_iovlen = 1;

  const int fd_buffer_size = CMSG_SPACE(64*sizeof(int));
  char fd_buffer[fd_buffer_size];
  socket_message.msg_control = fd_buffer;
  socket_message.msg_controllen = fd_buffer_size;

  int flags = 0;
#if defined(MSG_CMSG_CLOEXEC)
  flags |= MSG_CMSG_CLOEXEC;
#endif

  int len = recvmsg(sock, &socket_message, flags);
  check_unix(len);
  msg_buffer[len] = '\0';

  assert((socket_message.msg_flags & MSG_CTRUNC) == 0);

  message out_msg = parse_message(msg_buffer);

  // iterate ancillary elements
  for(cmsghdr* control_message = CMSG_FIRSTHDR(&socket_message);
      control_message != NULL;
      control_message = CMSG_NXTHDR(&socket_message, control_message)) {
    if((control_message->cmsg_level == SOL_SOCKET) &&
       (control_message->cmsg_type == SCM_RIGHTS)) {
      int num_fds = (control_message->cmsg_len - CMSG_LEN(0)) / sizeof(int);
      assert(num_fds <= 1000); // sanity check
      int* fds = (int*)CMSG_DATA(control_message);
      out_msg.fds.insert(out_msg.fds.end(), fds, fds+num_fds);
    }
  }

  if(g_print_ipc)
    logi("%s\n", debug_print_message(out_msg, "recv: ").c_str());

  return out_msg;
 }

void send_message(int sock,
                  const message& msg,
                  const unix_socket_address& to_addr) {
  string serialized_msg = serialize_message(msg);
  iovec io_vec;
  io_vec.iov_base = &serialized_msg[0];
  io_vec.iov_len = serialized_msg.size();

  // Initialize socket message
  msghdr socket_message;
  memset(&socket_message, 0, sizeof(socket_message));
  socket_message.msg_name = to_addr.sock_addr();
  socket_message.msg_namelen = to_addr.len();
  socket_message.msg_iov = &io_vec;
  socket_message.msg_iovlen = 1;

  // Provide space for the ancillary data
  char* fd_buffer = NULL;
  if(!msg.fds.empty()) {
    int fd_buffer_size = CMSG_SPACE(msg.fds.size()*sizeof(int));
    fd_buffer = new char[fd_buffer_size];
    socket_message.msg_control = fd_buffer;
    socket_message.msg_controllen = fd_buffer_size;

    // Initialize a single ancillary data element for fd passing
    cmsghdr* control_message = CMSG_FIRSTHDR(&socket_message);
    control_message->cmsg_level = SOL_SOCKET;
    control_message->cmsg_type = SCM_RIGHTS;
    control_message->cmsg_len = CMSG_LEN(sizeof(int)*msg.fds.size());
    int* cm_fd_ptr = (int*)CMSG_DATA(control_message);
    memcpy(cm_fd_ptr, &msg.fds[0], msg.fds.size()*sizeof(int));
  }

  if(g_print_ipc)
    logi("%s\n", debug_print_message(msg, "send: ").c_str());

  check_unix(sendmsg(sock, &socket_message, 0));
  delete[] fd_buffer;
}
