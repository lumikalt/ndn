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
  printf(CYAN "NOTICE" CLEAR "\tCommands:\n"
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
  printf(CYAN "NOTICE" CLEAR "\tJoining network %03d\n", net);

  NodeList *network = ndn_nodes(node, net);
  if (network->size == 0) {
    printf(GREEN "OK" CLEAR "\tLone node, waiting for others\n");
    return;
  }

  // Connect to a random node in the network
  int node_id = rand() % network->size;
  char *node_ip = network->ip[node_id];
  char *node_tcp = network->tcp[node_id];

  printf(CYAN "NOTICE" CLEAR "\tExternal %s:%s chosen, attempting connection\n",
         node_ip, node_tcp);

  // Create TCP socket
  int external_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (external_fd < 0) {
    fprintf(stderr, RED "ERR" CLEAR "\tFailed to create socket\n");
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
    fprintf(stderr, RED "ERR" CLEAR "\tgetaddrinfo error: %s\n",
            gai_strerror(status));
    close(external_fd);
    return;
  }

  if (connect(external_fd, res->ai_addr, res->ai_addrlen) < 0) {
    perror(RED "ERR" CLEAR "\tConnection to nearby node failed");
    close(external_fd);
    freeaddrinfo(res);
    return;
  }

  printf(GREEN "NOTICE" CLEAR "\tConnected to external %s:%s\n", node_ip,
         node_tcp);

  node->external->ip = node_ip;
  node->external->tcp = node_tcp;
  node->external->addr = res;
  node->external->fd = external_fd;

  ndn_register(node, net);
}

void ndn_direct_join(Node *node, u16 net, char *connectIP, char *connectTCP) {
  int external_fd, errcode;
  ssize_t n;
  struct addrinfo hints, *res;
  char buffer[128];

  printf(CYAN "NOTICE" CLEAR "\tDirectly joining network %d\n", net);
  node->external->ip = connectIP;
  node->external->tcp = connectTCP;

  // Create and connect the TCP socket to connectIP:connectTCP
  if ((external_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror(RED "ERR" CLEAR "\tsocket");
    exit(1);
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((errcode = getaddrinfo(connectIP, connectTCP, &hints, &res)) != 0) {
    fprintf(stderr, RED "ERR" CLEAR "\tgetaddrinfo: %s\n",
            gai_strerror(errcode));
    exit(1);
  }

  if ((n = connect(external_fd, res->ai_addr, res->ai_addrlen)) == -1) {
    perror(RED "ERR" CLEAR "\tconnect");
    exit(1);
  }
  freeaddrinfo(res);

  // ENTRY
  snprintf(buffer, sizeof(buffer), "ENTRY %s %s\n", node->ip, node->tcp);
  n = write(external_fd, buffer, strlen(buffer));
  if (n < 0) {
    perror(RED "ERR" CLEAR "\twrite");
    exit(1);
  }

  // wait for the SAFE response
  memset(buffer, 0, sizeof(buffer));
  if ((n = read(external_fd, buffer, sizeof(buffer) - 1)) < 0) {
    perror(RED "ERR" CLEAR "\tread");
    exit(1);
  }

  // check "SAFE IP TCP\n"
  char expected[128];
  snprintf(expected, sizeof(expected), "SAFE %s %s\n", node->external->ip,
           node->external->tcp);
  if (strncmp(buffer, expected, strlen(expected)) != 0) {
    fprintf(stderr, RED "ERR" CLEAR "\tUnexpected response: %s\n", buffer);
    exit(1);
  }

  ndn_safe(node, connectIP, connectTCP);

  // off you go buddy
  printf(GREEN "OK" CLEAR
               "\tDirectly joined network %d, linked to external %s:%s\n",
         net, connectIP, connectTCP);
}

void ndn_create(Node *node, const char *name) {
  printf(CYAN "NOTICE" CLEAR "\tCreating object %s\n", name);

  Object object = malloc(strlen(name) + 1);
  strcpy(object, name);

  list_add(node->objects, object, NULL, NULL);
}

void ndn_delete(Node *node, const char *name) {
  printf(CYAN "NOTICE" CLEAR "\tDeleting object %s\n", name);

  list_remove(node->objects, (char *)name);
}

void ndn_retrieve(Node *node, const char *name) {
  printf(CYAN "NOTICE" CLEAR "\tRetrieving object %s\n", name);

  ObjectList *object = list_find(node->objects, (char *)name);
  if (object != NULL) {
    printf(GREEN "OK" CLEAR "\tAlready in node\n");
    return;
  }

  printf(CYAN "NOTICE" CLEAR "\tNot in node, requesting to adjacent nodes\n");

  // TODO: Send interest to adjacent nodes
}

void ndn_show_topology(Node *node) {
  printf(CYAN "NOTICE" CLEAR "\tNetwork topology:\n");
  printf(CYAN "NOTICE" CLEAR "\t\tSafeguard: %s:%s\n", node->safeguard->ip,
         node->safeguard->tcp);
  printf(CYAN "NOTICE" CLEAR "\t\tExternal: %s:%s\n", node->external->ip,
         node->external->tcp);
  printf(CYAN "NOTICE" CLEAR "\t\tInternal:\n");
  for (usize i = 0; i < node->internal_size; i++) {
    printf(CYAN "NOTICE" CLEAR "\t\t\t%s:%s\n", node->internal[i]->ip,
           node->internal[i]->tcp);
  }
}

void ndn_show_names(Node *node) {
  printf(CYAN "NOTICE" CLEAR "\tOwned:\n");
  list_print(node->objects);

  // print the cache
  printf(CYAN "NOTICE" CLEAR "\tCached:\n");
  for (usize i = 0; i < node->cache_size; i++) {
    if (node->cache[i] != NULL) {
      printf(CYAN "NOTICE" CLEAR "\t\t%s\n", node->cache[i]);
    }
  }
}

void ndn_show_interest_table(Node *node) {
  printf(CYAN "NOTICE" CLEAR "\tInterest:\n");
  list_print_interests(node->interests);
}

void ndn_leave(Node *node) {
  printf(CYAN "NOTICE" CLEAR "\tLeaving network\n");
}
