#include "util.h"
#include "commands.h"
#include "list.h"
#include "protocols/udp.h"
#include "types.h"

#include <netdb.h>
#include <stdio.h>
#include <unistd.h>

Node *init_node(usize cache_size, char *ip, char *tcp, char *regIP,
                char *regUDP) {
  Node *node = malloc(sizeof(Node));
  if (!node) {
    perror("malloc fail");
    exit(1);
  }

  node->cache_size = cache_size;
  node->cache = calloc(cache_size, sizeof(Object));
  if (!node->cache) {
    perror("calloc fail");
    exit(1);
  }

  node->objects = NULL;
  node->interests = NULL;

  node->network = malloc(sizeof(NodeList));
  if (!node->network) {
    perror("malloc");
    exit(1);
  }
  node->network->ip = NULL;
  node->network->tcp = NULL;
  node->network->size = 0;
  node->network->capacity = 0;

  node->ip = ip;
  node->tcp = tcp;
  if (!node->ip || !node->tcp) {
    perror("strdup");
    exit(1);
  }

  node->safeguard = malloc(sizeof(AdjacentNode));
  if (!node->safeguard) {
    perror("malloc");
    exit(1);
  }
  node->safeguard->ip = NULL;
  node->safeguard->tcp = NULL;
  node->safeguard->addr = NULL;
  node->safeguard->fd = -1;

  node->external = malloc(sizeof(AdjacentNode));
  if (!node->external) {
    perror("malloc");
    exit(1);
  }
  node->external->ip = NULL;
  node->external->tcp = NULL;
  node->external->addr = NULL;
  node->external->fd = -1;

  node->internal = NULL;

  // create UDP client connection to the server
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if (getaddrinfo(regIP, regUDP, &hints, &res) != 0) {
    perror("getaddrinfo");
    exit(1);
  }

  int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (fd == -1) {
    perror("socket");
    exit(1);
  }

  node->server = malloc(sizeof(Server));
  if (!node->server) {
    perror("malloc");
    exit(1);
  }
  node->server->addr = res;
  node->server->fd = fd;

  // create TCP listener
  // memset(&hints, 0, sizeof hints);
  // hints.ai_family = AF_INET;
  // hints.ai_socktype = SOCK_STREAM;
  // hints.ai_flags = AI_PASSIVE;

  // if (getaddrinfo(NULL, tcp, &hints, &res) != 0) {
  //   perror("getaddrinfo");
  //   exit(1);
  // }

  // int listener_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  // if (listener_fd == -1) {
  //   perror("socket");
  //   exit(1);
  // }

  // if (bind(listener_fd, res->ai_addr, res->ai_addrlen) == -1) {
  //   perror("bind");
  //   exit(1);
  // }

  // node->listener_fd = listener_fd;
  // FD_ZERO(&node->master_fds);
  // FD_ZERO(&node->read_fds);
  // FD_SET(listener_fd, &node->master_fds);
  // node->max_fd = listener_fd;

  return node;
}

void clean_node(Node *node) {
  list_destroy(node->objects);
  list_destroy(node->interests);
  free(node->cache);
  free(node->network->ip);
  free(node->network->tcp);
  free(node->network);
  free(node->safeguard->addr);
  close(node->safeguard->fd);
  freeaddrinfo(node->safeguard->addr);
  free(node->safeguard);
  freeaddrinfo(node->external->addr);
  close(node->external->fd);
  free(node->external);
  for (usize i = 0; i < node->network->size; i++) {
    freeaddrinfo(node->internal[i]->addr);
    // clean the file descriptors
    close(node->internal[i]->fd);
    free(node->internal[i]);
  }
  free(node->internal);
  freeaddrinfo(node->server->addr);
  free(node->server);
  free(node->ip);
  free(node->tcp);

  free(node);
}

usize str_char_count(const char *s, char c) {
  usize count = 0;
  for (usize i = 0; s[i]; s[i] == c ? count++, i++ : i++)
    ;
  return count;
}

int is_valid_net(char *net) {
  return strlen(net) == 3 && isdigit(net[0]) && isdigit(net[1]) &&
         isdigit(net[2]);
}

int is_valid_ip(char *ip) {
  struct sockaddr_in sa;
  return inet_pton(AF_INET, ip, &sa.sin_addr);
}

int is_valid_port(char *port) {
  char *end;
  long p = strtol(port, &end, 10);
  return *end == '\0' && p > 0 &&
         p <= 65535; // NOTE: confirmar se é o numero certo
}

int is_valid_name(char *name) {
  size_t len = strlen(name);
  if (len == 0 || len > 100)
    return 0;
  for (size_t i = 0; i < len; i++) {
    if (!isalnum(name[i]))
      return 0;
  }
  return 1;
}

void process_input_commands(Node *node, char *input) {
  char net[4], ip[16], port[6], name[101];
  int pos;
  input[strcspn(input, "\n")] = '\0';

  //---nodes---
  if ((strcmp(input, "nodes") == 0) || (strcmp(input, "n") == 0)) {
    NodeList *nodes = ndn_nodes(node, 123);

    for (usize i = 0; i < nodes->size; i++) {
      printf("%s:%s\n", nodes->ip[i], nodes->tcp[i]);
    }

    return;
  }
  //----------

  //---join---
  if ((sscanf(input, "join %3s%n", net, &pos) == 1 && input[pos] == '\0') ||
      (sscanf(input, "j %3s%n", net, &pos) == 1 && input[pos] == '\0')) {

    if (!is_valid_net(net)) {
      printf("Wrong input, it must be 3 digits.\n");
      return;
    }

    // ndn_join(node, atoi(net));
    printf("Joining network %s...\n", net);
    return;
  }
  //----------

  //---direct join---
  if ((sscanf(input, "direct join %15s %5s%n", ip, port, &pos) == 2 &&
       input[pos] == '\0') ||
      (sscanf(input, "dj %15s %5s%n", ip, port, &pos) == 2 &&
       input[pos] == '\0')) {

    if (!is_valid_ip(ip)) {
      printf("Invalid IP address\n");
    } else if (!is_valid_port(port)) {
      printf("Invalid port number\n");
    } else {
      printf("Direct joining via %s:%s\n", ip, port);

      if (strcmp(ip, "0.0.0.0") == 0) {
        printf("Created new network\n");
      }
    }

    // ndn_register(node, 0);

    return;
  }

  //----------

  //---create---
  if ((sscanf(input, "create %100s%n", name, &pos) == 1 &&
       input[pos] == '\0') ||
      (sscanf(input, "c %100s%n", name, &pos) == 1 && input[pos] == '\0')) {

    if (!is_valid_name(name)) {
      printf("Invalid name (alphanumeric, 1-100 chars)\n");
      return;
    }

    ndn_create(node, name);

    printf("Created object '%s'\n", name);
    return;
  }
  //----------

  //---delete---
  if ((sscanf(input, "delete %100s%n", name, &pos) == 1 &&
       input[pos] == '\0') ||
      (sscanf(input, "dl %100s%n", name, &pos) == 1 && input[pos] == '\0')) {

    if (!is_valid_name(name)) {
      printf("Invalid name\n");
      return;
    }

    ndn_delete(node, name);

    printf("Deleted object '%s'\n", name);
    return;
  }
  //----------

  //---help---
  if ((strcmp(input, "help") == 0) || (strcmp(input, "h") == 0)) {
    ndn_help();
    return;
  }
  //----------

  // if the command does not exist
  printf("That command does not exist. Please type '(h)elp' for the list of "
         "commands\n");
}

void errored(const char *msg, Node *node) {
  clean_node(node);

  printf("%s", msg);
  exit(1);
}
