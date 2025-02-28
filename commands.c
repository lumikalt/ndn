#include "commands.h"
#include "list.h"
#include "protocols/udp.h"
#include "types.h"

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

  NodeList *network = ndn_nodes((Node *)node->server, net);
  struct addrinfo hints, *res;

  if (network->size == 0) {
    printf("Lone node, waiting for others\n");
  } else {
    // Connect to a random node in the network
    int node_id = rand() % network->size;
    char *node_ip = network->ip[node_id];
    char *node_tcp = network->tcp[node_id];

    printf("Attempting connection to %s:%s\n", node_ip, node_tcp);

    // TCP port string to integer
    int node_port = atoi(node_tcp);
    if (node_port <= 0) {
      return;
    }

    // create TCP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
      printf("Failed to create socket");
      return;
    }

    // node setup
    struct sockaddr_in node_addr;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (connect(sockfd, (struct sockaddr *)&node_addr, sizeof(node_addr)) < 0) {
      printf("Connection to nearby node failed");
      close(sockfd);
      return;
    }
  }

  ndn_register(node, net);

  freeaddrinfo(res);
}

void ndn_direct_join(Node *node, u16 net, char *connectIP, char *connectTCP) {
  printf("Directly joining network %d, linking to %s:%s\n", net, connectIP,
         connectTCP);
}

void ndn_create(Node *node, const char *name) {
  printf("Creating object %s\n", name);

  Object object = malloc(strlen(name) + 1);
  strcpy(object, name);

  list_add(node->objects, object, -1);
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

void ndn_show_topology(Node *node) { printf("Showing network topology\n"); }

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

void ndn_show_interest_table(Node *node) { printf("Showing interest table\n"); }

void ndn_leave(Node *node) { printf("Leaving network\n"); }

void ndn_exit(Node *node) { printf("Exiting program\n"); }
