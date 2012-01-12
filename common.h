#ifndef common_h
#define common_h

#include <vector>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <ui/GraphicBuffer.h>
#include <utils/RefBase.h>

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

message form_surfaces_message(const android::GraphicBuffer& front_gbuf,
                              const android::GraphicBuffer& back_gbuf);
void unpack_surfaces_message(const message& msg,
                             android::sp<android::GraphicBuffer>* front_gbuf,
                             android::sp<android::GraphicBuffer>* back_gbuf);

bool is_address_bound(const unix_socket_address& addr);

message recv_message(int socket, unix_socket_address* from_addr = NULL);
void send_message(int socket,
                  const message& msg,
                  const unix_socket_address& to_addr);

#endif
