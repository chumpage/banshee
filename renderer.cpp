#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <cassert>
#include "common.h"

using namespace std;

void handle_connection(int sock) {
  send_message(sock, form_connect_message());
  message msg = recv_message(sock);
  printf("received message from host: %s\n", serialize_message(msg).c_str());
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
