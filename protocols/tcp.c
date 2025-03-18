#include "tcp.h"
#include "../util.h"

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Change function signature
void ndn_safe(Node *node, char *ip, char *tcp) {
  // Create and connect the TCP socket to ip:tcp
  int fd, errcode;
  ssize_t n;
  struct addrinfo hints, *res;

  // Error checks
  if (ip == NULL || tcp == NULL) {
    fprintf(stderr, ERR "Invalid SAFE message format\n");
    return;
  }

  node->safeguard->ip = strdup(ip);
  node->safeguard->tcp = strdup(tcp);

  printf(NOTICE "Got external's (%s:%s) external (%s:%s)\n", node->external->ip,
         node->external->tcp, ip, tcp);

  if (strcmp(node->external->ip, ip) == 0 &&
      strcmp(node->external->tcp, tcp) == 0) {
    printf(NOTICE "SAFE contains external node's own details\n");
    return;
  }

  if (strcmp(node->ip, ip) == 0 && strcmp(node->tcp, tcp) == 0) {
    printf(NOTICE "SAFE contains this node's own details\n");
    return;
  }

  printf(NOTICE "Connecting to safeguard\n");

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, ERR "Failed to create the socket\n");
    return;
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((errcode = getaddrinfo(ip, tcp, &hints, &res)) != 0) {
    fprintf(stderr, ERR "Failed to get the address info (%s)\n",
            gai_strerror(errcode));
    return;
  }

  if ((n = connect(fd, res->ai_addr, res->ai_addrlen)) == -1) {
    fprintf(stderr, ERR "Failed to connect to the safeguard\n");
    return;
  }

  node->safeguard->fd = fd;
  node->safeguard->addr = res;

  printf(OK "Connected to safeguard\n");

  return;
}

void ndn_entry(Node *node, char *ip, char *tcp) {
  // Create and connect the TCP socket to ip:tcp
  int fd, errcode;
  ssize_t n;
  struct addrinfo hints, *res;

  // Initialize node->internal if it's NULL
  if (node->internal == NULL) {
    node->internal = malloc(10 * sizeof(AdjacentNode *)); // Start with 10 slots
    if (!node->internal) {
      fprintf(stderr, ERR "Failed to allocate internal nodes array\n");
      return;
    }
    node->internal_size = 0;

    // Initialize all pointers to NULL
    for (int j = 0; j < 10; j++) {
      node->internal[j] = NULL;
    }
  }
  else if (node->internal_size == node->internal_capacity) {
    // Grow the array
    node->internal = realloc(node->internal, 2 * node->internal_capacity * sizeof(AdjacentNode *));
    if (!node->internal) {
      fprintf(stderr, ERR "Failed to reallocate internal nodes array\n");
      return;
    }
    node->internal_capacity *= 2;
  }

  // Allocate a new AdjacentNode for this connection
  node->internal[node->internal_size] = malloc(sizeof(AdjacentNode));
  if (!node->internal[node->internal_size]) {
    fprintf(stderr, ERR "Failed to allocate internal node\n");
    return;
  }

  node->internal[node->internal_size]->ip = strdup(ip);
  node->internal[node->internal_size]->tcp = strdup(tcp);

  if (strcmp(node->ip, ip) == 0 && strcmp(node->tcp, tcp) == 0) {
    fprintf(stderr, ERR "Entry contains joining node's own details\n");
    return;
  }

  printf(NOTICE "Connecting to new internal %s:%s\n", ip, tcp);

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, ERR "Failed to create the socket\n");
    return;
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((errcode = getaddrinfo(ip, tcp, &hints, &res)) != 0) {
    fprintf(stderr, ERR "Failed to get the address info\n");
    return;
  }

  if ((n = connect(fd, res->ai_addr, res->ai_addrlen)) == -1) {
    fprintf(stderr, ERR "Failed to connect to the new internal\n");
    return;
  }

  node->internal[node->internal_size]->fd = fd;
  node->internal[node->internal_size]->addr = res;

  if (node->external->fd == -1) { // No external yet
    node->external->fd = fd;
    node->external->addr = res;
    node->external->ip = strdup(ip);
    node->external->tcp = strdup(tcp);

    printf(NOTICE "No external yet, chose this connection\n");
  }

  // Send SAFE to the new internal
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "SAFE %s %s\n", node->external->ip,
           node->external->tcp);

  if (write(fd, buffer, strlen(buffer)) == -1) {
    fprintf(stderr, ERR "Failed to send SAFE to the new internal\n");
    return;
  }
  printf(NOTICE "Sent SAFE to new internal\n");

  printf(OK "Connected to new internal %s:%s\n", ip, tcp);

  node->internal_size++;
}
