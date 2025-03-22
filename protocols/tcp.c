#include "tcp.h"
#include "../util.h"

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

  printf(NOTICE "Got external (%s:%s)'s external (%s:%s)\n", node->external->ip,
         node->external->tcp, ip, tcp);

  if (strcmp(node->external->ip, ip) == 0 &&
      strcmp(node->external->tcp, tcp) == 0) {
    printf(WARN "SAFE contains external node's own details\n");
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

  freeaddrinfo(res);

  node->safeguard->fd = fd;

  printf(OK "Connected to safeguard\n");

  return;
}

void ndn_entry(Node *node, char *ip, char *tcp, int enteringfd) {
  char buffer[128];

  grow_internal(node);
  node->internal[node->internal_index] = malloc(sizeof(AdjacentNode));
  if (!node->internal[node->internal_index]) {
    fprintf(stderr, ERR "Failed to allocate internal node\n");
    return;
  }

  node->internal[node->internal_index]->ip = strdup(ip);
  node->internal[node->internal_index]->tcp = strdup(tcp);
  node->internal[node->internal_index]->fd = enteringfd;
  node->internal_index++;

  printf(OK "Added new internal %s:%s\n", ip, tcp);

  // Check if we need an external
  if (node->external->fd == -1) {
    printf(NOTICE "No external yet, choosing this connection\n");

    node->external->ip = strdup(ip);
    node->external->tcp = strdup(tcp);
    node->external->fd = enteringfd;

    sprintf(buffer, "SAFE %s %s\n",
            node->external->ip ? node->external->ip : "0.0.0.0",
            node->external->tcp ? node->external->tcp : "0");

    if (write(enteringfd, buffer, strlen(buffer)) < 0) {
      perror(ERR "writing SAFE");
    }

    sprintf(buffer, "ENTRY %s %s\n", node->ip, node->tcp);
    if (write(enteringfd, buffer, strlen(buffer)) < 0) {
      perror(ERR "writing ENTRY");
    } else {
      printf(NOTICE "Sending it an ENTRY message\n");
    }

    return;
  }

  // Se já tiver externo
  sprintf(buffer, "SAFE %s %s\n",
          node->external->ip ? node->external->ip : "0.0.0.0",
          node->external->tcp ? node->external->tcp : "0");

  if (write(enteringfd, buffer, strlen(buffer)) < 0) {
    perror(ERR "writing SAFE");
  }

  printf(NOTICE "Sent safeguard\n");

  // enviar msg de salvaguarda por fd;
  // return;
}

/* Object */

void ndn_interest(Node *node, Object object, int fd) {
  char buffer[128];

  // Check if we have the object
  if (list_find(node->objects, object)) { // we got it
    printf(NOTICE "Object belongs to self... ");

    sprintf(buffer, "OBJECT %s\n", object);
    if (write(fd, buffer, strlen(buffer)) < 0) {
      perror("\n" ERR "writing OBJECT");
    }

    printf("sent\n");

    return;
  }

  // Check if we have it cached
  for (usize i = 0; i < node->cache_size; i++) {
    if (strcmp(node->cache[i], object) == 0) { // its cached
      printf(NOTICE "Object is cached... ");

      sprintf(buffer, "OBJECT %s\n", object);
      if (write(fd, buffer, strlen(buffer)) < 0) {
        perror("\n" ERR "writing OBJECT");
      }

      printf("sent\n");

      return;
    }
  }

  // check if fd is our only connection
  if (node->external->fd == fd &&
      (node->internal_index == 0 ||
       (node->internal_index == 1 && node->internal[0]->fd == fd))) {
    printf(NOTICE "No other connections, can't help\n");

    sprintf(buffer, "NOOBJECT %s\n", object);
    if (write(fd, buffer, strlen(buffer)) < 0) {
      perror("\n" ERR "writing NOOBJECT");
    }

    return;
  }

  // Request it from the network

  printf(NOTICE "Requesting object from network\n");

  // Add to interest list
  list_add(node->interests, object, fd);

  sprintf(buffer, "INTEREST %s\n", object);

  // Ask all internals
  for (usize i = 0; i < node->internal_index; i++) {
    if (node->internal[i]->fd == fd) {
      continue;
    }

    if (write(node->internal[i]->fd, buffer, strlen(buffer)) < 0) {
      perror("\n" ERR "writing INTEREST");
    }

    printf(NOTICE "Sent INTEREST to %s:%s\n", node->internal[i]->ip,
           node->internal[i]->tcp);
  }

  // Ask external
  if (node->external->fd != fd) {
    if (write(node->external->fd, buffer, strlen(buffer)) < 0) {
      perror("\n" ERR "writing INTEREST");
    }

    printf(NOTICE "Sent INTEREST to external\n");
  }
}

