// #include "./input.h"

#include "input.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc < 4)
    return printf("Usage: %s <cache> <IP> <TCP> [regIP=193.136.138.142] "
                  "[regUDP=59000]\n"
                  "  cache  - size of the node cache\n"
                  "  IP     - IP address of the server\n"
                  "  TCP    - TCP port to listen on\n"
                  "  regIP  - IP address of the node\n"
                  "  regUDP - UDP port of the node\n",
                  argv[0]),
           1;

  /* Connect via UDP to the server */

  usize cache = atoi(argv[1]);
  char *IP = argv[2];
  char *TCP = argv[3];
  char *regIP =
      argc > 4 ? argv[4] : INADDR_ANY; //"193.136.138.142"; //NOTE: foi so para
  // debug, depois tiro
  char *regUDP = argc > 5 ? argv[5] : "59000";



  int listener_fd, new_fd, max_fd, counter;
  struct addrinfo hints, *res;
  struct sockaddr addr;
  socklen_t addrlen;
  char buffer[128];
  fd_set master_fds, read_fds;

  memset(&hints, 0, sizeof hints);

  hints.ai_socktype = SOCK_STREAM;  // TCP socket
  hints.ai_flags = AI_PASSIVE;

  // Get address info for binding the socket
  if (getaddrinfo(NULL, TCP, &hints, &res) != 0) {
    perror("getaddrinfo");
    exit(1);
  }

  // Create a socket
  if ((listener_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
    perror("socket");
    exit(1);
  }

  // Bind socket to the specified port
  if (bind(listener_fd, res->ai_addr, res->ai_addrlen) == -1) {
    perror("bind");
    exit(1);
  }

  // No longer needed, so we free the structure
  freeaddrinfo(res);

  // Start listening for incoming connections
  if (listen(listener_fd, 5) == -1) {
    perror("listen");
    exit(1);
  }

  printf("Server is listening on port %s...\n", TCP);


  // Node init
   Node node;
   node.ip = IP;
   node.tcp = TCP;
   node.cache_size = cache;
   node.server = malloc(sizeof(Server));
   if (node.server == NULL) {
     perror("Memory allocation fail");
     return 1;
  }
  node.server->fd = listener_fd;
  //node.server->addr = TCP;



  user_in(listener_fd, &node);
  /* wooooo
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    if (sigaction(SIGPIPE, &sa, NULL) == -1)
      return perror("FATAL: Failed to ignore SIGPIPE"), 1;
  */

 return 0;
}
