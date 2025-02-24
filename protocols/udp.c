#include "udp.h"
#include "../util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// Request the list of nodes in the network
NodeList *ndn_nodes(Node *node, u16 net) {
  Server *s = node->server;
  NodeList *nodes = malloc(10 * sizeof(NodeList));
  nodes->size = 0;
  nodes->capacity = 10;

  ssize_t n;
  char buffer[256];

  sprintf(buffer, "NODES %03d", net);

  if (n = sendto(s->fd, buffer, strlen(buffer), 0, s->udp->ai_addr, s->udp->ai_addrlen), n <= 0)
    perror("ERR: Failed to send the NODES request"); // TODO: clean up
  else
    printf("OK: Requested nodes for net %03d\n", net);

  /* Wait for the OK */

  char ok[15];
  sprintf(ok, "NODESLIST %03d\n", net);

  char response[15];
  if (n = recvfrom(s->fd, response, sizeof(response), 0, s->udp->ai_addr, &s->udp->ai_addrlen),n <= 0)
    perror("ERR: No response from the server"); // TODO: clean up
  else if (strncmp(response, ok, 15) != 0)
    printf("ERR: Server response did not match the spec");
  else
    printf("OK: Receiving nodes in net %d\n", net);

  /* Get the nodes */

  // char buffer[256];
  char line[512];      // Buffer to store incomplete lines
  size_t line_len = 0; // Length of the incomplete line
  char *ptr;
  usize i = 0;
  while (true) {
    // Check if there are no more bytes to read
    n = recvfrom(s->fd, buffer, sizeof(buffer) - 1, 0, s->udp->ai_addr, &s->udp->ai_addrlen);
    if (n <= 0) {
      if (n == 0)
        printf("OK: Finished reading nodes\n");
      else
        perror("ERR: No response from the server"); // TODO: clean up

      break;
    }

    buffer[n] = '\0'; // Null-terminate the received data

    // Handle all but the last line, which may be incomplete
    usize line_num = str_char_count(buffer, '\n');

    for (usize j = 0; j < line_num; j++) {
      ptr = strchr(buffer, '\n');
      *ptr = '\0';

      if (i > nodes->capacity) {
        nodes->capacity *= 2;
        nodes->IP = realloc(nodes->IP, nodes->capacity * sizeof(char *));
        nodes->TCP = realloc(nodes->TCP, nodes->capacity * sizeof(char *));
      }

      nodes->IP[i] = malloc(16);
      nodes->TCP[i] = malloc(6);

      sscanf(buffer, "%s %s", nodes->IP[i], nodes->TCP[i]);

      i++;
    }
  }

  return nodes;
}


/// Register the node in the network, and check if the server accepted the node
/// entry.
void ndn_register(Node *node, u16 net) {
  ssize_t n;
  char buffer[256];
  Server *s = node->server;

  sprintf(buffer, "REG %03d %s %s", net, s->IP, s->TCP);

  if (n = sendto(s->fd, buffer, sizeof(buffer), 0, s->udp->ai_addr, s->udp->ai_addrlen),n <= 0)
    perror("ERR: Failed to send the join request"); // TODO: clean up
  else
    printf("OK: Requested join to net %03d\n", net);

  /* Wait for the OK */

  const char *ok = "OKREG";
  char response[6];

  if (n = recvfrom(s->fd, response, sizeof(response), 0, s->udp->ai_addr, &s->udp->ai_addrlen), n <= 0)
    perror("ERR: No response from the server"); // TODO: clean up
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

  sprintf(buffer, "UNREG %03d %s %s", net, s->IP, s->TCP);

  if (n = sendto(s->fd, buffer, sizeof(buffer), 0, s->udp->ai_addr,
                 s->udp->ai_addrlen),
      n <= 0)
    perror("ERR: Failed to send the leave request"); // TODO: clean up
  else
    printf("OK: Requested leave from net %03d\n", net);

  /* Wait for the OK */

  const char *ok = "OKUNREG";
  char response[8];

  if (n = recvfrom(s->fd, response, sizeof(response), 0, s->udp->ai_addr,
                   &s->udp->ai_addrlen),
      n <= 0)
    perror("ERR: No response from the server"); // TODO: clean up
  else if (strncmp(response, ok, 8) != 0)
    printf("ERR: Server refused the connection to net %03d\n", net);
  else
    printf("OK: Successfully left network %d\n", net);
}
