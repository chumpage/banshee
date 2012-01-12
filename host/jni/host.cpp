#include <cassert>
#include <jni.h>
#include <errno.h>
#include <unistd.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android_native_app_glue.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <private/ui/sw_gralloc_handle.h>
#include <utils/RefBase.h>
#include "../../common.h"

using namespace std;
using namespace android;

float g_vertices[] = { 0,0,0, 1,1,0, 1,0,0,
                       0,0,0, 0,1,0, 1,1,0 };
float g_tex_coords[] = { 0,0, 1,1, 1,0,
                         0,0, 0,1, 1,1 };

struct renderer_connection {
  int sock;
  sp<gralloc_buffer> front_buffer;
  sp<gralloc_buffer> back_buffer;

  renderer_connection() : sock(-1) { }
  renderer_connection(int sock_,
                      sp<gralloc_buffer> front_buffer_,
                      sp<gralloc_buffer> back_buffer_) {
    sock = sock_;
    front_buffer = front_buffer_;
    back_buffer = back_buffer_;
  }
};

struct shader_state {
  GLuint vertex_shader;
  GLuint fragment_shader;
  GLuint shader_program;
  GLint pos_loc;
  GLint tex_coord_loc;
  GLint mvp_loc;
  GLint texture_loc;

  shader_state()
    : vertex_shader(0),
      fragment_shader(0),
      shader_program(0),
      pos_loc(-1),
      tex_coord_loc(-1),
      mvp_loc(-1),
      texture_loc(-1) {
  }
};

struct app_state {
  android_app* android_app_instance;

  gl_state gl;
  renderer_connection connection;
  shader_state shader;
  bool animating;
  int32_t width;
  int32_t height;

  app_state()
    : android_app_instance(NULL),
      animating(false),
      width(0),
      height(0) {
  }
};

renderer_connection init_renderer_connection(int width, int height) {
  if(!is_address_bound(g_renderer_socket_path)) {
    loge("The renderer doesn't seem to be running. Exiting.\n");
    assert(false);
  }

  unlink(g_host_socket_path.c_str());
  int sock = socket(PF_UNIX, SOCK_DGRAM, 0);
  check_unix(sock);
  unix_socket_address addr(g_host_socket_path.c_str());
  check_unix(bind(sock, addr.sock_addr(), addr.len()));

  unix_socket_address renderer_addr(g_renderer_socket_path);
  send_message(sock, message("connect"), renderer_addr);
  send_message(sock, form_request_surfaces_message(width, height), renderer_addr);
  // send_message(sock, form_request_surfaces_message(1024, 1024), renderer_addr);

  message msg = recv_message(sock);
  assert(msg.type == "surfaces");
  sp<GraphicBuffer> front_gbuf, back_gbuf;
  unpack_surfaces_message(msg, &front_gbuf, &back_gbuf);
  assert(front_gbuf->width == width);
  assert(front_gbuf->height == height);
  assert(back_gbuf->width == width);
  assert(back_gbuf->height == height);

  sp<gralloc_buffer> front_gralloc_buffer = new gralloc_buffer(front_gbuf);
  sp<gralloc_buffer> back_gralloc_buffer = new gralloc_buffer(back_gbuf);

  return renderer_connection(sock, front_gralloc_buffer, back_gralloc_buffer);
}

void term_renderer_connection(renderer_connection& connection) {
  if(connection.sock == -1)
    return;

  connection.front_buffer.clear();
  connection.back_buffer.clear();

  unix_socket_address renderer_addr(g_renderer_socket_path);
  send_message(connection.sock, message("terminate"), renderer_addr);
  
  check_unix(close(connection.sock));
  unlink(g_host_socket_path.c_str());

  connection = renderer_connection();
}

const char* vertex_shader_src = 
"attribute vec3 pos;\n\
attribute vec2 tex_coord;\n\
uniform mat4 mvp;\n\
varying mediump vec2 v_tex_coord;\n\
\n\
void main() {\n\
  gl_Position = mvp*vec4(pos, 1.0);\n\
  v_tex_coord = tex_coord;\n\
}\n";

const char* fragment_shader_src =
"precision mediump float;\n\
varying mediump vec2 v_tex_coord;\n\
uniform sampler2D texture;\n\
\n\
void main() {\n\
  gl_FragColor = vec4(texture2D(texture, v_tex_coord).wzy, 1);\n\
}\n";

GLuint init_shader(const char* src, GLenum type) {
  GLuint shader = glCreateShader(type);
  check(shader != 0);
  glShaderSource(shader, 1, (const GLchar**)&src, NULL);
  check_gl();
  glCompileShader(shader);
  check_gl();
  GLint compiled = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if(compiled == GL_FALSE) {
    GLint info_len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);

    if (info_len > 0) {
      string info_log(info_len, ' ');
      glGetShaderInfoLog(shader, info_len, NULL, &info_log[0]);
      loge("Shader compile error: %s", info_log.c_str());
    }
  }
  check(compiled == GL_TRUE);
  return shader;
}

shader_state init_shader_program() {
  shader_state state;
  state.vertex_shader = init_shader(vertex_shader_src, GL_VERTEX_SHADER);
  state.fragment_shader = init_shader(fragment_shader_src, GL_FRAGMENT_SHADER);
  state.shader_program = glCreateProgram();
  check(state.shader_program != 0);
  glAttachShader(state.shader_program, state.vertex_shader);
  glAttachShader(state.shader_program, state.fragment_shader);
  check_gl();
  glLinkProgram(state.shader_program);
  GLint linked = GL_FALSE;
  glGetProgramiv(state.shader_program, GL_LINK_STATUS, (GLint*)&linked);
  check(linked == GL_TRUE);
  state.pos_loc = glGetAttribLocation(state.shader_program, "pos");
  check(state.pos_loc != -1);
  state.tex_coord_loc = glGetAttribLocation(state.shader_program, "tex_coord");
  check(state.tex_coord_loc != -1);
  state.mvp_loc = glGetUniformLocation(state.shader_program, "mvp");
  check(state.mvp_loc != -1);
  state.texture_loc = glGetUniformLocation(state.shader_program, "texture");
  check(state.texture_loc != -1);
  check_gl();
  return state;
}

void term_shader_program(shader_state& state) {
  glDetachShader(state.shader_program, state.vertex_shader);
  glDetachShader(state.shader_program, state.fragment_shader);
  glDeleteShader(state.vertex_shader);
  glDeleteShader(state.fragment_shader);
  glDeleteProgram(state.shader_program);
  state = shader_state();
}

void init_display(app_state& app) {
  app.gl = init_gl(app.android_app_instance->window, -1, -1);

  EGLint width, height;
  eglQuerySurface(app.gl.display, app.gl.surface, EGL_WIDTH, &width);
  eglQuerySurface(app.gl.display, app.gl.surface, EGL_HEIGHT, &height);

  app.width = width;
  app.height = height;

  app.shader = init_shader_program();

  glVertexAttribPointer(app.shader.pos_loc, 3, GL_FLOAT, GL_FALSE, 0, g_vertices);
  glEnableVertexAttribArray(app.shader.pos_loc);
  glVertexAttribPointer(app.shader.tex_coord_loc, 2, GL_FLOAT, GL_FALSE, 0, g_tex_coords);
  glEnableVertexAttribArray(app.shader.tex_coord_loc);
  check_gl();

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  check_gl();

  glViewport(0, 0, width, height);
}

void draw_frame(app_state* app) {
  if(!app->gl.valid()) {
    return;
  }

  unix_socket_address renderer_addr(g_renderer_socket_path);
  send_message(app->connection.sock, message("render-frame"), renderer_addr);
  message msg = recv_message(app->connection.sock);
  check(msg.type == "frame-finished");
  swap(app->connection.front_buffer, app->connection.back_buffer);

  glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  check_gl();

  glUseProgram(app->shader.shader_program);
  check_gl();

  // float mvp_matrix[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
  // Hard-coded orthographic transform. Creates a transform where the viewable area is
  // the unit cube with dimensions (0 0 0) x (1 1 1).
  float mvp_matrix[16] = {2,0,0,0, 0,2,0,0, 0,0,-2,0, -1,-1,-1,1};
  glUniformMatrix4fv(app->shader.mvp_loc, 1, GL_FALSE, mvp_matrix);
  check_gl();

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, app->connection.front_buffer->texture_id);
  check_gl();
  glUniform1i(app->shader.texture_loc, 0);
  check_gl();

  glDrawArrays(GL_TRIANGLES, 0, 6);

  check_gl();
  check_egl(eglSwapBuffers(app->gl.display, app->gl.surface));
}

/**
 * Tear down the EGL context currently associated with the display.
 */
void term_display(app_state& app) {
  app.animating = false;
  term_shader_program(app.shader);
  term_gl(app.gl);
}

/**
 * Process the next main command.
 */
void on_android_cmd(android_app* android_app_instance, int32_t cmd) {
  app_state* app = (app_state*)android_app_instance->userData;
  switch(cmd) {
  case APP_CMD_SAVE_STATE:
    app->android_app_instance->savedState = NULL;
    app->android_app_instance->savedStateSize = 0;
    break;
  case APP_CMD_INIT_WINDOW:
    // The window is being shown, get it ready.
    if(app->android_app_instance->window != NULL) {
      init_display(*app);
      app->connection = init_renderer_connection(app->width, app->height);
      app->animating = true;
      draw_frame(app);
    }
    break;
  case APP_CMD_TERM_WINDOW:
    // The window is being hidden or closed, clean it up.
    term_renderer_connection(app->connection);
    term_display(*app);
    break;
  case APP_CMD_GAINED_FOCUS:
    break;
  case APP_CMD_LOST_FOCUS:
    // Also stop animating.
    app->animating = 0;
    break;
  }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void android_main(android_app* android_app_instance) {
  app_state app;

  // Make sure glue isn't stripped.
  app_dummy();

  android_app_instance->userData = &app;
  android_app_instance->onAppCmd = on_android_cmd;
  app.android_app_instance = android_app_instance;

  // loop waiting for stuff to do.

  while(1) {
    // Read all pending events.
    int ident;
    int events;
    android_poll_source* source;

    // If not animating, we will block forever waiting for events.
    // If animating, we loop until all events are read, then continue
    // to draw the next frame of animation.
    while((ident=ALooper_pollAll(app.animating ? 0 : -1, NULL, &events,
                                 (void**)&source)) >= 0) {

      // Process this event.
      if(source != NULL) {
        source->process(android_app_instance, source);
      }

      // Check if we are exiting.
      if(android_app_instance->destroyRequested != 0) {
        term_renderer_connection(app.connection);
        term_display(app);
        return;
      }
    }

    if(app.animating) {
      // Drawing is throttled to the screen update rate, so there
      // is no need to do timing here.
      usleep(500000);
      draw_frame(&app);
    }
  }
}
