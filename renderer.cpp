#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <cassert>
#include "common.h"

using namespace std;
using namespace android;

void handle_connection(int sock) {
  send_message(sock, form_connect_message());
  GraphicBuffer gbuf(1024, 1024, PIXEL_FORMAT_RGB_565,
    GraphicBuffer::USAGE_SW_WRITE_OFTEN | GraphicBuffer::USAGE_HW_TEXTURE);
  message new_surface_msg("new-surface");
  graphic_buffer_to_message(gbuf, new_surface_msg);
  send_message(sock, new_surface_msg);
  message msg = recv_message(sock);
}

bool run_renderer() {
  int rc = 0;
  int sock = socket(PF_UNIX, g_stream ? SOCK_STREAM : SOCK_DGRAM, 0);
  assert(sock >= 0);

  sockaddr_un addr;
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  snprintf(addr.sun_path, UNIX_PATH_MAX, g_socket_path.c_str());

  if(g_stream) {
    rc = connect(sock, (sockaddr*)&addr, sizeof(addr));
    assert(rc == 0);
  }

  handle_connection(sock);
  close(sock);
  return true;
}


int main() {
  return run_renderer() ? 0 : 1;
}
