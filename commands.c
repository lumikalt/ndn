#include "commands.h"
#include "list.h"
#include "protocols/tcp.h"
#include "protocols/udp.h"
#include "types.h"
#include "util.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

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

  clean_nodelist(network);

  printf(NOTICE "External %s:%s chosen, attempting connection\n", node_ip,
         node_tcp);

  // Create TCP socket
  int external_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (external_fd < 0) {
    fprintf(stderr, ERR "Failed to create socket\n");
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
    free(node_ip);
    free(node_tcp);
    return;
  }
  buffer[n] = '\0';
  char *escaped = str_escape(buffer);
  printf(MAGENTA "fd_%02d" RESET "\t%s\n", external_fd, escaped);
  free(escaped);

  // Process each message separated by newline
  char *next_msg = strtok(buffer, "\n");
  int safe_found = 0;
  while (next_msg != NULL) {
    // Check if the message is a SAFE message
    if (strncmp(next_msg, "SAFE", 4) == 0) {
      char ip[16], tcp[6];
      if (sscanf(next_msg, "SAFE %15s %5s", ip, tcp) == 2) {
        ndn_safe(node, ip, tcp);
        safe_found = 1;
      } else {
        fprintf(stderr, ERR "Invalid SAFE message: %s\n", next_msg);
        close(external_fd);
        free(node_ip);
        free(node_tcp);
        return;
      }
    } else {
      // Process or ignore other messages (e.g., ENTRY) as needed
      printf(NOTICE "Ignoring message: %s\n", next_msg);
    }
    next_msg = strtok(NULL, "\n");
  }

  if (!safe_found) {
    fprintf(stderr, ERR "No SAFE message received\n");
    close(external_fd);
    free(node_ip);
    free(node_tcp);
    return;
  }

  // Now register the node with the network
  printf(OK "Registering node\n");
  ndn_register(node);
  node->in_net = true;
  node->net = net;
  printf(OK "Joined network %03d\n", net);
}

void ndn_direct_join(Node *node, char *connectIP, char *connectTCP) {
  if (node->in_net) {
    fprintf(stderr, ERR "Already in a network, `(l)eave` first\n");
    return;
  }

  int external_fd, errcode;
  ssize_t n;
  struct addrinfo hints, *res;
  char buffer[128];

  // Create and connect the TCP socket to connectIP:connectTCP
  if ((external_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror(ERR "socket");
    return;
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((errcode = getaddrinfo(connectIP, connectTCP, &hints, &res)) != 0) {
    fprintf(stderr, ERR "getaddrinfo: %s\n", gai_strerror(errcode));
    close(external_fd);
    return;
  }

  printf(NOTICE "Attempting to connect with %s:%s\n", connectIP, connectTCP);

  if ((n = connect(external_fd, res->ai_addr, res->ai_addrlen)) == -1) {
    perror(ERR "connect");
    close(external_fd);
    freeaddrinfo(res);
    return;
  }

  printf(NOTICE "Connected to external %s:%s\n", connectIP, connectTCP);

  freeaddrinfo(res);

  // Set up external node information
  node->external->ip = strdup(connectIP);
  node->external->tcp = strdup(connectTCP);
  node->external->fd = external_fd;

  // Send ENTRY message
  sprintf(buffer, "ENTRY %s %s\n", node->ip, node->tcp);
  if ((n = write(external_fd, buffer, strlen(buffer))) < 0) {
    perror(ERR "write");
    close(external_fd);
    free(node->external->ip);
    free(node->external->tcp);
    node->external->ip = NULL;
    node->external->tcp = NULL;
    node->external->fd = -1;
    return;
  }

  // Read SAFE response
  memset(buffer, 0, sizeof(buffer));
  if ((n = read(external_fd, buffer, sizeof(buffer) - 1)) <= 0) {
    perror(ERR "read");
    close(external_fd);
    free(node->external->ip);
    free(node->external->tcp);
    node->external->ip = NULL;
    node->external->tcp = NULL;
    node->external->fd = -1;
    return;
  }

  // Process SAFE response
  buffer[n] = '\0';
  char *escaped = str_escape(buffer);
  printf(MAGENTA "fd_%02d" RESET "\t%s\n", external_fd, escaped);
  free(escaped);

  // Parse and handle SAFE
  char ip[16], tcp[6];
  if (sscanf(buffer, "SAFE %15s %5s", ip, tcp) == 2) {
    ndn_safe(node, ip, tcp);
  } else {
    fprintf(stderr, ERR "Invalid SAFE response: %s\n", buffer);
    close(external_fd);
    freeaddrinfo(res);
    free(node->external->ip);
    free(node->external->tcp);
    node->external->ip = NULL;
    node->external->tcp = NULL;
    node->external->fd = -1;
    return;
  }

  node->in_net = true;
  printf(OK "Directly joined network of external %s:%s\n", connectIP,
         connectTCP);
}

void ndn_create(Node *node, const char *name) {
  printf(NOTICE "Creating object %s\n", name);

  Object object = malloc(strlen(name) + 1);
  strcpy(object, name);

  list_add(node->objects, object, 0, 0);
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

  // Send interest to adjacent nodes

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
    list_add(node->interests, (Object)name, 0, i);
  }

  printf(OK "Interest sent\n");

  // Set up a retrieval request with timeout
  time_t start_time = time(NULL);
  node->current_retrieval = strdup(name);
  node->retrieval_start_time = start_time;

  printf(NOTICE "Waiting for responses (timeout: %d seconds)\n",
         node->retrieval_timeout);

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
    printf(RESET "\t->%s:%s\n", node->internal[i]->ip, node->internal[i]->tcp);
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
  printf(NOTICE "Interests: [requested by / requested from]\n");
  list_print_interests(node->external->fd, node->interests);
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
