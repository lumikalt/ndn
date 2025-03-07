#include "tcp.h"
#include "../util.h"

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ndn_safe(Node *node, char *ip, char *tcp) {
  // Create and connect the TCP socket to ip:tcp
  int fd, errcode;
  ssize_t n;
  struct addrinfo hints, *res;
  char buffer[128];

  node->safeguard->ip = ip;
  node->safeguard->tcp = tcp;

  printf("OK: Got external's (%s:%s) external (%s:%s)\n", node->external->ip,
         node->external->tcp, ip, tcp);

  printf("OK: Connecting to safeguard\n");

  fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd == -1) {
    errored("ERR: Failed to create the socket", node);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  errcode = getaddrinfo(ip, tcp, &hints, &res);
  if (errcode != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errcode));
    exit(1);
  }
  n = connect(fd, res->ai_addr, res->ai_addrlen);
  if (n == -1) {
    errored("ERR: Failed to connect to the safeguard", node);
  }

  node->safeguard->fd = fd;
  node->safeguard->addr = res;
}

void ndn_entry(Node *node, char* ip, char *tcp) {

}
