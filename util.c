#include "util.h"
#include "list.h"
#include "types.h"

#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

Node *init_node(usize cache_size, char *ip, char *tcp, char *regIP,
                char *regUDP) {
  Node *node = malloc(sizeof(Node));
  if (!node) {
    perror(ERR "malloc fail");
    exit(1);
  }

  node->cache_size = cache_size;
  node->cache = calloc(cache_size, sizeof(Object));
  if (!node->cache) {
    perror(ERR "calloc fail");
    exit(1);
  }

  node->cache_head = 0;
  node->cache_count = 0;

  node->objects = list_create();
  node->interests = list_create();

  node->ip = ip;
  node->tcp = tcp;

  node->safeguard = malloc(sizeof(AdjacentNode));
  if (!node->safeguard) {
    perror(ERR "malloc");
    exit(1);
  }
  node->safeguard->ip = NULL;
  node->safeguard->tcp = NULL;
  node->safeguard->fd = -1;

  node->external = malloc(sizeof(AdjacentNode));
  if (!node->external) {
    perror(ERR "malloc");
    exit(1);
  }
  node->external->ip = NULL;
  node->external->tcp = NULL;
  node->external->fd = -1;

  node->internal = calloc(10, sizeof(AdjacentNode *));
  if (!node->internal) {
    perror(ERR "calloc");
    exit(1);
  }
  node->internal_index = 0;
  node->internal_capacity = 10;

  // create UDP client connection to the server
  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  if (getaddrinfo(regIP, regUDP, &hints, &res) != 0) {
    perror(ERR "getaddrinfo");
    exit(1);
  }

  int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (fd == -1) {
    perror(ERR "socket");
    exit(1);
  }

  node->server = malloc(sizeof(Server));
  if (!node->server) {
    perror(ERR "malloc");
    exit(1);
  }
  node->server->addr = res;
  node->server->fd = fd;
  node->server->ip = regIP;
  node->server->udp = regUDP;

  node->in_net = false;
  node->net = 1000;

  node->current_retrieval = NULL;
  node->retrieval_start_time = 0;
  node->retrieval_timeout = 3600; // 1h, absurdly high for testing
  node->retrieval_done = false;

  // create TCP listener

  int listener_fd;
  if ((listener_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror(ERR "socket");
    exit(1);
  }

  // Set socket options
  int optval = 1;
  if (setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                 sizeof(optval)) == -1) {
    perror(ERR "setsockopt");
    close(listener_fd);
    exit(1);
  }

  // Create IPv4 address structure directly
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(tcp));
  server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces

  // Bind socket
  if (bind(listener_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror(ERR "bind");
    close(listener_fd);
    exit(1);
  }

  // Start listening
  if (listen(listener_fd, 5) == -1) {
    perror(ERR "listen");
    close(listener_fd);
    exit(1);
  }

  printf(OK "TCP server listening on all interfaces, port %s\n", tcp);
  node->listener_fd = listener_fd;

  printf(OK "Node initialized, starting main loop...\n");

  node->exit = false;

  return node;
}

/* clean memory */

void clean_node(Node *node) {
  list_destroy(node->objects);
  list_destroy(node->interests);

  for (usize i = 0; i < node->cache_count; i++) {
    usize pos = (node->cache_head + i) % node->cache_size;
    if (node->cache[pos]) {
      free(node->cache[pos]);
    }
  }
  free(node->cache);

  if (node->safeguard) {
    if (node->safeguard->ip) {
      free(node->safeguard->ip);
    }
    if (node->safeguard->tcp) {
      free(node->safeguard->tcp);
    }
    if (node->safeguard->fd != -1) {
      close(node->safeguard->fd);
    }
    free(node->safeguard);
  }
  if (node->external) {
    if (node->external->ip) {
      free(node->external->ip);
    }
    if (node->external->tcp) {
      free(node->external->tcp);
    }
    if (node->external->fd != -1) {
      close(node->external->fd);
    }
    free(node->external);
  }
  if (node->internal) {
    for (usize i = 0; i < node->internal_index; i++) {
      if (node->internal[i]) {
        if (node->internal[i]->ip) {
          free(node->internal[i]->ip);
        }
        if (node->internal[i]->tcp) {
          free(node->internal[i]->tcp);
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

  if (node->listener_fd != -1)
    close(node->listener_fd);

  free(node);
}

void clean_nodelist(NodeList *nodes) {
  if (!nodes)
    return;

  for (usize i = 0; i < nodes->size; i++) {
    free(nodes->ip[i]);
    free(nodes->tcp[i]);
  }
  free(nodes->ip);
  free(nodes->tcp);
  free(nodes);
}

/* string utils */

usize str_char_count(const char *s, char c) {
  usize count = 0;
  for (usize i = 0; s[i]; s[i] == c ? count++, i++ : i++)
    ;
  return count;
}

char *str_escape(const char *s) {
  char *escaped = malloc(strlen(s) * 2 + 1);
  if (!escaped) {
    perror(ERR "malloc");
    exit(1);
  }

  usize j = 0;
  for (usize i = 0; s[i]; i++) {
    if (s[i] == '\n') {
      escaped[j++] = '\\';
      escaped[j++] = 'n';
    } else {
      escaped[j++] = s[i];
    }
  }
  escaped[j] = '\0';

  return escaped;
}

/* validity checks */

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

/* */

void grow_internal(Node *node) {
  if (node->internal_index == node->internal_capacity) {
    node->internal_capacity *= 2;
    node->internal = realloc(node->internal,
                             node->internal_capacity * sizeof(AdjacentNode *));
    if (!node->internal) {
      perror(ERR "realloc");
      exit(1);
    }
  }
}

void cache_add(Node *node, Object object) {
  if (node->cache_count == node->cache_size) {
    // Cache full, remove oldest (FIFO)
    free(node->cache[node->cache_head]);
    // Add new element where the oldest was
    node->cache[node->cache_head] = strdup(object);
    // Move head pointer (wrapping around if needed)
    node->cache_head = (node->cache_head + 1) % node->cache_size;
  } else {
    usize insert_pos =
        (node->cache_head + node->cache_count) % node->cache_size;
    node->cache[insert_pos] = strdup(object);
    node->cache_count++;
  }
}

bool cache_contains(Node *node, Object object) {
  for (usize i = 0; i < node->cache_count; i++) {
    usize pos = (node->cache_head + i) % node->cache_size;
    if (strcmp(node->cache[pos], object) == 0) {
      return true;
    }
  }
  return false;
}
