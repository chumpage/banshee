// taken from http://www.thomasstover.com/uds.html

#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <ui/GraphicBuffer.h>
#include "common.h"
#include <vector>

using namespace android;

int connection_handler(int connection_fd)
{
  int nbytes;
  char buffer[256];

  nbytes = read(connection_fd, buffer, 256);
  buffer[nbytes] = 0;

  printf("MESSAGE FROM CLIENT: %s\n", buffer);
  nbytes = snprintf(buffer, 256, "hello from the server");
  write(connection_fd, buffer, nbytes);
 
  close(connection_fd);
  return 0;
}

// ssize_t recvfrom(int socket, void *buffer, size_t length, int flags,
//              struct sockaddr *address, socklen_t *address_len);

int recv_msg(int sock_fd)
{
  int nbytes;
  char buffer[256];

  // sleep(2);
  sockaddr_un from_addr;
  socklen_t from_addr_len = sizeof(from_addr);
  nbytes = recvfrom(sock_fd, buffer, 256, 0, (sockaddr*)&from_addr, &from_addr_len);
  // nbytes = read(sock_fd, buffer, 256);
  buffer[nbytes] = 0;

  printf("MESSAGE FROM CLIENT: %s\n", buffer);
  nbytes = snprintf(buffer, 256, "hello from the server");
  sendto(sock_fd, buffer, nbytes, 0, (sockaddr*)&from_addr, sizeof(from_addr));
  // write(sock_fd, buffer, nbytes);

  // sleep(2);
 
  // close(connection_fd);
  return 0;
}

int recv_connection()
{
  struct sockaddr_un address;
  int socket_fd, connection_fd;
  socklen_t address_length;
  pid_t child;
 
  socket_fd = socket(PF_UNIX, g_stream ? SOCK_STREAM : SOCK_DGRAM, 0);
  if(socket_fd < 0)
  {
    printf("socket() failed\n");
    return 1;
  } 

  unlink("./demo_socket");

  /* start with a clean address structure */
  memset(&address, 0, sizeof(struct sockaddr_un));

  address.sun_family = AF_UNIX;
  snprintf(address.sun_path, UNIX_PATH_MAX, "./demo_socket");

  if(bind(socket_fd, 
         (struct sockaddr *) &address, 
         sizeof(struct sockaddr_un)) != 0)
  {
    printf("bind() failed\n");
    return 1;
  }

  if(g_stream) {
    if(listen(socket_fd, 5) != 0)
    {
      printf("listen() failed\n");
      return 1;
    }

    while((connection_fd = accept(socket_fd, 
                               (struct sockaddr *) &address,
                               &address_length)) > -1)
    {
      connection_handler(connection_fd);
      close(connection_fd);
    }
  }
  else {
    while(1) 
      recv_msg(socket_fd);
  }

  close(socket_fd);
  unlink("./demo_socket");
  return 0;
}

int main() {
  // vector<int> vals(10, 0);
  printf("Creating GraphicBuffer\n");
  GraphicBuffer buffer(1024, 1024, PIXEL_FORMAT_RGB_565, GraphicBuffer::USAGE_HW_TEXTURE);
  printf("Done creating GraphicBuffer\n");
  return recv_connection();
  // return 0;
}
