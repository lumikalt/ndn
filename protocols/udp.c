#include "udp.h"
#include "../util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/// Request the list of nodes in the network
NodeList *ndn_nodes(Node *node, u16 net) {
  Server *s = node->server;
  NodeList *nodes = malloc(10 * sizeof(NodeList));
  nodes->size = 0;
  nodes->capacity = 10;

  ssize_t n;
  char buffer[256];

  sprintf(buffer, "NODES %03d", net);

  if ((n = sendto(s->fd, buffer, strlen(buffer), 0, s->addr->ai_addr,
                  s->addr->ai_addrlen)) <= 0)
    errored("ERR: Failed to send the NODES request",
            node); // Changed to errored
  else
    printf("OK: Requested nodes for net %03d\n", net);

  /* Wait for the OK */

  char ok[17];
  sprintf(ok, "NODESLIST %03d\n", net);

  char response[15];
  if ((n = recvfrom(s->fd, response, sizeof(response), 0, s->addr->ai_addr,
                    &s->addr->ai_addrlen)) <= 0)
    errored("ERR: No response from the server", node); // Changed to errored
  else if (strncmp(response, ok, 15) != 0)
    printf("ERR: Server response did not match the spec");
  else
    printf("OK: Receiving nodes in net %d\n", net);

  /* Get the nodes */

  // char buffer[256];
  char *line = malloc(521); // Buffer to store incomplete lines
  size_t line_len = 0;      // Length of the incomplete line
  char *ptr;
  while (true) {
    // Check if there are no more bytes to read
    n = recvfrom(s->fd, buffer, sizeof(buffer) - 1, 0, s->addr->ai_addr,
                 &s->addr->ai_addrlen);
    if (n <= 0) {
      if (n == 0)
        printf("OK: Finished reading nodes\n");
      else
        errored("ERR: No response from the server", node); // Changed to errored

      break;
    }

    buffer[n] = '\0'; // Null-terminate the received data

    // Handle all but the last line, which may be incomplete
    usize line_num = str_char_count(buffer, '\n');

    for (usize j = 0; j < line_num - 1; j++) {
      ptr = strchr(buffer, '\n');
      *ptr = '\0';

      nodes->size++;

      if (nodes->size > nodes->capacity) {
        nodes->capacity *= 2;
        nodes->ip = realloc(nodes->ip, nodes->capacity * sizeof(char *));
        nodes->tcp = realloc(nodes->tcp, nodes->capacity * sizeof(char *));
      }

      nodes->ip[nodes->size - 1] = malloc(256);
      nodes->tcp[nodes->size - 1] = malloc(10);

      sscanf(buffer, "%s %s", nodes->ip[nodes->size - 1],
             nodes->tcp[nodes->size - 1]);
    }

    // Handle the last line, which may be incomplete
    ptr = strchr(buffer, '\n');
    if (ptr == NULL) {
      // If the buffer does not contain a newline character, it is an incomplete
      // line
      usize buffer_len = strlen(buffer);
      if (line_len + buffer_len > 520) {
        errored("ERR: Incomplete line is too long", node); // Changed to errored
        break;
      }

      memcpy(line + line_len, buffer, buffer_len);
      line_len += buffer_len;
    } else {
      // If the buffer contains a newline character, it is a complete line
      *ptr = '\0';

      nodes->size++;

      if (nodes->size > nodes->capacity) {
        nodes->capacity *= 2;
        nodes->ip = realloc(nodes->ip, nodes->capacity * sizeof(char *));
        nodes->tcp = realloc(nodes->tcp, nodes->capacity * sizeof(char *));
      }

      nodes->ip[nodes->size - 1] = malloc(256);
      nodes->tcp[nodes->size - 1] = malloc(10);

      sscanf(buffer, "%s %s", nodes->ip[nodes->size - 1],
             nodes->tcp[nodes->size - 1]);

      // Copy the incomplete line to the beginning of the buffer
      memcpy(buffer, ptr + 1, strlen(ptr + 1));
      line_len = 0;
    }
  }

  free(line);

  return nodes;
}

/// Register the node in the network, and check if the server accepted the node
/// entry.
void ndn_register(Node *node, u16 net) {
  ssize_t n;
  char buffer[256];
  Server *s = node->server;

  sprintf(buffer, "REG %03d %s %s", net, node->ip, node->tcp);

  if ((n = sendto(s->fd, buffer, sizeof(buffer), 0, s->addr->ai_addr,
                  s->addr->ai_addrlen)) <= 0)
    errored("ERR: Failed to send the join request", node); // Changed to errored
  else
    printf("OK: Requested join to net %03d\n", net);

  /* Wait for the OK */

  const char *ok = "OKREG";
  char response[6];

  if (n = recvfrom(s->fd, response, sizeof(response), 0, s->addr->ai_addr,
                   &s->addr->ai_addrlen),
      n <= 0)
    errored("ERR: No response from the server", node); // Changed to errored
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
    errored("ERR: Failed to send the leave request",
            node); // Changed to errored
  else
    printf("OK: Requested leave from net %03d\n", net);

  /* Wait for the OK */

  const char *ok = "OKUNREG";
  char response[8];

  if ((n = recvfrom(s->fd, response, sizeof(response), 0, s->addr->ai_addr,
                    &s->addr->ai_addrlen)) <= 0)
    errored("ERR: No response from the server", node); // Changed to errored
  else if (strncmp(response, ok, 8) != 0)
    printf("ERR: Server refused the connection to net %03d\n", net);
  else
    printf("OK: Successfully left network %d\n", net);
}
