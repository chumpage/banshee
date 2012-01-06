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
  unix_socket_address host_addr(g_host_socket_path);
  send_message(sock, form_connect_message(), host_addr);

  FILE* file = NULL;
  bool send_surface = true;

  if(send_surface) {
    GraphicBuffer gbuf(1024, 1024, PIXEL_FORMAT_RGB_565,
    GraphicBuffer::USAGE_SW_WRITE_OFTEN |
    GraphicBuffer::USAGE_SW_READ_OFTEN
      | GraphicBuffer::USAGE_HW_TEXTURE
    );
    int* raw_surface = NULL;
    int rc = gbuf.lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN, (void**)&raw_surface);
    assert(rc == NO_ERROR);
    assert(raw_surface != NULL);
    printf("raw_surface = %08p\n", raw_surface);
    // sleep(10);
    raw_surface[0] = 123456;
    rc = gbuf.unlock();
    assert(rc == NO_ERROR);

    message new_surface_msg("new-surface");
    graphic_buffer_to_message(gbuf, new_surface_msg);
    send_message(sock, new_surface_msg, host_addr);
  }
  else {
    file = fopen("test.txt", "w");
    assert(file);
    message file_msg("file-test");
    string text = "writing from the renderer\n";
    write(fileno(file), text.c_str(), text.length());
    file_msg.fds.push_back(fileno(file));
    send_message(sock, file_msg, host_addr);
    // fclose(file);
  }

  if(file)
    fclose(file);
  message msg = recv_message(sock);
}

bool run_renderer() {
  int sock = socket(PF_UNIX, SOCK_DGRAM, 0);
  assert(sock >= 0);

  unlink(g_renderer_socket_path.c_str());

  unix_socket_address addr(g_renderer_socket_path);
  int rc = bind(sock, addr.sock_addr(), addr.len());
  assert(rc == 0);

  handle_connection(sock);
  close(sock);
  unlink(g_renderer_socket_path.c_str());
  return true;
}


int main() {
  return run_renderer() ? 0 : 1;
}
