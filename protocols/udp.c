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
                  s->addr->ai_addrlen)) <= 0)
    errored("ERR: Failed to send the NODES request", node);

  printf("OK: Requested nodes for net %03d\n", net);

  /* Wait for the OK */

  char *response = calloc(4096, sizeof(char));

  char ok[15];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-overflow"
  sprintf(ok, "NODESLIST %03d\n", net);
#pragma GCC diagnostic pop

  if ((n = recvfrom(s->fd, response, 4096, 0, s->addr->ai_addr,
                    &s->addr->ai_addrlen)) <= 0)
    errored("ERR: No response from the server", node);

  printf("OK: Response (%zu): %s\n", n, response);

  if (strncmp(response, ok, 14) != 0)
    errored("ERR: Server response did not match the spec", node);

  char *string = response + 14;

  printf("List: %s\n", string);

  printf("OK: Parsing nodes\n");

  /* Parse the string */

  usize len = strlen(string);

  for (usize i = 0; i < str_char_count(string, '\n'); i++) {
    char *ip = strtok(string, " ");
    char *tcp = strtok(NULL, " ");

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
                  s->addr->ai_addrlen)) <= 0)
    errored("ERR: Failed to send the join request", node);
  else
    printf("OK: Requested join to net %03d\n", net);

  /* Wait for the OK */

  const char *ok = "OKREG";
  char response[6];

  if (n = recvfrom(s->fd, response, sizeof(response), 0, s->addr->ai_addr,
                   &s->addr->ai_addrlen),
      n <= 0)
    errored("ERR: No response from the server", node);
  else if (strncmp(response, ok, 6) != 0)
    printf("ERR: Server refused the connection to net %03d\n", net);
  else
    printf("OK: Successfully joined network %d\n", net);
}

/// Unregister the node from the network, and check if the server accepted the
/// node exit.
void ndn_unregister(Node *node, u16 net) {
  ssize_t n;
  char buffer[256];
  Server *s = node->server;

  sprintf(buffer, "UNREG %03d %s %s", net, node->ip, node->tcp);

  if ((n = sendto(s->fd, buffer, sizeof(buffer), 0, s->addr->ai_addr,
                  s->addr->ai_addrlen)) <= 0)
    errored("ERR: Failed to send the leave request", node);
  else
    printf("OK: Requested leave from net %03d\n", net);

  /* Wait for the OK */

  const char *ok = "OKUNREG";
  char response[8];

  if ((n = recvfrom(s->fd, response, sizeof(response), 0, s->addr->ai_addr,
                    &s->addr->ai_addrlen)) <= 0)
    errored("ERR: No response from the server", node);
  else if (strncmp(response, ok, 8) != 0)
    printf("ERR: Server refused the connection to net %03d\n", net);
  else
    printf("OK: Successfully left network %d\n", net);
}
