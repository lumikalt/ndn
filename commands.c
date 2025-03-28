#include "commands.h"
#include "list.h"
#include "protocols/tcp.h"
#include "protocols/udp.h"
#include "types.h"
#include "util.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

void ndn_help() {
  printf(NOTICE
         "Commands:\n"
         "\t(h)  help - show this message\n"
         "\t(j)  join <net> - join the network (000-999)\n"
         "\t(dj) direct join <net> <IP> <TCP> - directly join a network\n"
         "\t(c)  create <name> - create an object\n"
         "\t(dl) delete <name> - delete an object\n"
         "\t(r)  retrieve <name> - retrieve an object\n"
         "\t(st) show_topology - show the neighbourhood's topology\n"
         "\t(sn) show_names - show the object names in this node\n"
         "\t(si) show_interest_table - show the interest table\n"
         "\t(l)  leave - leave the network\n"
         "\t(x)  exit - close the program\n");
}

void ndn_join(Node *node, u16 net) {
  char *node_ip = NULL;
  char *node_tcp = NULL;

  if (node->in_net) {
    fprintf(stderr, ERR "Already in a network\n");
    return;
  }

  node->net = net;

  NodeList *network = ndn_nodes(node);
  if (!network) {
    fprintf(stderr, ERR "Failed to get the network list\n");
    return;
  }

  if (network->size == 0) {
    clean_nodelist(network);
    ndn_register(node);
    node->in_net = true;
    printf(OK "Lone node, waiting for others\n");

    // Set self as external
    if (node->external->ip) {
      free(node->external->ip);
    }
    if (node->external->tcp) {
      free(node->external->tcp);
    }

    node->external->ip = strdup(node->ip);
    node->external->tcp = strdup(node->tcp);
    node->external->fd = -1;

    return;
  }

  // Connect to a random node in the network
  // Reseed the random number generator
  srand(time(NULL));
  int node_id = rand() % network->size;
  node_ip = strdup(network->ip[node_id]);
  node_tcp = strdup(network->tcp[node_id]);

  if (!node_ip || !node_tcp) {
    perror(ERR "strdup");
    if (node_ip)
      free(node_ip);
    if (node_tcp)
      free(node_tcp);
    clean_nodelist(network);
    return;
  }

  clean_nodelist(network);

  printf(NOTICE "External %s:%s chosen, attempting connection\n", node_ip,
         node_tcp);

  // Create TCP socket
  int external_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (!is_valid_fd(external_fd)) {
    perror(ERR "socket");
    free(node_ip);
    free(node_tcp);
    return;
  }

  // Node setup
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  // Resolve the node IP and TCP port using getaddrinfo
  int status = getaddrinfo(node_ip, node_tcp, &hints, &res);
  if (status != 0) {
    fprintf(stderr, ERR "getaddrinfo error: %s\n", gai_strerror(status));
    close(external_fd);
    free(node_ip);
    free(node_tcp);
    return;
  }

  if (connect(external_fd, res->ai_addr, res->ai_addrlen) < 0) {
    perror(ERR "Connection to nearby node failed");
    close(external_fd);
    freeaddrinfo(res);
    free(node_ip);
    free(node_tcp);
    return;
  }

  printf(NOTICE "Connected to external %s:%s\n", node_ip, node_tcp);
  freeaddrinfo(res);

  if (node->external->ip) {
    free(node->external->ip);
  }
  if (node->external->tcp) {
    free(node->external->tcp);
  }

  node->external->ip = node_ip;
  node->external->tcp = node_tcp;
  node->external->fd = external_fd;

  // ENTRY message
  char buffer[128];
  ssize_t n;
  snprintf(buffer, sizeof(buffer), "ENTRY %s %s\n", node->ip, node->tcp);

  if ((n = write(external_fd, buffer, strlen(buffer))) < 0) {
    perror(ERR "write");
    close(external_fd);
    free(node_ip);
    free(node_tcp);
    return;
  }

  // Read responses (which may include multiple messages)
  memset(buffer, 0, sizeof(buffer));
  if ((n = read(external_fd, buffer, sizeof(buffer) - 1)) <= 0) {
    perror(ERR "read");
    close(external_fd);

    // Free the memory before nullifying the pointers
    free(node->external->ip);
    free(node->external->tcp);

    node->external->ip = NULL;
    node->external->tcp = NULL;
    node->external->fd = -1;
    return;
  }
  buffer[n] = '\0';
  char *escaped = str_escape(buffer);
  printf(MAGENTA "fd_%02d" RESET "\t%s\n", external_fd, escaped);
  free(escaped);

  // Process each message separated by newline
  char *next_msg = strtok(buffer, "\n");
  bool safe_found = false;
  while (next_msg != NULL) {
    // Check if the message is a SAFE message
    if (strncmp(next_msg, "SAFE", 4) == 0) {
      char ip[16], tcp[6];
      if (sscanf(next_msg, "SAFE %15s %5s", ip, tcp) == 2) {
        ndn_safe(node, ip, tcp);
        safe_found = true;
      }
    } else {
      // Queue other messages for later processing by ndn_run
      // (Add to node->last_msgs for the external_fd)
      if (external_fd < (i64)node->last_msgs_capacity) {
        if (node->last_msgs[external_fd]) {
          free(node->last_msgs[external_fd]);
        }
        node->last_msgs[external_fd] = strdup(next_msg);
      }
      printf(NOTICE "Queued message for later processing: %s\n", next_msg);
    }
    next_msg = strtok(NULL, "\n");
  }

  if (!safe_found) {
    fprintf(stderr, ERR "No SAFE message received\n");
    close(external_fd);

    // Free memory before nullifying pointers
    if (node->external->ip) {
      free(node->external->ip);
    }
    if (node->external->tcp) {
      free(node->external->tcp);
    }

    node->external->ip = NULL;
    node->external->tcp = NULL;
    node->external->fd = -1;
    return;
  }

  // Now register the node with the network
  printf(OK "Registering node\n");
  ndn_register(node);
  node->in_net = true;
  node->net = net;
  printf(OK "Joined network %03d\n", net);
}

void ndn_direct_join(Node *node, char *ip, char *tcp) {
  int fd, errcode;
  struct addrinfo hints, *res;
  struct timeval timeout;
  fd_set write_fds;
  int flags;

  printf(NOTICE "Attempting to connect with %s:%s\n", ip, tcp);

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror(ERR "socket");
    return;
  }

  // Set socket to non-blocking mode
  flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((errcode = getaddrinfo(ip, tcp, &hints, &res)) != 0) {
    fprintf(stderr, ERR "Failed to get address info (%s)\n",
            gai_strerror(errcode));
    close(fd);
    return;
  }

  // Try to connect (will return immediately in non-blocking mode)
  int result = connect(fd, res->ai_addr, res->ai_addrlen);

  if (result < 0 && errno != EINPROGRESS) {
    // Immediate failure
    perror(ERR "connect");
    freeaddrinfo(res);
    close(fd);
    return;
  }

  // Set up for select - wait for connection to complete
  FD_ZERO(&write_fds);
  FD_SET(fd, &write_fds);

  // Set timeout to 3 seconds
  timeout.tv_sec = 3;
  timeout.tv_usec = 0;

  // Wait for connection to complete
  result = select(fd + 1, NULL, &write_fds, NULL, &timeout);

  if (result <= 0) {
    if (result == 0) {
      fprintf(stderr, ERR "Connection timed out\n");
    } else {
      perror(ERR "select");
    }
    freeaddrinfo(res);
    close(fd);
    return;
  }

  // Check if connection succeeded
  int error = 0;
  socklen_t len = sizeof(error);

  // Get the error value from getsockopt - CRITICAL step
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
    perror(ERR "getsockopt");
    freeaddrinfo(res);
    close(fd);
    return;
  }

  // Check if there was an actual connection error
  if (error != 0) {
    fprintf(stderr, ERR "Connection failed: %s (error %d)\n", strerror(error),
            error);
    freeaddrinfo(res);
    close(fd);
    return;
  }

  // Add a verification step - try to send a small byte
  struct timeval short_timeout;
  short_timeout.tv_sec = 1;
  short_timeout.tv_usec = 0;
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &short_timeout,
             sizeof(short_timeout));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &short_timeout,
             sizeof(short_timeout));

  const char *test_msg = "TEST\n";
  if (send(fd, test_msg, strlen(test_msg), MSG_NOSIGNAL) < 0) {
    fprintf(stderr, ERR "Connection verification failed: %s\n",
            strerror(errno));
    freeaddrinfo(res);
    close(fd);
    return;
  }

  char test_buffer[8];
  if (recv(fd, test_buffer, sizeof(test_buffer), MSG_PEEK | MSG_DONTWAIT) < 0 &&
      errno != EAGAIN && errno != EWOULDBLOCK) {
    fprintf(stderr, ERR "Connection verification failed: %s\n",
            strerror(errno));
    freeaddrinfo(res);
    close(fd);
    return;
  }

  // Set socket back to blocking mode
  fcntl(fd, F_SETFL, flags);

  freeaddrinfo(res);

  printf(NOTICE "Connected to external %s:%s\n", ip, tcp);

  // Rest of your function remains the same
  ssize_t n;
  char buffer[128];

  // Set up external node information
  node->external->ip = strdup(ip);
  node->external->tcp = strdup(tcp);
  node->external->fd = fd;

  // Send ENTRY message
  sprintf(buffer, "ENTRY %s %s\n", node->ip, node->tcp);
  if ((n = write(fd, buffer, strlen(buffer))) < 0) {
    perror(ERR "write");
    close(fd);
    free(node->external->ip);
    free(node->external->tcp);
    node->external->ip = NULL;
    node->external->tcp = NULL;
    node->external->fd = -1;
    return;
  }

  // Read response(s)
  memset(buffer, 0, sizeof(buffer));
  if ((n = read(fd, buffer, sizeof(buffer) - 1)) <= 0) {
    perror(ERR "read");
    close(fd);
    free(node->external->ip);
    free(node->external->tcp);
    node->external->ip = NULL;
    node->external->tcp = NULL;
    node->external->fd = -1;
    return;
  }

  buffer[n] = '\0';
  char *escaped = str_escape(buffer);
  printf(MAGENTA "fd_%02d" RESET "\t%s\n", fd, escaped);
  free(escaped);

  // Process each message separated by newline
  char *next_msg = strtok(buffer, "\n");
  bool safe_found = false;
  while (next_msg != NULL) {
    // Check if the message is a SAFE message
    if (strncmp(next_msg, "SAFE", 4) == 0) {
      char ip[16], tcp[6];
      if (sscanf(next_msg, "SAFE %15s %5s", ip, tcp) == 2) {
        ndn_safe(node, ip, tcp);
        safe_found = true;
      }
    } else {
      // Queue other messages for later processing by ndn_run
      // (Add to node->last_msgs for the fd)
      if (fd < (i64)node->last_msgs_capacity) {
        if (node->last_msgs[fd]) {
          free(node->last_msgs[fd]);
        }
        node->last_msgs[fd] = strdup(next_msg);
      }
      printf(NOTICE "Queued message for later processing: %s\n", next_msg);
    }
    next_msg = strtok(NULL, "\n");
  }

  if (!safe_found) {
    fprintf(stderr, ERR "No SAFE message received\n");
    close(fd);
    free(node->external->ip);
    free(node->external->tcp);
    node->external->ip = NULL;
    node->external->tcp = NULL;
    node->external->fd = -1;
    return;
  }

  node->in_net = true;
  printf(OK "Directly joined network of external %s:%s\n", ip, tcp);
}

void ndn_create(Node *node, const char *name) {
  // Check if name is valid
  if (!is_valid_name((char *)name)) {
    fprintf(stderr, ERR "Invalid object name '%s'\n", name);
    return;
  }

  // Check if object already exists
  if (list_find(node->objects, (char *)name)) {
    fprintf(stderr, ERR "Object '%s' already exists\n", name);
    return;
  }

  printf(NOTICE "Creating object %s\n", name);

  // Add to objects list - make sure this always happens
  list_add(node->objects, (char *)name, 0, 0);

  printf(OK "Created object `%s`\n", name);
}

void ndn_delete(Node *node, const char *name) {
  printf(NOTICE "Deleting object %s\n", name);

  list_remove(node->objects, (Object)name);
}

void ndn_retrieve(Node *node, const char *name) {
  printf(NOTICE "Retrieving object %s\n", name);

  ObjectList *object = list_find(node->objects, (char *)name);
  if (object != NULL) {
    printf(OK "Already in node\n");
    return;
  }

  // Check if we have the object in cache
  for (usize i = 0; i < node->cache_count; i++) {
    usize pos = (node->cache_head + i) % node->cache_size;
    if (node->cache[pos] && strcmp(node->cache[pos], name) == 0) {
      printf(OK "Found in cache\n");
      return;
    }
  }

  printf(NOTICE "Not in node, requesting to adjacent nodes\n");

  // Check if we're alone in the network - immediately fail if so
  bool alone = true;

  // Check if we have any active internal connections
  for (usize i = 0; i < node->internal_index; i++) {
    if (node->internal[i] && node->internal[i]->fd > 0) {
      alone = false;
      break;
    }
  }

  // Check if external is connected and not just self-reference
  if (node->external->fd > 0) {
    alone = false;
  }

  // If we're alone, fail immediately
  if (alone) {
    printf(ERR "No other nodes in network, object '%s' cannot be retrieved\n",
           name);
    return;
  }

  // Prepare the INTEREST message
  list_add(node->interests, (Object)name, -1, 0);
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "INTEREST %s\n", name);

  // Send the INTEREST message to all non-safe nodes
  int external_fd = node->external->fd;
  if (external_fd != -1) {
    if (write(external_fd, buffer, strlen(buffer)) < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Socket buffer full, not fatal
        printf(WARN "Socket buffer full, continuing anyway\n");
      } else {
        perror(ERR "write");
      }
    }
    list_add(node->interests, (Object)name, 0, external_fd);
  }

  for (usize i = 0; i < node->internal_index; i++) {
    if (node->internal[i]->fd == -1 || node->internal[i]->fd == external_fd) {
      continue;
    }

    if (write(node->internal[i]->fd, buffer, strlen(buffer)) < 0) {
      perror(ERR "write");
    }
    list_add(node->interests, (Object)name, 0, node->internal[i]->fd);
  }

  printf(OK "Interest sent\n");

  // Return to main loop - we'll check for the response there
}

void ndn_show_topology(Node *node) {
  printf(NOTICE "Network topology:\n");
  printf(RESET "\tSafeguard -> %s:%s\n", node->safeguard->ip,
         node->safeguard->tcp);
  printf(RESET "\tExternal  -> %s:%s\n", node->external->ip,
         node->external->tcp);
  printf(RESET "\tInternals:\n");
  for (usize i = 0; i < node->internal_index; i++) {
    printf(RESET "\t-> %s:%s\n", node->internal[i]->ip, node->internal[i]->tcp);
  }
}

void ndn_show_names(Node *node) {
  printf(NOTICE "Owned:\n");
  list_print(node->objects);

  // Print the cache from oldest to newest
  printf(NOTICE "Cached:\n");

  if (node->cache_count == 0) {
    printf(RESET "\t(empty)\n");
  } else {
    // Start from the oldest item (cache_head) and iterate through all items
    for (usize i = 0; i < node->cache_count; i++) {
      // Calculate the position in the circular buffer
      usize pos = (node->cache_head + i) % node->cache_size;

      if (node->cache[pos]) {
        // Show the age of the item (0 = oldest)
        printf(RESET "\t[%zu] `%s`%s\n", i, node->cache[pos],
               (i == 0)                       ? " (oldest)"
               : (i == node->cache_count - 1) ? " (newest)"
                                              : "");
      }
    }
  }
}

void ndn_show_interest_table(Node *node) {
  // Skip the sentinel head node
  ObjectList *interest = node->interests->next;
  if (interest == NULL) {
    printf(CYAN "(empty)\n" RESET);
    return;
  }

  // Find longest object name for formatting
  usize longest_name = 6; // Minimum length for "Object"
  ObjectList *scan = node->interests->next;
  while (scan != NULL) {
    if (scan->self && strlen(scan->self) > longest_name) {
      longest_name = strlen(scan->self);
    }
    scan = scan->next;
  }

  // Collect all file descriptors we need to display
  int *fds = NULL;
  int fds_count = 0;
  int fds_capacity = 10;

  fds = malloc(fds_capacity * sizeof(int));
  if (!fds) {
    perror(ERR "malloc");
    return;
  }

  // Include external node if connected
  if (node->external->fd != -1) {
    fds[fds_count++] = node->external->fd;
  }

  // Include all internal nodes
  for (usize i = 0; i < node->internal_index; i++) {
    if (node->internal[i]->fd != -1) {
      // Check if this fd already exists in our array
      if (fd_exists_in_array(node->internal[i]->fd, fds, fds_count)) {
        continue; // Skip duplicates
      }

      // Check if we need to expand the array
      if (fds_count >= fds_capacity) {
        fds_capacity *= 2;
        fds = realloc(fds, fds_capacity * sizeof(int));
        if (!fds) {
          perror(ERR "realloc");
          return;
        }
      }

      fds[fds_count++] = node->internal[i]->fd;
    }
  }

  if (fds_count == 0) {
    printf(RESET "\t(no connected nodes)\n");
    free(fds);
    return;
  }

  // Print header row
  printf(CYAN "%-*s |", (int)longest_name, "Object");
  for (int i = 0; i < fds_count; i++) {
    printf(" %02d |", fds[i]);
  }
  printf(RESET "\n");

  // Print separator
  for (usize i = 0; i < longest_name; i++)
    printf("-");
  printf("-+");
  for (int i = 0; i < fds_count; i++) {
    printf("----+");
  }
  printf("\n");

  // Print each interest row
  while (interest != NULL) {
    if (!interest->self) {
      interest = interest->next;
      continue;
    }

    // Print object name
    printf(YELLOW "%-*s" RESET " |", (int)longest_name, interest->self);

    // Print status for each file descriptor
    for (int i = 0; i < fds_count; i++) {
      int fd = fds[i];
      bool is_by = false;
      bool is_to = false;

      // Check if this fd is in the by array (requested by)
      if (interest->response && interest->response_size > 0) {
        for (usize j = 0; j < interest->response_size; j++) {
          if (interest->response[j] == fd) {
            is_by = true;
            break;
          }
        }
      }

      // Check if this fd is in the to array (waiting for)
      if (interest->waiting && interest->waiting_size > 0) {
        for (usize j = 0; j < interest->waiting_size; j++) {
          if (interest->waiting[j] == fd) {
            is_to = true;
            break;
          }
        }
      }

      // Print appropriate marker
      if (is_by) {
        printf(GREEN " r" RESET "  |"); // r = response (requested by)
      } else if (is_to) {
        printf(BLUE " w" RESET "  |"); // w = waiting for
      } else {
        printf(RESET " c" RESET "  |"); // c = closed (not involved)
      }
    }
    printf(RESET "\n");

    interest = interest->next;
  }

  // Print legend
  printf("\nLegend: ");
  printf(GREEN "r" RESET " = response, ");
  printf(BLUE "w" RESET " = waiting, ");
  printf(RESET "c = closed\n");

  free(fds);
}

void ndn_leave(Node *node) {
  if (!node->in_net) {
    fprintf(stderr, ERR "Not in a network\n");
    return;
  }

  // Unregister from the network if we're in a valid net
  if (node->net < 1000) {
    ndn_unregister(node);
  }

  // Close external connection
  if (node->external) {
    if (node->external->ip) {
      free(node->external->ip);
      node->external->ip = NULL;
    }
    if (node->external->tcp) {
      free(node->external->tcp);
      node->external->tcp = NULL;
    }
    if (node->external->fd != -1) {
      close(node->external->fd);
      node->external->fd = -1;
    }
  }

  // Close safeguard connection
  if (node->safeguard) {
    if (node->safeguard->ip) {
      free(node->safeguard->ip);
      node->safeguard->ip = NULL;
    }
    if (node->safeguard->tcp) {
      free(node->safeguard->tcp);
      node->safeguard->tcp = NULL;
    }
    if (node->safeguard->fd != -1) {
      close(node->safeguard->fd);
      node->safeguard->fd = -1;
    }
  }

  // Clean up internal connections
  for (usize i = 0; i < node->internal_index; i++) {
    if (node->internal[i]) {
      if (node->internal[i]->ip) {
        free(node->internal[i]->ip);
        node->internal[i]->ip = NULL;
      }
      if (node->internal[i]->tcp) {
        free(node->internal[i]->tcp);
        node->internal[i]->tcp = NULL;
      }
      if (node->internal[i]->fd != -1) {
        close(node->internal[i]->fd);
        node->internal[i]->fd = -1;
      }
      free(node->internal[i]);
      node->internal[i] = NULL;
    }
  }
  node->internal_index = 0;

  // Clear objects & interests
  list_destroy(node->objects);
  list_destroy(node->interests);
  node->objects = list_create();
  node->interests = list_create();

  // Reset network state
  node->in_net = false;
  node->net = 1000;

  printf(OK "Leave completed\n");
}
