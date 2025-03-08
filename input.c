#include "input.h"
#include "commands.h"
#include "protocols/udp.h"
#include "util.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

void user_in(Node *node) {
  int listener_fd = node->listener_fd;
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

            if (!memcmp(buffer, "x", 1) || !memcmp(buffer, "exit", 4)) {
              write(1, "Terminating\n", 12);
              clean_node(node);
              exit(0);
            }

            process_input_commands(node, buffer);
          }
        }
      }
    }
  }
}

void process_input_commands(Node *node, char *input) {
  char net[4], ip[16], port[6], name[101];
  int pos;
  input[strcspn(input, "\n")] = '\0';

  //---nodes---
  if ((strcmp(input, "nodes") == 0) || (strcmp(input, "n") == 0)) {
    NodeList *nodes = ndn_nodes(node, 123);

    for (usize i = 0; i < nodes->size; i++) {
      printf("(%zu) %s:%s\n", i, nodes->ip[i], nodes->tcp[i]);
      free(nodes->ip[i]);
      free(nodes->tcp[i]);
    }

    free(nodes->ip);
    free(nodes->tcp);
    free(nodes);

    return;
  }
  //----------

  //---join---
  if ((sscanf(input, "join %3s%n", net, &pos) == 1 && input[pos] == '\0') ||
      (sscanf(input, "j %3s%n", net, &pos) == 1 && input[pos] == '\0')) {

    if (!is_valid_net(net)) {
      printf("Wrong input, it must be 3 digits.\n");
      return;
    }

    ndn_join(node, atoi(net));
    printf("Joining network %s...\n", net);
    return;
  }
  //----------

  //---direct join---
  if ((sscanf(input, "direct join %15s %5s%n", ip, port, &pos) == 2 &&
       input[pos] == '\0') ||
      (sscanf(input, "dj %15s %5s%n", ip, port, &pos) == 2 &&
       input[pos] == '\0')) {

    if (!is_valid_ip(ip)) {
      printf("Invalid IP address\n");
    } else if (!is_valid_port(port)) {
      printf("Invalid port number\n");
    } else {
      printf("Direct joining via %s:%s\n", ip, port);

      if (strcmp(ip, "0.0.0.0") == 0) {
        printf("Created new network\n");
      }
    }

    ndn_direct_join(node, atoi(net), ip, port);

    return;
  }

  //----------

  //---create---
  if ((sscanf(input, "create %100s%n", name, &pos) == 1 &&
       input[pos] == '\0') ||
      (sscanf(input, "c %100s%n", name, &pos) == 1 && input[pos] == '\0')) {

    if (!is_valid_name(name)) {
      printf("Invalid name (alphanumeric, 1-100 chars)\n");
      return;
    }

    ndn_create(node, name);

    printf("Created object '%s'\n", name);
    return;
  }
  //----------

  //---delete---
  if ((sscanf(input, "delete %100s%n", name, &pos) == 1 &&
       input[pos] == '\0') ||
      (sscanf(input, "dl %100s%n", name, &pos) == 1 && input[pos] == '\0')) {

    if (!is_valid_name(name)) {
      printf("Invalid name\n");
      return;
    }

    ndn_delete(node, name);

    printf("Deleted object '%s'\n", name);
    return;
  }
  //----------

  //---help---
  if ((strcmp(input, "help") == 0) || (strcmp(input, "h") == 0)) {
    ndn_help();
    return;
  }
  //----------

  // if the command does not exist
  printf("That command does not exist. Please type '(h)elp' for the list of "
         "commands\n");
}
