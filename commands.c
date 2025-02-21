#include "./commands.h"
#include "./types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ndn_help() {
  printf("Commands:\n"
         "\t(h)  help - show this message\n"
         "\t(j)  join <net> - join the network (000-999)\n"
         "\t(dj) direct join <net> <IP> <TCP> - directly join a network\n"
         "\t(c)  create <name> - create an object\n"
         "\t(dl) delete <name> - delete an object\n"
         "\t(r)  retrieve <name> - retrieve an object\n"
         "\t(st) show_topology - show the neighbourhood's topology\n"
         "\t(sn) show_names - show the object names in this node\n"
         "\t(si) show_interest_table - show the interest table\n"
         "\t(l)  leave - leave the network\n"
         "\t(x)  exit - close the program\n");
}

void ndn_join(Server *s, u16 net) {}

void ndn_direct_join(u16 net, char *connectIP, char *connectTCP) {
  printf("Directly joining network %d, linking to %s:%s\n", net, connectIP,
         connectTCP);
}

void ndn_create(const char *name) { printf("Creating object %s\n", name); }

void ndn_delete(const char *name) { printf("Deleting object %s\n", name); }

void ndn_retrieve(const char *name) { printf("Retrieving object %s\n", name); }

void ndn_show_topology() { printf("Showing network topology\n"); }

void ndn_show_names() { printf("Showing object names\n"); }

void ndn_show_interest_table() { printf("Showing interest table\n"); }

void ndn_leave() { printf("Leaving network\n"); }

void ndn_exit() { printf("Exiting program\n"); }

/* Server Comms internals */

typedef struct {
  char *IP;
  char *TCP;

  // Sadly this needs to be dynamic, the response doesn't include the number of
  // nodes
  usize size;
  usize capacity;
} NetNode;

/// Request the list of nodes in the network
NetNode *ndn_nodes(Server *s, u16 net) {
  NetNode *nodes = malloc(10 * sizeof(NetNode));
  nodes->size = 0;
  nodes->capacity = 10;

  ssize_t n;
  char buffer[256];

  sprintf(buffer, "NODES %03d", net);

  if (n = sendto(s->serverfd, buffer, sizeof(buffer), 0, s->udp->ai_addr,
                 s->udp->ai_addrlen),
      n <= 0)
    perror("ERR: Failed to send the NODES request"); // TODO: clean up
  else
    printf("OK: Requested nodes for net %03d\n", net);

  /* Wait for the OK */

  char ok[15];
  sprintf(ok, "NODESLIST %03d\n", net);

  char response[15];
  if (n = recvfrom(s->serverfd, response, sizeof(response), 0, s->udp->ai_addr,
                   &s->udp->ai_addrlen),
      n <= 0)
    perror("ERR: No response from the server"); // TODO: clean up
  else if (strncmp(response, ok, 15) != 0)
    printf("ERR: Server response did not match the spec");
  else
    printf("OK: Receiving nodes in net %d\n", net);

  /* Get the nodes */

  while (1) {
    char response[256];
    // Check if there are no more bytes to read
    n = recvfrom(s->serverfd, response, sizeof(response), 0, s->udp->ai_addr,
                 &s->udp->ai_addrlen);
    if (n <= 0) {
      if (n == 0) {
        printf("OK: Finished reading nodes\n");
      } else {
        perror("ERR: No response from the server"); // TODO: clean up
      }
      break;
    }

    // Parse the node
    char *IP = strtok(response, " ");
    char *TCP = strtok(NULL, " ");

    // Check if we need to resize the array
    if (nodes->size == nodes->capacity) {
      nodes->capacity *= 2;
      nodes = realloc(nodes, nodes->capacity * sizeof(NetNode));
    }

    // Add the node to the array
    strcpy(nodes[nodes->size].IP, IP);
    strcpy(nodes[nodes->size].TCP, TCP);
    nodes->size++;
  }

  return nodes;
}

/// Register the node in the network, and check if the server accepted the node
/// entry.
void ndn_register(Server *s, u16 net) {
  ssize_t n;
  char buffer[256];

  sprintf(buffer, "REG %03d %s %s", net, s->IP, s->TCP);

  if (n = sendto(s->serverfd, buffer, sizeof(buffer), 0, s->udp->ai_addr,
                 s->udp->ai_addrlen),
      n <= 0)
    perror("ERR: Failed to send the join request"); // TODO: clean up
  else
    printf("OK: Requested join to net %03d\n", net);

  /* Wait for the OK */

  const char *ok = "OKREG";
  char response[6];

  if (n = recvfrom(s->serverfd, response, sizeof(response), 0, s->udp->ai_addr,
                   &s->udp->ai_addrlen),
      n <= 0)
    perror("ERR: No response from the server"); // TODO: clean up
  else if (strncmp(response, ok, 6) != 0)
    printf("ERR: Server refused the connection to net %03d\n", net);
  else
    printf("OK: Successfully joined network %d\n", net);
}

/// Unregister the node from the network, and check if the server accepted the
/// node exit.
void ndn_unregister(Server *s, Adjacencies *adj, u16 net) {
  ssize_t n;
  char buffer[256];

  sprintf(buffer, "UNREG %03d %s %s", net, s->IP, s->TCP);

  if (n = sendto(s->serverfd, buffer, sizeof(buffer), 0, s->udp->ai_addr,
                 s->udp->ai_addrlen),
      n <= 0)
    perror("ERR: Failed to send the leave request"); // TODO: clean up
  else
    printf("OK: Requested leave from net %03d\n", net);

  /* Wait for the OK */

  const char *ok = "OKUNREG";
  char response[8];

  if (n = recvfrom(s->serverfd, response, sizeof(response), 0, s->udp->ai_addr,
                   &s->udp->ai_addrlen),
      n <= 0)
    perror("ERR: No response from the server"); // TODO: clean up
  else if (strncmp(response, ok, 8) != 0)
    printf("ERR: Server refused the connection to net %03d\n", net);
  else
    printf("OK: Successfully left network %d\n", net);
}
