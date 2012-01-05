// Socket setup and file descriptor transfer taken from
// http://www.thomasstover.com/uds.html

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ui/GraphicBuffer.h>
#include <vector>
#include <cassert>
#include "common.h"

using namespace android;
using namespace std;

void handle_connection(int sock) {
  message msg = recv_message(sock);
  msg = recv_message(sock);
  send_message(sock, form_terminate_message());
}

// ssize_t recvfrom(int sock, void *buffer, size_t length, int flags,
//              struct sockaddr *address, socklen_t *address_len);

int recv_msg(int sock_fd) {
  int nbytes;
  char buffer[256];

  // sleep(2);
  sockaddr_un from_addr;
  socklen_t from_addr_len = sizeof(from_addr);
  nbytes = recvfrom(sock_fd, buffer, 256, 0, (sockaddr*)&from_addr, &from_addr_len);
  // nbytes = read(sock_fd, buffer, 256);
  buffer[nbytes] = 0;

  printf("MESSAGE FROM CLIENT: %s\n", buffer);
  nbytes = snprintf(buffer, 256, "hello from the server");
  sendto(sock_fd, buffer, nbytes, 0, (sockaddr*)&from_addr, sizeof(from_addr));
  // write(sock_fd, buffer, nbytes);

  // sleep(2);
 
  // close(connection_fd);
  return 0;
}

bool run_host() {
  int listen_socket = socket(PF_UNIX, g_stream ? SOCK_STREAM : SOCK_DGRAM, 0);
  assert(listen_socket >= 0);

  unlink(g_socket_path.c_str());

  sockaddr_un sock_addr;
  memset(&sock_addr, 0, sizeof(sock_addr));
  sock_addr.sun_family = AF_UNIX;
  snprintf(sock_addr.sun_path, UNIX_PATH_MAX, g_socket_path.c_str());

  int rc = bind(listen_socket, (sockaddr*)&sock_addr, sizeof(sock_addr));
  assert(rc == 0);

  if(g_stream) {
    rc = listen(listen_socket, 2);
    assert(rc == 0);

    // while(1) {
      sockaddr_un renderer_addr;
      memset(&renderer_addr, 0, sizeof(renderer_addr));
      int addr_len;
      int connection_socket = accept(listen_socket, (sockaddr*)&renderer_addr, &addr_len);
      assert(connection_socket >= 0);
      handle_connection(connection_socket);
      close(connection_socket);
    // }
  }
  else {
    while(1) 
      recv_msg(listen_socket);
  }

  close(listen_socket);
  unlink(g_socket_path.c_str());
  return true;
}

int main() {
  // vector<int> vals(10, 0);
  // printf("Creating GraphicBuffer\n");
  // GraphicBuffer buffer(1024, 1024, PIXEL_FORMAT_RGB_565, GraphicBuffer::USAGE_HW_TEXTURE);
  // printf("Done creating GraphicBuffer\n");
  return run_host() ? 0 : 1;
  // return 0;
}
