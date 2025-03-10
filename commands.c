#include "commands.h"
#include "list.h"
#include "protocols/tcp.h"
#include "protocols/udp.h"
#include "types.h"
#include "util.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void ndn_help() {
  printf(NOTICE
         "Commands:\n"
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

void ndn_join(Node *node, u16 net) {
  if (node->in_net) {
    fprintf(stderr, ERR "Already in a network\n");
    return;
  }

  NodeList *network = ndn_nodes(node, net);
  if (network->size == 0) {

    clean_nodelist(network);

    ndn_register(node, net);

    node->in_net = true;

    printf(OK "Lone node, waiting for others\n");
    return;
  }

  // Connect to a random node in the network
  int node_id = rand() % network->size;
  char *node_ip = strdup(network->ip[node_id]);
  char *node_tcp = strdup(network->tcp[node_id]);

  clean_nodelist(network);

  printf(NOTICE "External %s:%s chosen, attempting connection\n", node_ip,
         node_tcp);

  // Create TCP socket
  int external_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (external_fd < 0) {
    fprintf(stderr, ERR "Failed to create socket\n");
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
    fprintf(stderr, ERR "getaddrinfo error: %s\n", gai_strerror(status));
    close(external_fd);
    return;
  }

  if (connect(external_fd, res->ai_addr, res->ai_addrlen) < 0) {
    perror(ERR "Connection to nearby node failed");
    close(external_fd);
    freeaddrinfo(res);
    return;
  }

  printf(NOTICE "Connected to external %s:%s\n", node_ip, node_tcp);

  node->external->ip = node_ip;
  node->external->tcp = node_tcp;
  node->external->addr = res;
  node->external->fd = external_fd;

  // ENTRY

  char buffer[128];
  ssize_t n;

  snprintf(buffer, sizeof(buffer), "ENTRY %s %s\n", node->ip, node->tcp);

  if ((n = write(external_fd, buffer, strlen(buffer))) < 0) {
    perror(ERR "write");
    return;
  }

  // wait for the SAFE response
  memset(buffer, 0, sizeof(buffer));
  if ((n = read(external_fd, buffer, sizeof(buffer) - 1)) < 0) {
    perror(ERR "read");
    return;
  }

  // check "SAFE IP TCP\n"
  char *ip = NULL, *tcp = NULL;
  if (sscanf(buffer, "SAFE %s %s\n", ip, tcp) != 2) {
    fprintf(stderr, ERR "Failed to parse response\n");
    return;
  }

  if (strcmp(node->ip, ip) == 0 && strcmp(node->tcp, tcp) == 0) {
    fprintf(stderr, ERR "SAFE contains joining node's own details\n");
    return;
  }

  ndn_safe(node, ip, tcp);

  ndn_register(node, net);

  node->in_net = true;

  printf(OK "Joined network %03d\n", net);
}

void ndn_direct_join(Node *node, char *connectIP, char *connectTCP) {
  if (node->in_net) {
    fprintf(stderr, ERR "Already in a network\n");
    return;
  }

  int external_fd, errcode;
  ssize_t n;
  struct addrinfo hints, *res;
  char buffer[128];

  node->external->ip = connectIP;
  node->external->tcp = connectTCP;

  // Create and connect the TCP socket to connectIP:connectTCP
  if ((external_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror(ERR "socket");
    return;
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((errcode = getaddrinfo(connectIP, connectTCP, &hints, &res)) != 0) {
    fprintf(stderr, ERR "getaddrinfo: %s\n", gai_strerror(errcode));
    return;
  }

  printf(NOTICE "Attempting to connect with %s:%s\n", connectIP, connectTCP);

  if ((n = connect(external_fd, res->ai_addr, res->ai_addrlen)) == -1) {
    perror(ERR "connect");
    return;
  }

  node->external->fd = external_fd;
  node->external->addr = res;

  // ENTRY

  snprintf(buffer, sizeof(buffer), "ENTRY %s %s\n", node->ip, node->tcp);

  if ((n = write(external_fd, buffer, strlen(buffer))) < 0) {
    perror(ERR "write");
    return;
  }

  // wait for the SAFE response
  memset(buffer, 0, sizeof(buffer));
  if ((n = read(external_fd, buffer, sizeof(buffer) - 1)) < 0) {
    perror(ERR "read");
    return;
  }

  // check "SAFE IP TCP\n"
  char *ip = NULL, *tcp = NULL;
  if (sscanf(buffer, "SAFE %s %s\n", ip, tcp) != 2) {
    fprintf(stderr, ERR "Failed to parse response\n");
    return;
  }

  ndn_safe(node, ip, tcp);

  node->in_net = true;

  // off you go buddy
  printf(OK "Directly joined network of external %s:%s\n", connectIP,
         connectTCP);
}

void ndn_create(Node *node, const char *name) {
  printf(NOTICE "Creating object %s\n", name);

  Object object = malloc(strlen(name) + 1);
  strcpy(object, name);

  list_add(node->objects, object, NULL, NULL);
}

void ndn_delete(Node *node, const char *name) {
  printf(NOTICE "Deleting object %s\n", name);

  list_remove(node->objects, (char *)name);
}

void ndn_retrieve(Node *node, const char *name) {
  printf(NOTICE "Retrieving object %s\n", name);

  ObjectList *object = list_find(node->objects, (char *)name);
  if (object != NULL) {
    printf(OK "Already in node\n");
    return;
  }

  printf(NOTICE "Not in node, requesting to adjacent nodes\n");

  // TODO: Send interest to adjacent nodes
}

void ndn_show_topology(Node *node) {
  printf(NOTICE "Network topology:\n");
  printf(RESET "\tSafeguard -> %s:%s\n", node->safeguard->ip, node->safeguard->tcp);
  printf(RESET "\tExternal  -> %s:%s\n", node->external->ip, node->external->tcp);
  printf(RESET "\tInternals:\n");
  for (usize i = 0; i < node->internal_size; i++) {
    printf(RESET "\t->%s:%s\n", node->internal[i]->ip, node->internal[i]->tcp);
  }
}

void ndn_show_names(Node *node) {
  printf(NOTICE "Owned:\n");
  list_print(node->objects);

  // print the cache
  printf(NOTICE "Cached:\n");
  for (usize i = 0; i < node->cache_size; i++) {
    if (node->cache[i] != NULL) {
      printf(NOTICE "\t%s\n", node->cache[i]);
    }
  }
}

void ndn_show_interest_table(Node *node) {
  printf(NOTICE "Interest:\n");
  list_print_interests(node->interests);
}

void ndn_leave(Node *node) {
  if (!node->in_net) {
    fprintf(stderr, ERR "Not connected to a network\n");
    return;
  }

  node->in_net = false;

  printf(NOTICE "Leaving network\n");
}
