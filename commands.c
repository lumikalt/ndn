#include "commands.h"
#include "list.h"
#include "protocols/udp.h"
#include "types.h"
#include "util.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void ndn_help() {

  printf("Commands:\n"
         "\t(h)  help - show this message\n"
         "\t(j)  join <net> - join the network (000-999)\n"
         "\t(dj) direct join <net> <IP> <TCP> - directly join a network\n"
         "\t(c)  create <name> - create an object\n"
         "\t(dl) delete <name> - delete an object\n"
         "\t(r)  retrieve <name> - re<trieve an object\n"
         "\t(st) show_topology - show the neighbourhood's topology\n"
         "\t(sn) show_names - show the object names in this node\n"
         "\t(si) show_interest_table - show the interest table\n"
         "\t(l)  leave - leave the network\n"
         "\t(x)  exit - close the program\n");
}

void ndn_join(Node *node, u16 net) {
  printf("Joining network %03d\n", net);

  NodeList *network = ndn_nodes(node, net);
  if (network->size == 0) {
    printf("Lone node, waiting for others\n");
    return;
  }

  // Connect to a random node in the network
  int node_id = rand() % network->size;
  char *node_ip = network->ip[node_id];
  char *node_tcp = network->tcp[node_id];

  printf("Attempting connection to %s:%s\n", node_ip, node_tcp);

  // Create TCP socket
  int node_port = socket(AF_INET, SOCK_STREAM, 0);
  if (node_port < 0) {
    errored("Failed to create socket", node);
    return;
  }

  // node setup
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  // Resolve the node IP and TCP port using getaddrinfo
  int status = getaddrinfo(node_ip, node_tcp, &hints, &res);
  if (status != 0) {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    close(node_port);
    return;
  }

  if (connect(node_port, res->ai_addr, res->ai_addrlen) < 0) {
    perror("Connection to nearby node failed");
    close(node_port);
    freeaddrinfo(res);
    return;
  }

  printf("Connected to %s:%s\n", node_ip, node_tcp);

  node->external->ip = node_ip;
  node->external->tcp = node_tcp;
  node->external->addr = res;
  node->external->fd = node_port;
}

void ndn_direct_join(Node *node, u16 net, char *connectIP, char *connectTCP) {
  printf("Directly joining network %d, linking to %s:%s\n", net, connectIP,
         connectTCP);
}

void ndn_create(Node *node, const char *name) {
  printf("Creating object %s\n", name);

  Object object = malloc(strlen(name) + 1);
  strcpy(object, name);

  list_add(node->objects, object, NULL, NULL);
}

void ndn_delete(Node *node, const char *name) {
  printf("Deleting object %s\n", name);

  list_remove(node->objects, (char *)name);
}

void ndn_retrieve(Node *node, const char *name) {
  printf("Retrieving object %s\n", name);

  ObjectList *object = list_find(node->objects, (char *)name);
  if (object != NULL) {
    printf("Already in node\n");
    return;
  }

  printf("Not in node, requesting to adjacent nodes\n");

  // TODO: Send interest to adjacent nodes
}

void ndn_show_topology(Node *node) {
  printf("Network topology:\n");
  printf("\tSafeguard: %s:%s\n", node->safeguard->ip, node->safeguard->tcp);
  printf("\tExternal: %s:%s\n", node->external->ip, node->external->tcp);
  printf("\tInternal:\n");
  for (usize i = 0; i < node->network->size; i++) {
    printf("\t\t%s:%s\n", node->network->ip[i], node->network->tcp[i]);
  }
}

void ndn_show_names(Node *node) {
  printf("Owned:\n");
  list_print(node->objects);

  // print the cache
  printf("Cached:\n");
  for (usize i = 0; i < node->cache_size; i++) {
    if (node->cache[i] != NULL) {
      printf("\t%s\n", node->cache[i]);
    }
  }
}

void ndn_show_interest_table(Node *node) {
  printf("Interest:\n");
  list_print_interests(node->interests);
}

void ndn_leave(Node *node) { printf("Leaving network\n"); }

// void ndn_exit(Node *node) { printf("Exiting program\n"); } // Select handles
// this
