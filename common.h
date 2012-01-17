#ifndef common_h
#define common_h

#include <vector>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <utils/RefBase.h>
#include <android/native_window.h>
#include <ui/PixelFormat.h>
#include <ui/android_native_buffer.h>
#include <ui/Rect.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

// Very nasty hack to work around a stupid compilation error:
//   /st/fire/cloudos/mydroid/system/core/include/cutils/uio.h:33: error: redefinition of 'struct iovec'
//   /st/android/ndk-r7-lab126/toolchains/arm-linux-androideabi-4.4.3/prebuilt/linux-x86/bin/../sysroot/usr/include/linux/uio.h:19: error: previous definition of 'struct iovec'
#define _LIBS_CUTILS_UIO_H 
#include <ui/GraphicBufferAllocator.h>

const std::string g_host_socket_path = "/data/local/banshee/ipc_host";
const std::string g_renderer_socket_path = "/data/local/banshee/ipc_renderer";
const bool g_print_ipc = false;

#if defined(ANDROID_APP)
#include <android/log.h>
#define logi(...) ((void)__android_log_print(ANDROID_LOG_INFO, "banshee-host", __VA_ARGS__))
#define logw(...) ((void)__android_log_print(ANDROID_LOG_WARN, "banshee-host", __VA_ARGS__))
#define loge(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "banshee-host", __VA_ARGS__))
#else
#define logi(...) printf(__VA_ARGS__);
#define logw(...) printf(__VA_ARGS__);
#define loge(...) printf(__VA_ARGS__);
#endif

#define check(proc) \
  { \
    bool result = proc; \
    if(!result) { \
      loge("Check failed at file %s, line %d. Exiting.\n",  __FILE__, __LINE__); \
      assert(false); \
    } \
  }

#define check_unix(proc) \
  { \
    int rc = proc; \
    if(rc < 0) { \
      loge("Unix error at file %s, line %d: rc=%d, errno=0x%x(%s). Exiting.\n", \
           __FILE__, __LINE__, rc, errno, strerror(errno)); \
      assert(false); \
    } \
  }

#define check_android(proc) \
  { \
    int rc = proc; \
    if(rc < 0) { \
      loge("Android Unix error at file %s, line %d: rc=%d, errno=0x%x(%s). Exiting.\n", \
           __FILE__, __LINE__, rc, errno, strerror(-errno)); \
      assert(false); \
    } \
  }

#define check_egl(proc) \
  { \
    EGLBoolean rc = proc; \
    if(rc == EGL_FALSE) { \
      loge("EGL error at file %s, line %d: 0x%x. Exiting.\n", \
           __FILE__, __LINE__, eglGetError()); \
      assert(false); \
    } \
  }

#define check_gl() \
  { \
    GLenum error = glGetError(); \
    if(error != GL_NO_ERROR) { \
      loge("GL error at file %s, line %d: 0x%x. Exiting.\n", \
           __FILE__, __LINE__, error); \
      assert(false); \
    } \
  }

struct unix_socket_address {
  unix_socket_address();
  unix_socket_address(const std::string& path);
  unix_socket_address(const unix_socket_address& addr);

  const unix_socket_address& operator=(const unix_socket_address& addr);

  sockaddr* sock_addr() const;
  sockaddr_un* sock_addr_un() const;
  socklen_t len() const;

  sockaddr_un addr;
};

struct message {
  message();
  message(const std::string& type,
          const std::vector<std::string>& args = std::vector<std::string>());

  std::string type;
  std::vector<std::string> args;
  std::vector<int> fds;
};

message parse_message(const std::string& raw_msg);
std::string serialize_message(const message& msg);

message form_connect_message();
message form_terminate_message();
message form_request_surfaces_message(int width, int height);
void unpack_request_surfaces_message(const message& msg, int* width, int* height);

struct gralloc_buffer;
message form_surfaces_message(const gralloc_buffer& front_gbuf,
                              const gralloc_buffer& back_gbuf);
void unpack_surfaces_message(const message& msg,
                             android::sp<gralloc_buffer>* front_gbuf,
                             android::sp<gralloc_buffer>* back_gbuf);

bool is_address_bound(const unix_socket_address& addr);

message recv_message(int socket, unix_socket_address* from_addr = NULL);
void send_message(int socket,
                  const message& msg,
                  const unix_socket_address& to_addr);

struct gl_state {
  EGLDisplay display;
  EGLSurface surface;
  EGLContext context;

  gl_state();
  gl_state(EGLDisplay display, EGLSurface surface, EGLContext context);
  bool valid();
};

// Either window is non-null and a window surface is created, or window is null and
// a pbuffer surface is created with the specified width and height.
gl_state init_gl(ANativeWindow* window, int pbuffer_width, int pbuffer_height);
void term_gl(gl_state& state);

struct shader_state {
  GLuint vertex_shader;
  GLuint fragment_shader;
  GLuint program;

  shader_state();
  shader_state(GLuint vertex_shader_,
               GLuint fragment_shader_,
               GLuint program_);
};

shader_state init_shader(const char* vertex_shader_src,
                         const char* fragment_shader_src);
void term_shader(shader_state& shader);
GLint get_shader_uniform(const shader_state& shader, const char* name);
GLint get_shader_attribute(const shader_state& shader, const char* name);

struct gralloc_native_buffer
  : public android::EGLNativeBase<android_native_buffer_t, 
                                  gralloc_native_buffer, 
                                  android::LightRefBase<gralloc_native_buffer> > {
};

struct gralloc_buffer : public android::LightRefBase<gralloc_buffer> {
  // android_native_buffer_t native_buffer;
  gralloc_native_buffer native_buffer;
  EGLImageKHR egl_img;
  GLuint texture_id;

  gralloc_buffer();
  gralloc_buffer(
    int width, int height, android::PixelFormat pixel_format, unsigned int usage);
  virtual ~gralloc_buffer();

  void pack(message& msg) const;
  void unpack(const message& msg, int& arg_offset, int& fd_offset);

  void* lock(int usage, const android::Rect* rect = NULL) const;
  void unlock() const;

private:
  bool deallocate_handle;
  void init();
  void clear();
  void create_texture();
};

double get_time(); // in seconds

struct matrix {
  std::vector<float> m;
  matrix();
  matrix(float m00, float m01, float m02, float m03,
         float m10, float m11, float m12, float m13,
         float m20, float m21, float m22, float m23,
         float m30, float m31, float m32, float m33);
  const float& operator()(int i, int j) const;
  float& operator()(int i, int j);
};

matrix matrix_mult(const matrix& mat1, const matrix& mat2);
matrix matrix_z_rot(float radians);
matrix matrix_translate(float x, float y, float z);
matrix matrix_scale(float x, float y, float z);
matrix matrix_transpose(const matrix& mat);
matrix matrix_ortho(
  float left, float right, float bottom, float top, float near, float far);

#endif
