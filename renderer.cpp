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
  //printf("raw_surface = %08p\n", raw_surface);
  for(int i = 0; i < gbuf.width*gbuf.height; i++)
    raw_surface[i] = color;
  check_android(gbuf.unlock());
}

struct fbo_state {
  GLuint fb;
  GLuint depth_rb;

  fbo_state() : fb((GLuint)-1), depth_rb((GLuint)-1) { }
  fbo_state(GLuint fb_, GLuint depth_rb_)
    : fb(fb_), depth_rb(depth_rb_) {
  }
};

struct renderer_state {
  gl_state gl;
  sp<gralloc_buffer> front_buf, back_buf;
  fbo_state fbo;

  renderer_state() { }
  
  renderer_state(const gl_state& gl_,
                 const sp<gralloc_buffer>& front_buf_,
                 const sp<gralloc_buffer>& back_buf_,
                 const fbo_state& fbo_)
    : gl(gl_),
      front_buf(front_buf_),
      back_buf(back_buf_),
      fbo(fbo_) {
  }

  bool valid() { return gl.valid(); }
};

fbo_state init_fbo(int width, int height) {
  GLuint fb;
  glGenFramebuffers(1, &fb);
  check_gl();

  GLuint depth_rb;
  glGenRenderbuffers(1, &depth_rb);
  glBindRenderbuffer(GL_RENDERBUFFER, depth_rb);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
  check_gl();
}

void term_fbo(fbo_state& fbo) {
  glDeleteRenderbuffers(1, &fbo.depth_rb);
  glDeleteFramebuffers(1, &fbo.fb);
  check_gl();
  fbo = fbo_state();
}

renderer_state init_renderer(const int surface_width,
                             const int surface_height) {
  gl_state gl = init_gl(NULL, 1, 1);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  check_gl();

  glViewport(0, 0, surface_width, surface_height);

  const bool hardware_surface = true;
  int gbuf_usage = GraphicBuffer::USAGE_SW_WRITE_OFTEN | GraphicBuffer::USAGE_SW_READ_OFTEN;
  if(hardware_surface)
    gbuf_usage |= GraphicBuffer::USAGE_HW_TEXTURE;

  sp<GraphicBuffer> front_graphic_buf = new GraphicBuffer(
    surface_width, surface_height, PIXEL_FORMAT_RGBA_8888, gbuf_usage);
  check_android(front_graphic_buf->initCheck());
  set_graphic_buffer_solid_color(*front_graphic_buf, 255, 0, 0);

  sp<GraphicBuffer> back_graphic_buf = new GraphicBuffer(
    surface_width, surface_height, PIXEL_FORMAT_RGBA_8888, gbuf_usage);
  check_android(back_graphic_buf->initCheck());
  set_graphic_buffer_solid_color(*back_graphic_buf, 0, 0, 255);

  sp<gralloc_buffer> front_buf = new gralloc_buffer(front_graphic_buf);
  sp<gralloc_buffer> back_buf = new gralloc_buffer(back_graphic_buf);

  fbo_state fbo = init_fbo(surface_width, surface_height);

  return renderer_state(gl, front_buf, back_buf, fbo);
}

void term_renderer(renderer_state& renderer) {
  if(!renderer.valid())
    return;

  term_fbo(renderer.fbo);
  renderer.front_buf.clear();
  renderer.back_buf.clear();
  term_gl(renderer.gl);
}

void render_frame(renderer_state& renderer) {
}

void run_renderer(int sock) {
  renderer_state renderer;

  while(1) {
    unix_socket_address host_addr;
    message msg = recv_message(sock, &host_addr);

    if(msg.type == "connect" || msg.type == "terminate") {
      term_renderer(renderer);
    }
    else if(msg.type == "request-surfaces") {
      int width, height;
      unpack_request_surfaces_message(msg, &width, &height);
      renderer = init_renderer(width, height);

      message msg = form_surfaces_message(
        *renderer.front_buf->gbuf, *renderer.back_buf->gbuf);
      send_message(sock, msg, host_addr);
    }
    else if(msg.type == "render-frame") {
      check(renderer.valid());
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
