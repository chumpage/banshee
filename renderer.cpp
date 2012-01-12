#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <cassert>
#include "common.h"

using namespace std;
using namespace android;

void set_graphic_buffer_solid_color(GraphicBuffer& gbuf, int red, int green, int blue) {
  unsigned int color = (red << 24) | (green << 16) | (blue << 8);
  unsigned int* raw_surface = NULL;
  check_android(gbuf.lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN, (void**)&raw_surface));
  printf("raw_surface = %08p\n", raw_surface);
  for(int i = 0; i < gbuf.width*gbuf.height; i++)
    raw_surface[i] = color;
  check_android(gbuf.unlock());
}

void run_renderer(int sock) {
  sp<GraphicBuffer> front_gbuf, back_gbuf;

  while(1) {
    unix_socket_address host_addr;
    message msg = recv_message(sock, &host_addr);

    if(msg.type == "connect" || msg.type == "terminate") {
      front_gbuf.clear();
      back_gbuf.clear();
    }
    else if(msg.type == "request-surfaces") {
      front_gbuf.clear();
      back_gbuf.clear();

      const bool hardware_surface = true;
      int width, height;
      unpack_request_surfaces_message(msg, &width, &height);

      int gbuf_usage = GraphicBuffer::USAGE_SW_WRITE_OFTEN | GraphicBuffer::USAGE_SW_READ_OFTEN;
      if(hardware_surface)
        gbuf_usage |= GraphicBuffer::USAGE_HW_TEXTURE;

      front_gbuf = new GraphicBuffer(width, height, PIXEL_FORMAT_RGBA_8888, gbuf_usage);
      check_android(front_gbuf->initCheck());
      set_graphic_buffer_solid_color(*front_gbuf, 255, 0, 0);

      back_gbuf = new GraphicBuffer(width, height, PIXEL_FORMAT_RGBA_8888, gbuf_usage);
      check_android(back_gbuf->initCheck());
      set_graphic_buffer_solid_color(*back_gbuf, 0, 0, 255);

      send_message(sock, form_surfaces_message(*front_gbuf, *back_gbuf), host_addr);
    }
    else if(msg.type == "render-frame") {
      check(front_gbuf.get() && back_gbuf.get());
      send_message(sock, message("frame-finished"), host_addr);
    }
    else if(msg.type == "file-test") {
      FILE* file = fopen("test.txt", "w");
      assert(file);
      message file_msg("file-test");
      string text = "writing from the renderer\n";
      write(fileno(file), text.c_str(), text.length());
      file_msg.fds.push_back(fileno(file));
      send_message(sock, file_msg, host_addr);
      fclose(file);
    }
  }
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
