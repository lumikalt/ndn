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
  printf(CYAN "NOTICE" RESET "\tCommands:\n"
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
  printf(CYAN "NOTICE" RESET "\tJoining network %03d\n", net);

  NodeList *network = ndn_nodes(node, net);
  if (network->size == 0) {
    printf(GREEN "OK" RESET "\tLone node, waiting for others\n");
    return;
  }

  // Connect to a random node in the network
  int node_id = rand() % network->size;
  char *node_ip = network->ip[node_id];
  char *node_tcp = network->tcp[node_id];

  printf(CYAN "NOTICE" RESET "\tExternal %s:%s chosen, attempting connection\n",
         node_ip, node_tcp);

  // Create TCP socket
  int external_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (external_fd < 0) {
    fprintf(stderr, RED "ERR" RESET "\tFailed to create socket\n");
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
    fprintf(stderr, RED "ERR" RESET "\tgetaddrinfo error: %s\n",
            gai_strerror(status));
    close(external_fd);
    return;
  }

  if (connect(external_fd, res->ai_addr, res->ai_addrlen) < 0) {
    perror(RED "ERR" RESET "\tConnection to nearby node failed");
    close(external_fd);
    freeaddrinfo(res);
    return;
  }

  printf(CYAN "NOTICE" RESET "\tConnected to external %s:%s\n", node_ip,
         node_tcp);

  node->external->ip = node_ip;
  node->external->tcp = node_tcp;
  node->external->addr = res;
  node->external->fd = external_fd;

  // ENTRY

  char buffer[128];
  ssize_t n;

  snprintf(buffer, sizeof(buffer), "ENTRY %s %s\n", node->ip, node->tcp);

  if ((n = write(external_fd, buffer, strlen(buffer))) < 0) {
    perror(RED "ERR" RESET "\twrite");
    return;
  }

  // wait for the SAFE response
  memset(buffer, 0, sizeof(buffer));
  if ((n = read(external_fd, buffer, sizeof(buffer) - 1)) < 0) {
    perror(RED "ERR" RESET "\tread");
    return;
  }

  // check "SAFE IP TCP\n"
  char *ip, *tcp;
  if (sscanf(buffer, "SAFE %s %s\n", ip, tcp) != 2) {
    fprintf(stderr, RED "ERR" RESET "\tFailed to parse response\n");
    return;
  }

  ndn_safe(node, ip, tcp);

  ndn_register(node, net);

  printf(GREEN "OK" RESET "\tJoined network %03d\n", net);
}

void ndn_direct_join(Node *node, u16 net, char *connectIP, char *connectTCP) {
  int external_fd, errcode;
  ssize_t n;
  struct addrinfo hints, *res;
  char buffer[128];

  printf(CYAN "NOTICE" RESET "\tDirectly joining network %d\n", net);
  node->external->ip = connectIP;
  node->external->tcp = connectTCP;

  // Create and connect the TCP socket to connectIP:connectTCP
  if ((external_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror(RED "ERR" RESET "\tsocket");
    return;
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((errcode = getaddrinfo(connectIP, connectTCP, &hints, &res)) != 0) {
    fprintf(stderr, RED "ERR" RESET "\tgetaddrinfo: %s\n",
            gai_strerror(errcode));
    return;
  }

  if ((n = connect(external_fd, res->ai_addr, res->ai_addrlen)) == -1) {
    perror(RED "ERR" RESET "\tconnect");
    return;
  }

  node->external->fd = external_fd;
  node->external->addr = res;

  // ENTRY

  snprintf(buffer, sizeof(buffer), "ENTRY %s %s\n", node->ip, node->tcp);

  if ((n = write(external_fd, buffer, strlen(buffer))) < 0) {
    perror(RED "ERR" RESET "\twrite");
    return;
  }

  // wait for the SAFE response
  memset(buffer, 0, sizeof(buffer));
  if ((n = read(external_fd, buffer, sizeof(buffer) - 1)) < 0) {
    perror(RED "ERR" RESET "\tread");
    return;
  }

  // check "SAFE IP TCP\n"
  char *ip, *tcp;
  if (sscanf(buffer, "SAFE %s %s\n", ip, tcp) != 2) {
    fprintf(stderr, RED "ERR" RESET "\tFailed to parse response\n");
    return;
  }

  ndn_safe(node, ip, tcp);

  ndn_register(node, net);

  // off you go buddy
  printf(GREEN "OK" RESET
               "\tDirectly joined network %d, linked to external %s:%s\n",
         net, connectIP, connectTCP);
}

void ndn_create(Node *node, const char *name) {
  printf(CYAN "NOTICE" RESET "\tCreating object %s\n", name);

  Object object = malloc(strlen(name) + 1);
  strcpy(object, name);

  list_add(node->objects, object, NULL, NULL);
}

void ndn_delete(Node *node, const char *name) {
  printf(CYAN "NOTICE" RESET "\tDeleting object %s\n", name);

  list_remove(node->objects, (char *)name);
}

void ndn_retrieve(Node *node, const char *name) {
  printf(CYAN "NOTICE" RESET "\tRetrieving object %s\n", name);

  ObjectList *object = list_find(node->objects, (char *)name);
  if (object != NULL) {
    printf(GREEN "OK" RESET "\tAlready in node\n");
    return;
  }

  printf(CYAN "NOTICE" RESET "\tNot in node, requesting to adjacent nodes\n");

  // TODO: Send interest to adjacent nodes
}

void ndn_show_topology(Node *node) {
  printf(CYAN "NOTICE" RESET "\tNetwork topology:\n");
  printf(CYAN "NOTICE" RESET "\t\tSafeguard: %s:%s\n", node->safeguard->ip,
         node->safeguard->tcp);
  printf(CYAN "NOTICE" RESET "\t\tExternal: %s:%s\n", node->external->ip,
         node->external->tcp);
  printf(CYAN "NOTICE" RESET "\t\tInternal:\n");
  for (usize i = 0; i < node->internal_size; i++) {
    printf(CYAN "NOTICE" RESET "\t\t\t%s:%s\n", node->internal[i]->ip,
           node->internal[i]->tcp);
  }
}

void ndn_show_names(Node *node) {
  printf(CYAN "NOTICE" RESET "\tOwned:\n");
  list_print(node->objects);

  // print the cache
  printf(CYAN "NOTICE" RESET "\tCached:\n");
  for (usize i = 0; i < node->cache_size; i++) {
    if (node->cache[i] != NULL) {
      printf(CYAN "NOTICE" RESET "\t\t%s\n", node->cache[i]);
    }
  }
}

void ndn_show_interest_table(Node *node) {
  printf(CYAN "NOTICE" RESET "\tInterest:\n");
  list_print_interests(node->interests);
}

void ndn_leave(Node *node) {
  printf(CYAN "NOTICE" RESET "\tLeaving network\n");
}
