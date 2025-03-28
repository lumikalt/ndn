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

bool process_command(Node *node, char *command, int fd);

// Update ndn_run to use the Node's built-in FD tracking
void ndn_run(Node *node) {
  int listener_fd = node->listener_fd;
  int new_fd;
  struct sockaddr_storage addr;
  socklen_t addrlen;
  char buffer[128];
  fd_set read_fds;

  // Initialize file descriptor sets - they're now in Node
  // Only add listener_fd if already in a network
  if (node->in_net) {
    add_fd_to_set(node, listener_fd);
  }
  add_fd_to_set(node, STDIN_FILENO); // Always add stdin to the set

  printf(OK "TCP server started and ready to accept connections\n");
  printf(NOTICE "Type '(h)elp' for the list of commands\n");

  printf(YELLOW "> ");
  fflush(stdout);
  while (!node->exit) {
    // Check if the node is in a network and update listener_fd status if needed
    if (node->in_net && !FD_ISSET(listener_fd, &node->master_fds)) {
      add_fd_to_set(node, listener_fd);
    } else if (!node->in_net && FD_ISSET(listener_fd, &node->master_fds)) {
      remove_fd_from_set(node, listener_fd);
    }

    // Copy master set to temporary set for select()
    read_fds = node->master_fds;

    // Add timeout to prevent indefinite blocking
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    int activity = select(node->max_fd + 1, &read_fds, NULL, NULL, &timeout);

    if (activity < 0) {
      perror(ERR "select");
      break;
    }

    // Check for user input first - ensure STDIN is always in the set
    if (!FD_ISSET(STDIN_FILENO, &node->master_fds)) {
      add_fd_to_set(node, STDIN_FILENO);
    }

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
        // Use helper function to add the FD
        if (!is_valid_fd(new_fd)) {
          close(new_fd);
        } else {
          struct sockaddr_in *client_addr = (struct sockaddr_in *)&addr;
          char client_ip[INET_ADDRSTRLEN];
          inet_ntop(AF_INET, &(client_addr->sin_addr), client_ip,
                    INET_ADDRSTRLEN);

          printf("\b\b" MAGENTA "fd_%02d" RESET "\t<new connection %s:%05d>\n",
                 new_fd, client_ip, ntohs(client_addr->sin_port));

          add_fd_to_set(node, new_fd);

          printf(YELLOW "> ");
          fflush(stdout);
        }
      }
    }

    // Check data from connected clients
    for (int i = 0; i <= node->max_fd; i++) {
      if (i != listener_fd && i != STDIN_FILENO && FD_ISSET(i, &read_fds)) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_read = read(i, buffer, sizeof(buffer) - 1);

        if (bytes_read <= 0) {
          if (bytes_read == 0) {
            printf("\b\b" MAGENTA "fd_%02d" RESET "\t<disconnected>\n", i);

            free(node->last_msgs[i]);
            node->last_msgs[i] = NULL;
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
          remove_fd_from_set(node, i);

          printf(YELLOW "> ");
          fflush(stdout);
        } else {
          buffer[bytes_read] = '\0';
          char *escaped = str_escape(buffer);
          printf("\b\b" MAGENTA "fd_%02d" RESET "\t%s\n", i, escaped);
          free(escaped);

          if (i >= (i64)node->last_msgs_capacity) {
            usize new_capacity = i * 2;
            char **new_last_msgs =
                realloc(node->last_msgs, new_capacity * sizeof(char *));
            if (!new_last_msgs) {
              perror(ERR "realloc");
              exit(1);
            }
            node->last_msgs = new_last_msgs;
            for (usize j = node->last_msgs_capacity; j < new_capacity; j++) {
              node->last_msgs[j] = NULL;
            }
            node->last_msgs_capacity = new_capacity;
          }

          // Step 1: Combine existing buffer with new data
          char *combined_buffer;
          if (node->last_msgs[i] != NULL) {
            // We have a previous incomplete message, append new data
            combined_buffer =
                malloc(strlen(node->last_msgs[i]) + bytes_read + 1);
            if (!combined_buffer) {
              perror(ERR "malloc");
              exit(1);
            }
            strcpy(combined_buffer, node->last_msgs[i]);
            strcat(combined_buffer, buffer);
            free(node->last_msgs[i]);
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
            *next_newline = '\0'; // Replace newline with null terminator

            // Process the command
            if (process_command(node, search_start, i)) {
              processed_command = true;
            }

            // Move to the character after the newline
            search_start = next_newline + 1;
          }

          // Step 3: Store any remaining incomplete message
          if (*search_start != '\0') {
            // We have an incomplete message
            node->last_msgs[i] = strdup(search_start);
            printf(NOTICE "Stored incomplete message: %s\n", search_start);
          } else {
            // No incomplete message
            node->last_msgs[i] = NULL;
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

    // Process stored messages every iteration, regardless of new activity
    for (int i = 0; i <= node->max_fd; i++) {
      if (i != listener_fd && i != STDIN_FILENO &&
          FD_ISSET(i, &node->master_fds) && i < (i64)node->last_msgs_capacity &&
          node->last_msgs[i] != NULL) {

        // Check if this stored message has a complete command
        char *stored_msg = node->last_msgs[i];
        char *newline = strchr(stored_msg, '\n');

        if (newline) {
          printf(NOTICE "Processing stored complete message from fd %d: %s\n",
                 i, stored_msg);

          // Create a copy of the buffer to process
          char *process_buffer = strdup(stored_msg);
          if (!process_buffer) {
            perror(ERR "strdup");
            continue;
          }

          // Process all complete messages in the buffer
          char *search_start = process_buffer;
          char *next_newline;

          while ((next_newline = strchr(search_start, '\n')) != NULL) {
            *next_newline = '\0'; // Replace newline with null terminator

            // Process the command
            if (process_command(node, search_start, i)) {
            }

            // Move to the character after the newline
            search_start = next_newline + 1;
          }

          free(process_buffer);

          // Cleanup the stored message now that we've processed it
          free(node->last_msgs[i]);
          node->last_msgs[i] = NULL;

          // Print new prompt if needed
          printf(YELLOW "> " RESET);
          fflush(stdout);
        }
      }
    }

    // Add any new external connections to the set
    if (node->external && node->external->fd > 0) {
      if (!FD_ISSET(node->external->fd, &node->master_fds)) {
        add_fd_to_set(node, node->external->fd);
      }
    }
  }

  printf(NOTICE "Exiting main loop\n");

  // Leave the network and send all internals the leave message
  ndn_leave(node);

  clean_node(node);
}

bool process_command(Node *node, char *command, int fd) {
  if (!node->in_net) {
    fprintf(stderr, ERR "Not in a network\n");
    return false;
  }

  bool processed = false;

  // Process based on command type
  if (!memcmp(command, "ENTRY", 5)) {
    char ip[16], tcp[6];
    if (sscanf(command, "ENTRY %15s %5s", ip, tcp) == 2) {
      ndn_entry(node, ip, tcp, fd);
      processed = true;
    } else {
      fprintf(stderr, ERR "Invalid ENTRY format\n");
    }
  } else if (!memcmp(command, "SAFE", 4)) {
    char ip[16], tcp[6];
    if (sscanf(command + 5, "%15s %5s", ip, tcp) == 2) {
      ndn_safe(node, ip, tcp);
      processed = true;
    } else {
      fprintf(stderr, ERR "Invalid SAFE format: %s\n", command);
    }
  } else if (!memcmp(command, "INTEREST", 8)) {
    char object[101];
    if (sscanf(command, "INTEREST %100s", object) == 1) {
      ndn_interest(node, object, fd);
      ndn_show_interest_table(node);
      processed = true;
    } else {
      fprintf(stderr, ERR "Invalid INTEREST format\n");
    }
  } else if (!memcmp(command, "OBJECT", 6)) {
    char object[101];
    if (sscanf(command, "OBJECT %100s", object) == 1) {
      ndn_object(node, object, fd);
      ndn_show_interest_table(node);
      processed = true;
    } else {
      fprintf(stderr, ERR "Invalid OBJECT format\n");
    }
  } else if (!memcmp(command, "NOOBJECT", 8)) {
    char object[101];
    if (sscanf(command, "NOOBJECT %100s", object) == 1) {
      ndn_noobject(node, object, fd);
      ndn_show_interest_table(node);
      processed = true;
    } else {
      fprintf(stderr, ERR "Invalid NOOBJECT format\n");
    }
  } else {
    fprintf(stderr, ERR "Unknown command: %s\n", command);
  }

  return processed;
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

        node->net = 1000;
        node->in_net = true;

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
    if (!node->in_net) {
      fprintf(stderr, ERR "Not in a network\n");
      return;
    }

    if (!is_valid_name(name)) {
      fprintf(stderr, ERR "Invalid name (alphanumeric, 1-100 chars)\n");
      return;
    }

    ndn_create(node, name);

    return;
  }
  //----------

  //---delete---
  if ((sscanf(input, "delete %100s%n", name, &pos) == 1 &&
       input[pos] == '\0') ||
      (sscanf(input, "dl %100s%n", name, &pos) == 1 && input[pos] == '\0')) {
    if (!node->in_net) {
      fprintf(stderr, ERR "Not in a network\n");
      return;
    }

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
    if (!node->in_net) {
      fprintf(stderr, ERR "Not in a network\n");
      return;
    }

    if (!is_valid_name(name)) {
      fprintf(stderr, ERR "Invalid name (alphanumeric, 1-100 chars)\n");
      return;
    }

    ndn_retrieve(node, name);
    ndn_show_interest_table(node);

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
    if (!node->in_net) {
      fprintf(stderr, ERR "Not in a network\n");
      return;
    }

    printf(OK "Leaving network\n");
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
