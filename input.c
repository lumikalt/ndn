#include "input.h"
#include "commands.h"
#include "protocols/tcp.h"
#include "protocols/udp.h"
#include "types.h"
#include "util.h"

#include <arpa/inet.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

// Unified function that handles both network I/O and user input
void ndn_run(Node *node) {
  int listener_fd = node->listener_fd;
  int new_fd, max_fd;
  struct sockaddr_storage addr;
  socklen_t addrlen;
  char buffer[128];
  fd_set master_fds, read_fds;

  FDMsg *last_msgs = malloc(sizeof(FDMsg) * 100);
  if (!last_msgs) {
    perror(ERR "malloc");
    exit(1);
  }
  usize last_msgs_size = 0;
  usize last_msgs_capacity = 10;

  for (usize i = 0; i < last_msgs_capacity; i++) {
    last_msgs[i].fd = -1;
    last_msgs[i].msg = NULL;
  }

  // Initialize file descriptor sets for select()
  FD_ZERO(&master_fds);
  FD_SET(listener_fd, &master_fds);
  FD_SET(STDIN_FILENO, &master_fds); // Add stdin to the set

  max_fd = listener_fd > STDIN_FILENO ? listener_fd : STDIN_FILENO;

  printf(OK "TCP server started and ready to accept connections\n");
  printf(NOTICE "Type '(h)elp' for the list of commands\n");

  printf(YELLOW "> ");
  fflush(stdout);

  while (!node->exit) {
    // Copy master set to temporary set for select()
    read_fds = master_fds;

    // Add timeout to prevent indefinite blocking
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

    if (activity < 0) {
      perror(ERR "select");
      break;
    }

    // Check for user input first
    if (FD_ISSET(STDIN_FILENO, &read_fds)) {
      char input[128];
      if (fgets(input, sizeof(input), stdin) != NULL) {
        input[strcspn(input, "\n")] = '\0';

        // Process user input
        process_user_input(node, input);

        // Print prompt again if not exiting
        if (!node->exit) {
          printf(YELLOW "> ");
        }
      }
    }

    // Check for incoming connections on listener socket
    if (FD_ISSET(listener_fd, &read_fds)) {
      addrlen = sizeof addr;
      new_fd = accept(listener_fd, (struct sockaddr *)&addr, &addrlen);

      if (new_fd < 0) {
        perror(ERR "accept");
      } else {
        struct sockaddr_in *client_addr = (struct sockaddr_in *)&addr;
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip,
                  INET_ADDRSTRLEN);

        printf("\b\b" MAGENTA "fd_%02d" RESET "\t<new connection %s:%05d>\n",
               new_fd, client_ip, ntohs(client_addr->sin_port));

        FD_SET(new_fd, &master_fds);
        if (new_fd > max_fd) {
          max_fd = new_fd;
        }

        printf(YELLOW "> ");
        fflush(stdout);
      }
    }

    // Check data from connected clients
    for (int i = 0; i <= max_fd; i++) {
      if (i != listener_fd && i != STDIN_FILENO && FD_ISSET(i, &read_fds)) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(i, buffer, sizeof(buffer) - 1);

        if (bytes_read <= 0) {
          if (bytes_read == 0) {
            printf("\b\b" MAGENTA "fd_%02d" RESET "\t<disconnected>\n", i);
          } else {
            perror(ERR "read");
          }

          // Update node structures when a client disconnects
          if (node->external && node->external->fd == i) {
            printf(NOTICE "External disconnected\n");
            free(node->external->ip);
            free(node->external->tcp);
            node->external->ip = NULL;
            node->external->tcp = NULL;
            node->external->fd = -1;

            if (node->safeguard->fd != -1) {
              close(node->safeguard->fd);
              free(node->safeguard->ip);
              free(node->safeguard->tcp);
              node->safeguard->ip = NULL;
              node->safeguard->tcp = NULL;
              node->safeguard->fd = -1;
            }
          }

          // Check all internals
          for (usize j = 0; j < node->internal_index; j++) {
            if (node->internal[j] && node->internal[j]->fd == i) {
              printf(NOTICE "Internal %s:%s disconnected\n",
                     node->internal[j]->ip, node->internal[j]->tcp);
              free(node->internal[j]->ip);
              free(node->internal[j]->tcp);
              free(node->internal[j]);

              // Shift remaining nodes down to fill the gap
              for (usize k = j; k < node->internal_index - 1; k++) {
                node->internal[k] = node->internal[k + 1];
              }
              node->internal_index--;
              node->internal[node->internal_index] = NULL;
              break;
            }
          }

          // Close the socket and remove from fd set
          close(i);
          FD_CLR(i, &master_fds);

          // Recalculate max_fd if needed
          if (i == max_fd) {
            max_fd = listener_fd; // Start with listener
            for (int j = 0; j <= FD_SETSIZE; j++) {
              if (FD_ISSET(j, &master_fds) && j > max_fd) {
                max_fd = j;
              }
            }
          }

          printf(YELLOW "> ");
          fflush(stdout);
        } else {
          buffer[bytes_read] = '\0';
          printf(MAGENTA "\b\bfd_%02d" RESET "\t```%s```\n", i, buffer);

          if (last_msgs[i].msg == NULL) {
            last_msgs[i].msg = strdup(buffer);
            last_msgs[i].fd = i;
            last_msgs_size++;
          } else {
            free(last_msgs[i].msg);
            last_msgs[i].msg = strdup(buffer);
          }

          // Process commands
          if (!memcmp(buffer, "ENTRY", 5)) {
            char ip[16], tcp[6];
            if (sscanf(buffer, "ENTRY %15s %5s", ip, tcp) == 2) {
              printf(NOTICE "Processing ENTRY from %s:%s\n", ip, tcp);
              ndn_entry(node, ip, tcp, i);
            } else {
              fprintf(stderr, ERR "Invalid ENTRY format\n");
            }
          }

          if (!memcmp(buffer, "SAFE ", 5)) {
            char ip[16], tcp[6];
            if (sscanf(buffer + 5, "%15s %5s", ip, tcp) == 2) {
              ndn_safe(node, ip, tcp);
            } else {
              fprintf(stderr, ERR "Invalid SAFE format: %s\n", buffer);
            }
          }
          // Add other command handlers here

          printf(YELLOW "> ");
          fflush(stdout);
        }
      }
    }

    // Add any new external connections to the set
    if (node->external && node->external->fd > 0) {
      if (!FD_ISSET(node->external->fd, &master_fds)) {
        FD_SET(node->external->fd, &master_fds);
        if (node->external->fd > max_fd) {
          max_fd = node->external->fd;
        }
      }
    }
  }

  printf(NOTICE "Exiting main loop\n");

  // Leave the network and send all internals the leave message
  ndn_leave(node);

  clean_node(node);
}

// Helper function to process user input
void process_user_input(Node *node, char *input) {
  char net[4], ip[16], port[6], name[101];
  int pos;

  printf(RESET);

  //---nodes---
  // check for nodes net
  if ((sscanf(input, "nodes %3s%n", net, &pos) == 1 && input[pos] == '\0') ||
      (sscanf(input, "n %3s%n", net, &pos) == 1 && input[pos] == '\0')) {
    NodeList *nodes = ndn_nodes(node, atoi(net));

    if (nodes != NULL)
      for (usize i = 0; i < nodes->size; i++) {
        printf("\t(%zu) %s:%s\n", i, nodes->ip[i], nodes->tcp[i]);
      }

    clean_nodelist(nodes);

    return;
  }
  //----------

  //---join---
  if ((sscanf(input, "join %3s%n", net, &pos) == 1 && input[pos] == '\0') ||
      (sscanf(input, "j %3s%n", net, &pos) == 1 && input[pos] == '\0')) {
    if (!is_valid_net(net)) {
      fprintf(stderr, ERR "Wrong input, it must be 3 digits.\n");
      return;
    }

    ndn_join(node, atoi(net));
    return;
  }
  //----------

  //---direct join---
  if ((sscanf(input, "direct join %15s %5s%n", ip, port, &pos) == 2 &&
       input[pos] == '\0') ||
      (sscanf(input, "dj %15s %5s%n", ip, port, &pos) == 2 &&
       input[pos] == '\0')) {

    if (!is_valid_ip(ip)) {
      fprintf(stderr, ERR "Invalid IP address\n");
    } else if (!is_valid_port(port)) {
      fprintf(stderr, ERR "Invalid port number\n");
    } else {
      printf(NOTICE "Direct joining via %s:%s\n", ip, port);

      if (strcmp(ip, "0.0.0.0") == 0) {
        printf(OK "Created new network\n");
        return;
      }

      ndn_direct_join(node, ip, port);
    }

    return;
  }
  //----------

  //---show topology---
  if ((strcmp(input, "show topology") == 0) || (strcmp(input, "st") == 0)) {
    ndn_show_topology(node);
    return;
  }

  //---create---
  if ((sscanf(input, "create %100s%n", name, &pos) == 1 &&
       input[pos] == '\0') ||
      (sscanf(input, "c %100s%n", name, &pos) == 1 && input[pos] == '\0')) {

    if (!is_valid_name(name)) {
      fprintf(stderr, ERR "Invalid name (alphanumeric, 1-100 chars)\n");
      return;
    }

    ndn_create(node, name);

    printf(OK "Created object `%s`\n", name);
    return;
  }
  //----------

  //---delete---
  if ((sscanf(input, "delete %100s%n", name, &pos) == 1 &&
       input[pos] == '\0') ||
      (sscanf(input, "dl %100s%n", name, &pos) == 1 && input[pos] == '\0')) {
    if (!is_valid_name(name)) {
      fprintf(stderr, ERR "Invalid name (alphanumeric, 1-100 chars)\n");
      return;
    }

    ndn_delete(node, name);

    printf(OK "Deleted object `%s`\n", name);
    return;
  }
  //----------

  //---exit---
  if ((strcmp(input, "exit") == 0) || (strcmp(input, "x") == 0)) {
    node->exit = true;
    printf(OK "Terminating\n");
    return;
  }
  //----------

  //---leave---
  if ((strcmp(input, "leave") == 0) || (strcmp(input, "l") == 0)) {
    printf(OK "Terminating\n");
    ndn_leave(node);
    return;
  }
  //----------

  //---help---
  if ((strcmp(input, "help") == 0) || (strcmp(input, "h") == 0)) {
    ndn_help();
    return;
  }
  //----------

  // if the command had the wrong format or does not exist
  fprintf(stderr,
          ERR "Command does not exist or wrong arguments passed\n" NOTICE
              "Type '(h)elp' for the list of commands\n");
}
