#include "util.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

void user_in(Node *node, fd_set *read_fds, int listener_fd) {
  char buffer[128];
  fd_set testfds;
  struct timeval timeout;

  while (1) {
    testfds = *read_fds;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

    int result = select(listener_fd + 1, &testfds, NULL, NULL, &timeout);

    switch (result) {
    case 0:
      printf("\n----------------Timeout event----------------\n");
      break;
    case -1:
      perror("select fail");
      exit(1);
    default:
      if (FD_ISSET(STDIN_FILENO, &testfds)) {
        if (fgets(buffer, sizeof(buffer), stdin)) {
          buffer[strcspn(buffer, "\n")] = '\0';
          process_input_commands(node, buffer);
          if (strcmp(buffer, "_STOP_") == 0) {
            printf("Terminating\n");
            exit(0);
          }
        }
      }
      if (FD_ISSET(listener_fd, &testfds)) {
        struct sockaddr_in udp_useraddr;
        socklen_t addrlen = sizeof(udp_useraddr);
        int ret = recvfrom(listener_fd, buffer, sizeof(buffer) - 1, 0,
                           (struct sockaddr *)&udp_useraddr, &addrlen);
        if (ret > 0) {
          buffer[ret] = '\0';
          printf("UDP Message: %s\n", buffer);
          if (strcmp(buffer, "_STOP_") == 0) {
            printf("Terminating\n");
            exit(0);
          }
        }
      }
    }
  }
}
