#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include "common.h"

int connect_to_host(void)
{
  struct sockaddr_un address;
  int  socket_fd, nbytes;
  char buffer[256];

  socket_fd = socket(PF_UNIX, g_stream ? SOCK_STREAM : SOCK_DGRAM, 0);
  if(socket_fd < 0)
  {
    printf("socket() failed\n");
    return 1;
  }

  /* start with a clean address structure */
  memset(&address, 0, sizeof(struct sockaddr_un));
 
  address.sun_family = AF_UNIX;
  snprintf(address.sun_path, UNIX_PATH_MAX, "./demo_socket");

  // if(bind(socket_fd, 
  //        (struct sockaddr *) &address, 
  //        sizeof(struct sockaddr_un)) != 0)
  // {
  //   printf("bind() failed\n");
  //   return 1;
  // }

  if(g_stream) {
    if(connect(socket_fd, 
               (struct sockaddr *) &address, 
               sizeof(struct sockaddr_un)) != 0)
    {
      printf("connect() failed\n");
      return 1;
    }
  }

  nbytes = snprintf(buffer, 256, "hello from a client");

  if(g_stream) {
    write(socket_fd, buffer, nbytes);
    nbytes = read(socket_fd, buffer, 256);
    buffer[nbytes] = 0;
  }
  else {
    sendto(socket_fd, buffer, nbytes, 0, (struct sockaddr *) &address,
           sizeof(struct sockaddr_un));
    sockaddr_un from_addr;
    socklen_t from_addr_len = sizeof(from_addr);
    nbytes = recvfrom(socket_fd, buffer, 256, 0, (sockaddr*)&from_addr, &from_addr_len);
    buffer[nbytes] = 0;
  }

  printf("MESSAGE FROM SERVER: %s\n", buffer);

  close(socket_fd);

  return 0;
}


int main() {
  return connect_to_host();
}
