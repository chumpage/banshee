#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <cassert>
#include "common.h"

using namespace std;
using namespace android;

void run_renderer(int sock) {
  unix_socket_address host_addr(g_host_socket_path);
  send_message(sock, form_connect_message(), host_addr);

  bool send_surface = true;
  bool hardware_surface = true;

  if(send_surface) {
    int gbuf_usage = GraphicBuffer::USAGE_SW_WRITE_OFTEN | GraphicBuffer::USAGE_SW_READ_OFTEN;
    if(hardware_surface)
      gbuf_usage |= GraphicBuffer::USAGE_HW_TEXTURE;
    GraphicBuffer gbuf(1024, 1024, PIXEL_FORMAT_RGB_565, gbuf_usage);
    unsigned short* raw_surface = NULL;
    check_android(gbuf.lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN, (void**)&raw_surface));
    printf("raw_surface = %08p\n", raw_surface);
    raw_surface[0] = 12345;
    check_android(gbuf.unlock());

    message new_surface_msg("new-surface");
    graphic_buffer_to_message(gbuf, new_surface_msg);
    send_message(sock, new_surface_msg, host_addr);
  }
  else {
    FILE* file = fopen("test.txt", "w");
    assert(file);
    message file_msg("file-test");
    string text = "writing from the renderer\n";
    write(fileno(file), text.c_str(), text.length());
    file_msg.fds.push_back(fileno(file));
    send_message(sock, file_msg, host_addr);
    fclose(file);
  }

  message msg = recv_message(sock);
}

bool setup_and_run() {
  int sock = socket(PF_UNIX, SOCK_DGRAM, 0);
  check_unix(sock);

  unlink(g_renderer_socket_path.c_str());

  unix_socket_address addr(g_renderer_socket_path);
  check_unix(bind(sock, addr.sock_addr(), addr.len()));

  run_renderer(sock);
  check_unix(close(sock));
  unlink(g_renderer_socket_path.c_str());
  return true;
}


int main() {
  return setup_and_run() ? 0 : 1;
}
