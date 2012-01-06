// Socket setup and file descriptor transfer taken from
// http://www.thomasstover.com/uds.html

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <private/ui/sw_gralloc_handle.h>
#include <vector>
#include <cassert>
#include "common.h"

using namespace android;
using namespace std;

void handle_connection(int sock) {
  message msg = recv_message(sock);

  msg = recv_message(sock);
  if(msg.type == "new-surface") {
    int args_read;
    sp<GraphicBuffer> gbuf = message_to_graphic_buffer(msg, 0, /* out */ args_read);
    // int* raw_surface = NULL;
    // assert(sw_gralloc_handle_t::validate(gbuf->handle) == 0);
    printf("we have a %s buffer\n",
           sw_gralloc_handle_t::validate(gbuf->handle) == 0 ? "software" : "hardware");
    // int rc = sw_gralloc_handle_t::registerBuffer((sw_gralloc_handle_t*)gbuf->handle);
    int rc = GraphicBufferMapper::get().registerBuffer(gbuf->handle);
    assert(rc == NO_ERROR);
    unsigned char** raw_surface = NULL;
    rc = gbuf->lock(GraphicBuffer::USAGE_SW_READ_RARELY, (void**)&raw_surface);
    if(rc != NO_ERROR)
      printf("lock rc = %s\n", strerror(-rc));
    assert(rc == NO_ERROR);
    assert(raw_surface != NULL);
    printf("raw_surface = %08p\n", raw_surface);
    // sleep(10);
    printf("first pixel = %d\n", raw_surface[0]);
    rc = gbuf->unlock();
    assert(rc == NO_ERROR);
    // rc = sw_gralloc_handle_t::unregisterBuffer((sw_gralloc_handle_t*)gbuf->handle);
    rc = GraphicBufferMapper::get().unregisterBuffer(gbuf->handle);
    assert(rc == NO_ERROR);
  }
  else if(msg.type == "file-test") {
    assert(msg.fds.size() == 1);
    string text = "writing from the host\n";
    write(msg.fds[0], text.c_str(), text.length());
    close(msg.fds[0]);
  }

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
