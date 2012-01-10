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

struct gralloc_buffer : public LightRefBase<gralloc_buffer> {
  gralloc_buffer(sp<GraphicBuffer> gbuf);
  virtual ~gralloc_buffer();

  sp<GraphicBuffer> gbuf;
  EGLImageKHR egl_img; // EGL_NO_IMAGE_KHR
  GLuint texture_id;
};

gralloc_buffer::gralloc_buffer(sp<GraphicBuffer> gbuf_)
  : gbuf(gbuf_), egl_img(EGL_NO_IMAGE_KHR), texture_id((GLuint)-1) {
  check_android(GraphicBufferMapper::get().registerBuffer(gbuf->handle));

  EGLint img_attrs[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE };
  egl_img = eglCreateImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY),
                              EGL_NO_CONTEXT,
                              EGL_NATIVE_BUFFER_ANDROID,
                              (EGLClientBuffer)gbuf->getNativeBuffer(),
                              img_attrs);
  check_egl(egl_img != EGL_NO_IMAGE_KHR);

  glGenTextures(1, &texture_id);
  check_gl();
  glBindTexture(GL_TEXTURE_2D, texture_id);
  check_gl();
  glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_img);
  check_gl();
}

gralloc_buffer::~gralloc_buffer() {
  if(texture_id != (GLuint)-1) {
    glDeleteTextures(1, &texture_id);
    check_gl();
  }

  if(egl_img != EGL_NO_IMAGE_KHR) {
    check_egl(eglDestroyImageKHR(eglGetDisplay(EGL_DEFAULT_DISPLAY), egl_img));
  }

  if(gbuf.get()) {
    check_android(GraphicBufferMapper::get().unregisterBuffer(gbuf->handle));
  }
}

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

struct app_state {
  android_app* android_app_instance;

  bool animating;
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;
  int32_t width;
  int32_t height;
  renderer_connection connection;
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
  connection.front_buffer.clear();
  connection.back_buffer.clear();

  unix_socket_address renderer_addr(g_renderer_socket_path);
  send_message(connection.sock, message("terminate"), renderer_addr);
  
  check_unix(close(connection.sock));
  unlink(g_host_socket_path.c_str());
}

/**
 * Initialize an EGL context for the current display.
 */
int init_display(app_state* app) {
  EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  assert(display != EGL_NO_DISPLAY);
  check_egl(eglInitialize(display, 0, 0));

  const EGLint config_attribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
  };
  EGLint numConfigs;
  EGLConfig config;
  check_egl(eglChooseConfig(display, config_attribs, &config, 1, &numConfigs));

  /* EGL_NATIVE_VISUAL_ID is an attribute of the EGLConfig that is
   * guaranteed to be accepted by ANativeWindow_setBuffersGeometry().
   * As soon as we picked a EGLConfig, we can safely reconfigure the
   * ANativeWindow buffers to match, using EGL_NATIVE_VISUAL_ID. */
  EGLint buffer_format;
  check_egl(eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &buffer_format));

  ANativeWindow_setBuffersGeometry(app->android_app_instance->window, 0, 0, buffer_format);

  EGLSurface surface = eglCreateWindowSurface(
    display, config, app->android_app_instance->window, NULL);
  check_egl(surface != EGL_NO_SURFACE);

  const EGLint context_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };
  EGLContext context = eglCreateContext(display, config, NULL, context_attribs);
  check_egl(context != EGL_NO_CONTEXT);

  check_egl(eglMakeCurrent(display, surface, surface, context));

  EGLint w, h;
  eglQuerySurface(display, surface, EGL_WIDTH, &w);
  eglQuerySurface(display, surface, EGL_HEIGHT, &h);

  app->display = display;
  app->context = context;
  app->surface = surface;
  app->width = w;
  app->height = h;

  // Initialize GL state.
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  check_gl();

  return 0;
}

static void draw_frame(app_state* app) {
  if(app->display == NULL) {
    // No display.
    return;
  }

  // Just fill the screen with a color.
  glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  eglSwapBuffers(app->display, app->surface);
}

/**
 * Tear down the EGL context currently associated with the display.
 */
static void term_display(app_state* app) {
  if(app->display != EGL_NO_DISPLAY) {
    eglMakeCurrent(app->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if(app->context != EGL_NO_CONTEXT) {
      eglDestroyContext(app->display, app->context);
    }
    if(app->surface != EGL_NO_SURFACE) {
      eglDestroySurface(app->display, app->surface);
    }
    eglTerminate(app->display);
  }
  app->animating = false;
  app->display = EGL_NO_DISPLAY;
  app->context = EGL_NO_CONTEXT;
  app->surface = EGL_NO_SURFACE;
}

/**
 * Process the next main command.
 */
static void on_android_cmd(android_app* android_app_instance, int32_t cmd) {
  app_state* app = (app_state*)android_app_instance->userData;
  switch(cmd) {
  case APP_CMD_SAVE_STATE:
    // The system has asked us to save our current state.  Do so.
    // app->android_app_instance->savedState = malloc(sizeof(saved_state));
    // *((saved_state*)app->android_app_instance->savedState) = app->state;
    // app->android_app_instance->savedStateSize = sizeof(saved_state);
    app->android_app_instance->savedStateSize = 0;
    break;
  case APP_CMD_INIT_WINDOW:
    // The window is being shown, get it ready.
    if(app->android_app_instance->window != NULL) {
      init_display(app);
      app->connection = init_renderer_connection(app->width, app->height);
      app->animating = true;
      draw_frame(app);
    }
    break;
  case APP_CMD_TERM_WINDOW:
    // The window is being hidden or closed, clean it up.
    term_renderer_connection(app->connection);
    term_display(app);
    break;
  case APP_CMD_GAINED_FOCUS:
    break;
  case APP_CMD_LOST_FOCUS:
    // Also stop animating.
    app->animating = 0;
    draw_frame(app);
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

  memset(&app, 0, sizeof(app));
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
        term_display(&app);
        return;
      }
    }

    if(app.animating) {
      // Drawing is throttled to the screen update rate, so there
      // is no need to do timing here.
      draw_frame(&app);
    }
  }
}
