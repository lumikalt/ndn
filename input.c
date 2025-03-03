#include "util.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>


void user_in(int listener_fd) {
  int new_fd, max_fd, counter;
  struct sockaddr addr;
  socklen_t addrlen;
  char buffer[128];
  fd_set master_fds, read_fds;

  FD_ZERO(&master_fds);
  FD_SET(listener_fd, &master_fds);
  max_fd = listener_fd;

  while (1) {
    read_fds = master_fds;

    counter = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
    if (counter == -1) {
      perror("select fail");
      exit(1);
    }

    memset(buffer, 0, 128);
    for (int i = 0; i <= max_fd; i++) {
      if (FD_ISSET(i, &read_fds)) {
        if (i == listener_fd) {

          addrlen = sizeof addr;
          new_fd = accept(listener_fd, &addr, &addrlen);

          if (new_fd == -1) {
            perror("accept");
          } else {
            FD_SET(new_fd, &master_fds);
            if (new_fd > max_fd)
              max_fd = new_fd;
            printf("New connection established: FD %d\n", new_fd);
          }


        } else {
          int n = read(i, buffer, 128);

          if (n <= 0) {

            if (n == 0) {
              printf("Client on FD %d disconnected.\n", i);
            } else {
              perror("read");
            }
            close(i);
            FD_CLR(i, &master_fds);

          } else {
            printf("Message from FD %d: %s\n", i, buffer);
            if (write(i, buffer, n) == -1) {
              perror("write");
            }
          }
        }
      }
    }
  }

  close(listener_fd);
}
