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
  char buffer[128];

  node->safeguard->ip = ip;
  node->safeguard->tcp = tcp;

  printf(CYAN "NOTICE" CLEAR "\tGot external's (%s:%s) external (%s:%s)\n",
         node->external->ip, node->external->tcp, ip, tcp);

  printf(CYAN "NOTICE" CLEAR "\tConnecting to safeguard\n");

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, RED "ERR" CLEAR "\tFailed to create the socket\n");
    return;
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((errcode = getaddrinfo(ip, tcp, &hints, &res)) != 0) {
    fprintf(stderr, RED "ERR" CLEAR "\tFailed to get the address info (%s)\n",
            gai_strerror(errcode));
    return;
  }

  if ((n = connect(fd, res->ai_addr, res->ai_addrlen)) == -1) {
    fprintf(stderr, RED "ERR" CLEAR "\tFailed to connect to the safeguard\n");
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
  char buffer[128];

  node->external->ip = ip;
  node->external->tcp = tcp;

  printf(CYAN "NOTICE" CLEAR "\tConnecting to new internal %s:%s\n", ip, tcp);

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, RED "ERR" CLEAR "\tFailed to create the socket\n");
    return;
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((errcode = getaddrinfo(ip, tcp, &hints, &res)) != 0) {
    fprintf(stderr, RED "ERR" CLEAR "\tFailed to get the address info\n");
    return;
  }

  if ((n = connect(fd, res->ai_addr, res->ai_addrlen)) == -1) {
    fprintf(stderr,
            RED "ERR" CLEAR "\tFailed to connect to the new internal\n");
    return;
  }

  printf(GREEN "OK" CLEAR "\tConnected to new internal %s:%s\n", ip, tcp);

  node->external->fd = fd;
  node->external->addr = res;
}
