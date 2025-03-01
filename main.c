// #include "./input.h"

#include "input.h"
#include "util.h"
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

  struct addrinfo udp_hints, *udp;
  int server_fd, listener_fd, err;

  memset(&udp_hints, 0, sizeof(udp_hints));
  udp_hints.ai_family = AF_INET;
  udp_hints.ai_socktype = SOCK_DGRAM;

  if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("FATAL: Failed to create UDP socket");
    return 1;
  }

  if ((err = getaddrinfo(regIP, regUDP, &udp_hints, &udp)) != 0) {
    fprintf(stderr, "FATAL: getaddrinfo: %s\n", gai_strerror(err));
    return 1;
  }

  if ((listener_fd =
           socket(udp->ai_family, udp->ai_socktype, udp->ai_protocol)) == -1)
    return perror("socket"), 1;

  if (bind(listener_fd, udp->ai_addr, udp->ai_addrlen) == -1)
    return perror("bind"), 1;

  /* wooooo
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);

    if (sigaction(SIGPIPE, &sa, NULL) == -1)
      return perror("FATAL: Failed to ignore SIGPIPE"), 1;
  */

  printf("Server is listening on port %s...\n", regUDP);

  fd_set read_fds;
  //struct timeval timeout;
  //char buffer[128];

  FD_ZERO(&read_fds);              // to clear the set of discriptiors
  FD_SET(STDIN_FILENO, &read_fds); // add the keyboard to the set
  FD_SET(listener_fd, &read_fds);  // add a fd to the set


  user_in(&read_fds, listener_fd);



  freeaddrinfo(udp);
  close(server_fd);
  close(listener_fd);

  return 0;
}
