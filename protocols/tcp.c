#include "tcp.h"
#include "../util.h"

#include <netdb.h>
#include <stdio.h>
#include <string.h>

void ndn_safe(Node *node, char *ip, char *tcp) {
  // Create and connect the TCP socket to ip:tcp
  int fd, errcode;
  ssize_t n;
  struct addrinfo hints, *res;

  if (strcmp(node->external->ip, ip) == 0 &&
      strcmp(node->external->tcp, tcp) == 0) {
    fprintf(stderr, ERR "Entry contains joining node's own details\n");
    return;
  }
  if (strcmp(node->ip, ip) == 0 && strcmp(node->tcp, tcp) == 0) {
    fprintf(stderr, ERR "Entry contains joining node's own details\n");
    return;
  }

  node->safeguard->ip = ip;
  node->safeguard->tcp = tcp;

  printf(NOTICE "Got external's (%s:%s) external (%s:%s)\n", node->external->ip,
         node->external->tcp, ip, tcp);

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
}

void ndn_entry(Node *node, char *ip, char *tcp) {
  // Create and connect the TCP socket to ip:tcp
  int fd, errcode;
  ssize_t n;
  struct addrinfo hints, *res;

  if (strcmp(node->ip, ip) == 0 && strcmp(node->tcp, tcp) == 0) {
    fprintf(stderr, ERR "Entry contains joining node's own details\n");
    return;
  }

  node->internal[node->internal_size]->ip = ip;
  node->internal[node->internal_size]->tcp = tcp;

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
    node->external->ip = ip;
    node->external->tcp = tcp;
  }

  printf(OK "Connected to new internal %s:%s\n", ip, tcp);

  node->internal_size++;
}
