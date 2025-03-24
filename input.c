#include "input.h"
#include "commands.h"
#include "protocols/tcp.h"
#include "protocols/udp.h"
#include "types.h"
#include "util.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

// Unified function that handles both network I/O and user input
void ndn_run(Node *node) {
  int listener_fd = node->listener_fd;
  int new_fd, max_fd;
  struct sockaddr_storage addr;
  socklen_t addrlen;
  char buffer[128];
  fd_set master_fds, read_fds;

  char **last_msgs = malloc(sizeof(char *) * 100);
  if (!last_msgs) {
    perror(ERR "malloc");
    exit(1);
  }
  usize last_msgs_capacity = 100;

  for (usize i = 0; i < last_msgs_capacity; i++) {
    last_msgs[i] = NULL;
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
    // Clean up any invalid file descriptors from master_fds.
    for (int i = 0; i < FD_SETSIZE; i++) {
      if (FD_ISSET(i, &master_fds)) {
        // Check if the file descriptor is still valid.
        if (fcntl(i, F_GETFD) == -1) {
          FD_CLR(i, &master_fds);
        }
      }
    }

    // Recalculate max_fd based on the current master_fds.
    max_fd = (listener_fd > STDIN_FILENO ? listener_fd : STDIN_FILENO);
    for (int i = 0; i < FD_SETSIZE; i++) {
      if (FD_ISSET(i, &master_fds) && i > max_fd) {
        max_fd = i;
      }
    }

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
          fflush(stdout);
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

            free(last_msgs[i]);
            last_msgs[i] = NULL;
          } else {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
              // Not an error - just no data available now
              continue;
            }
            perror(ERR "read");
          }

          ndn_node_exit(node, i);
          // Close the socket and remove from fd set
          close(i);
          FD_CLR(i, &master_fds); // Make sure to clear it from master_fds

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
          char *escaped = str_escape(buffer);
          printf("\b\b" MAGENTA "fd_%02d" RESET "\t%s\n", i, escaped);
          free(escaped);

          if (i >= (i64)last_msgs_capacity) {
            usize new_capacity = i * 2;
            char **new_last_msgs =
                realloc(last_msgs, new_capacity * sizeof(char *));
            if (!new_last_msgs) {
              perror(ERR "realloc");
              exit(1);
            }
            last_msgs = new_last_msgs;
            for (usize j = last_msgs_capacity; j < new_capacity; j++) {
              last_msgs[j] = NULL;
            }
            last_msgs_capacity = new_capacity;
          }

          // Step 1: Combine existing buffer with new data
          char *combined_buffer;
          if (last_msgs[i] != NULL) {
            // We have a previous incomplete message, append new data
            combined_buffer = malloc(strlen(last_msgs[i]) + bytes_read + 1);
            if (!combined_buffer) {
              perror(ERR "malloc");
              exit(1);
            }
            strcpy(combined_buffer, last_msgs[i]);
            strcat(combined_buffer, buffer);
            free(last_msgs[i]);
          } else {
            // No previous data, just use the current buffer
            combined_buffer = strdup(buffer);
            if (!combined_buffer) {
              perror(ERR "strdup");
              exit(1);
            }
          }

          // Step 2: Process all complete messages
          char *search_start = combined_buffer;
          char *next_newline;
          bool processed_command = false;

          while ((next_newline = strchr(search_start, '\n')) != NULL) {
            // Found a complete message
            *next_newline = '\0'; // Replace newline with null terminator

            printf(NOTICE "Processing complete message: %s\n", search_start);

            // Process based on command type
            if (!memcmp(search_start, "ENTRY", 5)) {
              char ip[16], tcp[6];
              if (sscanf(search_start, "ENTRY %15s %5s", ip, tcp) == 2) {
                printf(NOTICE "Processing ENTRY from %s:%s\n", ip, tcp);
                ndn_entry(node, ip, tcp, i);
                processed_command = true;
              } else {
                fprintf(stderr, ERR "Invalid ENTRY format\n");
              }
            }

            // ------------------------------

            else if (!memcmp(search_start, "SAFE", 4)) {
              char ip[16], tcp[6];
              if (sscanf(search_start + 5, "%15s %5s", ip, tcp) == 2) {
                ndn_safe(node, ip, tcp);
                processed_command = true;
              } else {
                fprintf(stderr, ERR "Invalid SAFE format: %s\n", search_start);
              }
            }

            // ------------------------------

            else if (!memcmp(search_start, "INTEREST", 8)) {
              char object[101];
              if (sscanf(search_start, "INTEREST %100s", object) == 1) {
                printf(NOTICE "Processing INTEREST for object %s\n", object);
                ndn_interest(node, object, i);
                processed_command = true;
              } else {
                fprintf(stderr, ERR "Invalid INTEREST format\n");
              }
            }

            // ------------------------------

            else if (!memcmp(search_start, "OBJECT", 6)) {
              char object[101];
              if (sscanf(search_start, "OBJECT %100s", object) == 1) {
                printf(NOTICE "Processing OBJECT for object %s\n", object);
                ndn_object(node, object, i);

                if (node->current_retrieval != NULL &&
                    strcmp(node->current_retrieval, object) == 0) {
                  node->retrieval_done = true;
                }

                processed_command = true;
              } else {
                fprintf(stderr, ERR "Invalid OBJECT format\n");
              }
            }

            // ------------------------------

            else if (!memcmp(search_start, "NOOBJECT", 8)) {
              char object[101];
              if (sscanf(search_start, "NOOBJECT %100s", object) == 1) {
                printf(NOTICE "Processing NOOBJECT for object %s\n", object);
                ndn_noobject(node, object, i);
                processed_command = true;
              } else {
                fprintf(stderr, ERR "Invalid NOOBJECT format\n");
              }
            }

            // ------------------------------

            // Add other command handlers here

            else {
              fprintf(stderr, ERR "Unknown command: %s\n", search_start);
            }

            // Move to the character after the newline
            search_start = next_newline + 1;
          }

          // Step 3: Store any remaining incomplete message
          if (*search_start != '\0') {
            // We have an incomplete message
            last_msgs[i] = strdup(search_start);
            printf(NOTICE "Stored incomplete message: %s\n", search_start);
          } else {
            // No incomplete message
            last_msgs[i] = NULL;
          }

          free(combined_buffer);

          if (!processed_command) {
            printf(NOTICE "No complete commands found in message\n");
          }

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

    // Add this section to your main event loop in ndn_run
    if (node->current_retrieval != NULL) {
      time_t now = time(NULL);
      int elapsed = now - node->retrieval_start_time;

      // Only check once per second to avoid spamming
      static time_t last_check = 0;
      if (now != last_check) {
        last_check = now;

        // Check if object has been received (via flag or cache)
        if (node->retrieval_done) {
          printf("\b\b" OK "Object '%s' found after %d seconds\n",
                 node->current_retrieval, elapsed);
          free(node->current_retrieval);
          node->current_retrieval = NULL;
          node->retrieval_done = false;

          printf(YELLOW "> ");
          fflush(stdout);
        } else if (elapsed >= node->retrieval_timeout) {
          printf(ERR "Timeout after %d seconds waiting for '%s'\n", elapsed,
                 node->current_retrieval);
          free(node->current_retrieval);
          node->current_retrieval = NULL;

          printf(YELLOW "> ");
          fflush(stdout);
        }
      }
    }
  }

  printf(NOTICE "Exiting main loop\n");

  // Clean the last messages
  for (usize i = 0; i < last_msgs_capacity; i++) {
    free(last_msgs[i]);
  }
  free(last_msgs);

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
    if (!is_valid_net(net)) {
      fprintf(stderr, ERR "Wrong input, it must be 3 digits.\n");
      return;
    }
    node->net = atoi(net);
    NodeList *nodes = ndn_nodes(node);
    node->net = 1000;

    if (nodes != NULL) {
      for (usize i = 0; i < nodes->size; i++) {
        printf("\t(%zu) %s:%s\n", i, nodes->ip[i], nodes->tcp[i]);
      }

      clean_nodelist(nodes);
    }

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

        // Set self as external
        node->external->ip = strdup(node->ip);
        node->external->tcp = strdup(node->tcp);
        node->external->fd = -1;

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

  //---retrieve---
  if ((sscanf(input, "retrieve %100s%n", name, &pos) == 1 &&
       input[pos] == '\0') ||
      (sscanf(input, "r %100s%n", name, &pos) == 1 && input[pos] == '\0')) {
    if (!is_valid_name(name)) {
      fprintf(stderr, ERR "Invalid name (alphanumeric, 1-100 chars)\n");
      return;
    }

    ndn_retrieve(node, name);

    return;
  }
  //----------

  //---show names---
  if ((strcmp(input, "show names") == 0) || (strcmp(input, "sn") == 0)) {
    ndn_show_names(node);
    return;
  }
  //----------

  //---show interest table---
  if ((strcmp(input, "show interest table") == 0) ||
      (strcmp(input, "si") == 0)) {
    ndn_show_interest_table(node);
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
