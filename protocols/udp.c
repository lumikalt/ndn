#include "udp.h"
#include "../util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/// Request the list of nodes in the network
NodeList *ndn_nodes(Node *node, u16 net) {
  Server *s = node->server;
  NodeList *nodes = malloc(sizeof(NodeList));
  nodes->ip = malloc(10 * sizeof(char *));
  nodes->tcp = malloc(10 * sizeof(char *));
  nodes->size = 0;
  nodes->capacity = 10;

  ssize_t n;
  char buffer[256];

  sprintf(buffer, "NODES %03d", net);

  if ((n = sendto(s->fd, buffer, strlen(buffer), 0, s->addr->ai_addr,
                  s->addr->ai_addrlen)) <= 0) {
    fprintf(stderr, RED "ERR" CLEAR "\tFailed to send the nodes request\n");
    return NULL;
  }

  printf(CYAN "NOTICE" CLEAR "\tRequested nodes for net %03d\n", net);

  /* Wait for the OK */

  char *response = calloc(4096, sizeof(char));

  char ok[15];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-overflow"
  sprintf(ok, "NODESLIST %03d\n", net);
#pragma GCC diagnostic pop

  if ((n = recvfrom(s->fd, response, 4096, 0, s->addr->ai_addr,
                    &s->addr->ai_addrlen)) <= 0) {
    fprintf(stderr, RED "ERR" CLEAR "\tNo response from the server\n");
    free(response);
    return NULL;
  }

  if (strncmp(response, ok, 14) != 0) {
    fprintf(stderr,
            RED "ERR" CLEAR "\tServer response did not match the spec\n");
    free(response);
    return NULL;
  }

  char *string = response + 14;

  printf(CYAN "NOTICE" CLEAR "\tParsing nodes\n");

  /* Parse the string */

  usize newlines = str_char_count(string, '\n');
  for (usize i = 0; i < newlines; i++) {
    // Every line is in the format IP TCP\n
    char ip[100], tcp[6];
    sscanf(string, "%s %s\n", ip, tcp);
    string += strlen(ip) + strlen(tcp) + 2;

    if (nodes->size == nodes->capacity) {
      nodes->capacity *= 2;
      nodes->ip = realloc(nodes->ip, nodes->capacity * sizeof(char *));
      nodes->tcp = realloc(nodes->tcp, nodes->capacity * sizeof(char *));
    }

    nodes->ip[nodes->size] = malloc(strlen(ip) + 1);
    nodes->tcp[nodes->size] = malloc(strlen(tcp) + 1);
    strcpy(nodes->ip[nodes->size], ip);
    strcpy(nodes->tcp[nodes->size], tcp);

    nodes->size++;
  }

  free(response);

  return nodes;
}

/// Register the node in the network, and check if the server accepted the
/// node entry.
void ndn_register(Node *node, u16 net) {
  ssize_t n;
  char buffer[256];
  Server *s = node->server;

  sprintf(buffer, "REG %03d %s %s", net, node->ip, node->tcp);

  if ((n = sendto(s->fd, buffer, sizeof(buffer), 0, s->addr->ai_addr,
                  s->addr->ai_addrlen)) <= 0) {
    fprintf(stderr, RED "ERR" CLEAR "\tFailed to send the join request\n");
    return;
  }

  printf(CYAN "NOTICE" CLEAR "\tRequested registration in net %03d\n", net);

  /* Wait for the OK */

  const char *ok = "OKREG";
  char response[6];

  if ((n = recvfrom(s->fd, response, sizeof(response), 0, s->addr->ai_addr,
                    &s->addr->ai_addrlen)) <= 0) {
    fprintf(stderr, RED "ERR" CLEAR "\tNo response from the server\n");
    return;
  } else if (strncmp(response, ok, 6) != 0) {
    fprintf(stderr, RED "ERR" CLEAR "\tServer refused the registration\n");
  }

  printf(GREEN "OK" CLEAR "\tSuccessfully registered in the network %d\n", net);
}

/// Unregister the node from the network, and check if the server accepted the
/// node exit.
void ndn_unregister(Node *node, u16 net) {
  ssize_t n;
  char buffer[256];
  Server *s = node->server;

  sprintf(buffer, "UNREG %03d %s %s", net, node->ip, node->tcp);

  if ((n = sendto(s->fd, buffer, sizeof(buffer), 0, s->addr->ai_addr,
                  s->addr->ai_addrlen)) <= 0) {
    fprintf(stderr,
            RED "ERR" CLEAR "\tFailed to send unregistration request\n");
    return;
  }

  printf(CYAN "NOTICE" CLEAR "\tRequested unregistration from net %03d\n", net);

  /* Wait for the OK */

  const char *ok = "OKUNREG";
  char response[8];

  if ((n = recvfrom(s->fd, response, sizeof(response), 0, s->addr->ai_addr,
                    &s->addr->ai_addrlen)) <= 0) {
    fprintf(stderr, RED "ERR" CLEAR "\tNo response from the server\n");
    return;
  } else if (strncmp(response, ok, 8) != 0)
    printf(RED "ERR" CLEAR "\tServer refused the connection to net %03d\n",
           net);

  printf(GREEN "OK" CLEAR "\tSuccessfully left network %d\n", net);
}
