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

// Communication works like this
//   1. renderer -> host: connect
//   2. renderer -> host: new-surface or file-test
//   3. host -> renderer: terminate
void run_host(int sock) {
  unix_socket_address renderer_addr;
  message msg = recv_message(sock, &renderer_addr);

  msg = recv_message(sock);
  if(msg.type == "new-surface") {
    sp<GraphicBuffer> gbuf = message_to_graphic_buffer(msg, 0);
    printf("we have a %s buffer\n",
           sw_gralloc_handle_t::validate(gbuf->handle) == 0 ? "software" : "hardware");
    check_android(GraphicBufferMapper::get().registerBuffer(gbuf->handle));
    unsigned short** raw_surface = NULL;
    check_android(gbuf->lock(GraphicBuffer::USAGE_SW_READ_RARELY,
                             (void**)&raw_surface));
    printf("raw_surface = %08p\n", raw_surface);
    printf("first pixel = %u\n", raw_surface[0]);
    check_android(gbuf->unlock());
    check_android(GraphicBufferMapper::get().unregisterBuffer(gbuf->handle));
  }
  else if(msg.type == "file-test") {
    assert(msg.fds.size() == 1);
    string text = "writing from the host\n";
    write(msg.fds[0], text.c_str(), text.length());
    check_unix(close(msg.fds[0]));
  }

  send_message(sock, form_terminate_message(), renderer_addr);
}

bool setup_and_run() {
  unlink(g_host_socket_path.c_str());

  int sock = socket(PF_UNIX, SOCK_DGRAM, 0);
  check_unix(sock);

  unix_socket_address addr(g_host_socket_path.c_str());
  check_unix(bind(sock, addr.sock_addr(), addr.len()));

  run_host(sock);

  check_unix(close(sock));
  unlink(g_host_socket_path.c_str());
  return true;
}

int main() {
  return setup_and_run() ? 0 : 1;
}
