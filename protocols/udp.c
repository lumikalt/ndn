#include "udp.h"
#include "../util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void set_udp_timeout(int sockfd, int timeout_sec) {
  struct timeval tv;
  tv.tv_sec = timeout_sec;
  tv.tv_usec = 0;

  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    perror(ERR "setsockopt timeout");
  }
}

/// Request the list of nodes in the network
NodeList *ndn_nodes(Node *node) {
  Server *s = node->server;
  u16 net = node->net;
  NodeList *nodes = malloc(sizeof(NodeList));
  if (!nodes) {
    perror(ERR "malloc");
    return NULL;
  }
  nodes->size = 0;
  nodes->ip = NULL;
  nodes->tcp = NULL;

  ssize_t n;
  char buffer[256];

  sprintf(buffer, "NODES %03d", net);

  // Set a 3-second timeout for UDP responses
  set_udp_timeout(s->fd, 3);

  if ((n = sendto(s->fd, buffer, strlen(buffer), 0, s->addr->ai_addr,
                  s->addr->ai_addrlen)) <= 0) {
    fprintf(stderr, ERR "Failed to send the nodes request\n");
    clean_nodelist(nodes);
    return NULL;
  }

  printf(NOTICE "Requested nodes for net %03d\n", net);

  /* Wait for the OK */

  char *response = calloc(4096, sizeof(char));
  if (!response) {
    perror(ERR "calloc");
    clean_nodelist(nodes);
    return NULL;
  }

  char ok[15];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-overflow"
  sprintf(ok, "NODESLIST %03d\n", net);
#pragma GCC diagnostic pop

  if ((n = recvfrom(s->fd, response, 4096, 0, s->addr->ai_addr,
                    &s->addr->ai_addrlen)) <= 0) {
    fprintf(stderr, ERR "No response from the server (timeout or error)\n");
    free(response);
    clean_nodelist(nodes);
    return NULL;
  }

  if (strncmp(response, ok, 14) != 0) {
    fprintf(stderr, ERR "Server response did not match the spec\n");
    free(response);
    clean_nodelist(nodes);
    return NULL;
  }

  char *string = response + 14;

  printf(NOTICE "Parsing nodes\n");

  /* Parse the string */

  usize newlines = str_char_count(string, '\n');

  nodes->ip = malloc(newlines * sizeof(char *));
  nodes->tcp = malloc(newlines * sizeof(char *));
  if (!nodes->ip || !nodes->tcp) {
    perror(ERR "malloc");
    free(response);
    clean_nodelist(nodes);
    return NULL;
  }

  for (usize i = 0; i < newlines; i++) {
    // Every line is in the format IP TCP\n
    char ip[100], tcp[6];
    sscanf(string, "%s %s\n", ip, tcp);
    string += strlen(ip) + strlen(tcp) + 2;

    nodes->ip[nodes->size] = malloc(strlen(ip) + 1);
    nodes->tcp[nodes->size] = malloc(strlen(tcp) + 1);
    if (!nodes->ip[nodes->size] || !nodes->tcp[nodes->size]) {
      perror(ERR "malloc");
      free(response);
      clean_nodelist(nodes);
      return NULL;
    }
    strcpy(nodes->ip[nodes->size], ip);
    strcpy(nodes->tcp[nodes->size], tcp);

    nodes->size++;
  }

  free(response);

  return nodes;
}

/// Register the node in the network, and check if the server accepted the
/// node entry.
void ndn_register(Node *node) {
  ssize_t n;
  char buffer[256];
  Server *s = node->server;
  u16 net = node->net;

  sprintf(buffer, "REG %03u %s %s", net, node->ip, node->tcp);

  printf(NOTICE "Requesting registration in net %03u\n", net);

  if ((n = sendto(s->fd, buffer, strlen(buffer), 0, s->addr->ai_addr,
                  s->addr->ai_addrlen)) <= 0) {
    fprintf(stderr, ERR "Failed to send the join request\n");
    return;
  }

  /* Wait for the OK */

  const char *ok = "OKREG";
  char response[6];

  if ((n = recvfrom(s->fd, response, sizeof(response), 0, s->addr->ai_addr,
                    &s->addr->ai_addrlen)) <= 0) {
    fprintf(stderr, ERR "No response from the server\n");
    return;
  } else if (strncmp(response, ok, 6) != 0) {
    fprintf(stderr, ERR "Server refused the registration\n");
  }

  printf(OK "Successfully registered\n");
}

/// Unregister the node from the network, and check if the server accepted the
/// node exit.
void ndn_unregister(Node *node) {
  ssize_t n;
  char buffer[256];
  Server *s = node->server;

  sprintf(buffer, "UNREG %03zu %s %s", node->net, node->ip, node->tcp);

  if ((n = sendto(s->fd, buffer, strlen(buffer), 0, s->addr->ai_addr,
                  s->addr->ai_addrlen)) <= 0) {
    fprintf(stderr, ERR "Failed to send unregistration request\n");
    return;
  }

  printf(NOTICE "Requested unregistration\n");

  /* Wait for the OK */

  const char *ok = "OKUNREG";
  char response[8];

  if ((n = recvfrom(s->fd, response, sizeof(response), 0, s->addr->ai_addr,
                    &s->addr->ai_addrlen)) <= 0) {
    fprintf(stderr, ERR "No response from the server\n");
    return;
  } else if (strncmp(response, ok, 8) != 0) {
    fprintf(stderr, ERR "Server refused the connection to net %03zu\n",
            node->net);
  }

  printf(OK "Successfully left network %zu\n", node->net);

  node->in_net = false;
  node->net = 1000;
}
