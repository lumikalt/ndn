// #include "./input.h"
#include "./types.h"
#include "util.h"

#include <arpa/inet.h>
#include <netdb.h> // for AI_PASSIVE
#include <pthread.h>
#include <signal.h>
#include <string.h>
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

  usize cache = atoi(argv[1]);
  char *IP = argv[2];
  char *TCP = argv[3];
  char *regIP = argc > 4 ? argv[4] : "193.136.138.142";
  char *regUDP = argc > 5 ? argv[5] : "59000";

  /* Connect via UDP to the server */

  struct addrinfo udp_hints, *udp;
  int serverfd, err;
  int listener_fd;

  memset(&udp_hints, 0, sizeof(udp_hints));
  udp_hints.ai_family = AF_INET;
  udp_hints.ai_socktype = SOCK_DGRAM;

  if (serverfd = socket(AF_INET, SOCK_DGRAM, 0), serverfd != 0)
    return perror("FATAL: Failed to create UDP socket"), 1;

  if (err = getaddrinfo(regIP, regUDP, &udp_hints, &udp), err != 0)
    return perror("FATAL: Failed to get address info"), 1;

  // TODO, Blueprint to connect to the adjacent nodes

  // struct addrinfo tcp_hints, *tcp;
  // int listenerfd /*, err*/;
  struct sigaction sa;

  // memset(&tcp_hints, 0, sizeof(tcp_hints));
  // tcp_hints.ai_family = AF_INET;
  // tcp_hints.ai_socktype = SOCK_STREAM;
  // tcp_hints.ai_flags = AI_PASSIVE;

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_IGN;

  if (sigaction(SIGPIPE, &sa, NULL) == -1)
    return perror("FATAL: Failed to ignore SIGPIPE"), 1;


  if ((listener_fd = socket(udp->ai_family, udp->ai_socktype, udp->ai_protocol)) == -1) {
    perror("socket");
    exit(1);
  }


  if (bind(listener_fd, udp->ai_addr, udp->ai_addrlen) == -1) {
    perror("bind");
    exit(1);
  }

  // No longer needed, so we free the structure
  freeaddrinfo(udp);

  // Start listening for incoming connections
  if (listen(listener_fd, 5) == -1) {
    perror("listen");
    exit(1);
  }

  printf("Server is listening on port %s...\n", regUDP);

  // fd_set master_fds, read_fds; // File descriptor sets
  // int max_fd,counter;
  // FD_ZERO(&master_fds);
  // FD_SET(listener_fd, &master_fds);
  // max_fd = listener_fd;

  fd_set read_fds;
  int bytes_read, max_fd = listener_fd;
  char buffer[128];


  //loop waiting in case of an input
  while (1) {
    FD_ZERO(&read_fds);
    FD_SET(listener_fd, &read_fds);
    FD_SET(STDIN_FILENO, &read_fds);

    if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
      perror("select");
      exit(1);
    }

    if (FD_ISSET(STDIN_FILENO, &read_fds)) {
      if ((bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer)-1)) <= 0) {
        perror("read");
        continue;
      }
      buffer[bytes_read] = '\0';
      buffer[strcspn(buffer, "\n")] = '\0';

      //check if the parameters of the input are valid
      process_input_commands(buffer);
    }
  }

  // if (listenerfd = socket(AF_INET, SOCK_STREAM, 0), listenerfd != 0)
  //   return perror("FATAL: Failed to create TCP socket"), 1;

  // if (err = getaddrinfo(IP, TCP, &tcp_hints, &tcp), err != 0)
  //   return perror("FATAL: Failed to get address info"), 1;

  // if (err = connect(serverfd, tcp->ai_addr, tcp->ai_addrlen), err != 0)
  //   return perror("FATAL: Failed to connect to server"), 1;

  /* User Input */

  // pthread_t input_thread;
  // pthread_create(&input_thread, NULL, user_in, NULL);

  // /*
  //  * TODO: Ensure graceful shutdown of the program.
  //  */

  // pthread_join(input_thread, NULL);

  /* Cleanup */

  freeaddrinfo(udp);
  close(serverfd);

  return 0;
}
