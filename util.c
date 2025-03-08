#include "util.h"
#include "list.h"
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

  node->ip = ip;
  node->tcp = tcp;

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

  int listener_fd, new_fd, max_fd, counter;
  // struct addrinfo hints, *res;
  struct sockaddr addr;
  socklen_t addrlen;
  char buffer[128];
  fd_set master_fds, read_fds;

  memset(&hints, 0, sizeof hints);

  hints.ai_socktype = SOCK_STREAM; // TCP socket
  hints.ai_flags = AI_PASSIVE;

  // Get address info for binding the socket
  if (getaddrinfo(NULL, tcp, &hints, &res) != 0) {
    perror("getaddrinfo");
    exit(1);
  }

  // Create a socket
  if ((listener_fd =
           socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
    perror("socket");
    exit(1);
  }

  // Bind socket to the specified port
  if (bind(listener_fd, res->ai_addr, res->ai_addrlen) == -1) {
    perror("bind");
    exit(1);
  }

  // No longer needed, so we free the structure
  freeaddrinfo(res);

  // Start listening for incoming connections
  if (listen(listener_fd, 5) == -1) {
    perror("listen");
    exit(1);
  }

  printf("OK: TCP server listening on port %s\n", tcp);

  node->listener_fd = listener_fd;

  return node;
}

void clean_node(Node *node) {
  list_destroy(node->objects);
  list_destroy(node->interests);

  for (usize i = 0; i < node->cache_size; i++) {
    free(node->cache[i]);
  }
  free(node->cache);

  if (node->safeguard) {
    if (node->safeguard->addr) {
      freeaddrinfo(node->safeguard->addr);
    }
    if (node->safeguard->fd != -1) {
      close(node->safeguard->fd);
    }
    free(node->safeguard);
  }
  if (node->external) {
    if (node->external->addr) {
      freeaddrinfo(node->external->addr);
    }
    if (node->external->fd != -1) {
      close(node->external->fd);
    }
    free(node->external);
  }
  if (node->internal) {
    for (usize i = 0; i < node->internal_size; i++) {
      if (node->internal[i]) {
        if (node->internal[i]->addr) {
          freeaddrinfo(node->internal[i]->addr);
        }
        if (node->internal[i]->fd != -1) {
          close(node->internal[i]->fd);
        }
        free(node->internal[i]);
      }
    }
    free(node->internal);
  }
  if (node->server) {
    if (node->server->addr) {
      freeaddrinfo(node->server->addr);
    }
    free(node->server);
  }

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
  return *end == '\0' && p > 0 && p <= 65535;
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

void clear_nodelist(NodeList *nodes) {
  for (usize i = 0; i < nodes->size; i++) {
    free(nodes->ip[i]);
    free(nodes->tcp[i]);
  }
  free(nodes->ip);
  free(nodes->tcp);
  free(nodes);
}
