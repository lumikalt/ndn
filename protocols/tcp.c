#include "tcp.h"
#include "../util.h"

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Change function signature
void ndn_safe(Node *node, char *ip, char *tcp) {
  // Create and connect the TCP socket to ip:tcp
  int fd, errcode;
  ssize_t n;
  struct addrinfo hints, *res;

  // Error checks
  if (ip == NULL || tcp == NULL) {
    fprintf(stderr, ERR "Invalid SAFE message format\n");
    return;
  }

  node->safeguard->ip = strdup(ip);
  node->safeguard->tcp = strdup(tcp);

  printf(NOTICE "Got external (%s:%s)'s external (%s:%s)\n", node->external->ip,
         node->external->tcp, ip, tcp);

  if (strcmp(node->external->ip, ip) == 0 &&
      strcmp(node->external->tcp, tcp) == 0) {
    printf(WARN "SAFE contains external node's own details\n");
    return;
  }

  if (strcmp(node->ip, ip) == 0 && strcmp(node->tcp, tcp) == 0) {
    printf(NOTICE "SAFE contains this node's own details\n");
    return;
  }

  printf(NOTICE "Connecting to safeguard\n");

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    fprintf(stderr, ERR "Failed to create the socket\n");
    return;
  }

  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  if ((errcode = getaddrinfo(ip, tcp, &hints, &res)) != 0) {
    fprintf(stderr, ERR "Failed to get the address info (%s)\n",
            gai_strerror(errcode));
    return;
  }

  if ((n = connect(fd, res->ai_addr, res->ai_addrlen)) == -1) {
    fprintf(stderr, ERR "Failed to connect to the safeguard\n");
    return;
  }

  freeaddrinfo(res);

  node->safeguard->fd = fd;

  printf(OK "Connected to safeguard\n");

  return;
}

void ndn_entry(Node *node, char *ip, char *tcp, int fd2) {
  // Create and connect the TCP socket to ip:tcp
  int fd, errcode;
  ssize_t n;
  struct addrinfo hints, *res;
  char buffer[128];

  if (node->internal_size == node->internal_capacity) {
    node->internal = realloc(node->internal, 2 * node->internal_capacity *
                                                 sizeof(AdjacentNode *));
    if (!node->internal) {
      fprintf(stderr, ERR "Failed to reallocate internal nodes array\n");
      return;
    }
    node->internal_capacity *= 2;
  }

  // Allocate a new AdjacentNode for this connection
  node->internal[node->internal_size] = malloc(sizeof(AdjacentNode));
  if (!node->internal[node->internal_size]) {
    fprintf(stderr, ERR "Failed to allocate internal node\n");
    return;
  }

  // Store IP and TCP
  node->internal[node->internal_size]->ip = strdup(ip);
  node->internal[node->internal_size]->tcp = strdup(tcp);
  node->internal[node->internal_size]->fd = fd2; // Will set this later

  // Increment internal size
  node->internal_size++;

  printf(OK "Added new internal %s:%s\n", ip, tcp);

  // Check if we need to update external
  if (node->external->fd == -1) { // Se não tiver externo
    node->external->ip = strdup(ip);
    node->external->tcp = strdup(tcp);
    node->external->fd = fd2;

    sprintf(buffer, "SAFE %s %s\n",
            node->external->ip ? node->external->ip : "0.0.0.0",
            node->external->tcp ? node->external->tcp : "0");

    if (write(fd2, buffer, strlen(buffer)) < 0) {
      perror(ERR "writing SAFE");
    }

    printf(NOTICE "No external yet, choosing this connection\n");

    sprintf(buffer, "ENTRY %s %s\n", node->ip, node->tcp);
    if (write(fd2, buffer, strlen(buffer)) < 0) {
      perror(ERR "writing ENTRY");
    } else {
      printf(NOTICE "sending it an ENTRY message\n");
    }

    return;
  }

  // Se já tiver externo
  sprintf(buffer, "SAFE %s %s\n",
          node->external->ip ? node->external->ip : "0.0.0.0",
          node->external->tcp ? node->external->tcp : "0");

  if (write(fd2, buffer, strlen(buffer)) < 0) {
    perror("write SAFE");
  }

  printf("NOTICE: SAFE response sent to %s:%s\n", ip, tcp);

  // enviar msg de salvaguarda por fd;
  // return;

  /*
    // IMPORTANT: Send SAFE response BEFORE connecting back
    // This prevents the deadlock
    //char buffer[128];
    sprintf(buffer, "SAFE %s %s\n",
            node->external->ip ? node->external->ip : "0.0.0.0",
            node->external->tcp ? node->external->tcp : "0");

    // Find the socket connected to this client
    int client_fd = -1;
    for (int i = 0; i <= FD_SETSIZE; i++) {
      if (i != node->listener_fd && i != STDIN_FILENO && i > 2) {
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        if (getpeername(i, (struct sockaddr *)&addr, &addr_len) == 0) {
          char client_ip[16];
          inet_ntop(AF_INET, &(addr.sin_addr), client_ip, sizeof(client_ip));
          if (strcmp(client_ip, ip) == 0) {
            client_fd = i;
            break;
          }
        }
      }
    }

    if (client_fd != -1) {
      printf(NOTICE "Sending SAFE response to %s:%s on FD %d\n", ip, tcp,
             client_fd);
      if (write(client_fd, buffer, strlen(buffer)) < 0) {
        perror(ERR "write SAFE");
      }
    } else {
      fprintf(stderr, ERR "Could not find client socket for %s:%s\n", ip, tcp);
    }

    // Now connect back to the new node
    printf(NOTICE "Now connecting to new internal %s:%s\n", ip, tcp);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((errcode = getaddrinfo(ip, tcp, &hints, &res)) != 0) {
      fprintf(stderr, ERR "getaddrinfo: %s\n", gai_strerror(errcode));
      return;
    }

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      fprintf(stderr, ERR "Failed to create the socket\n");
      return;
    }

    if ((n = connect(fd, res->ai_addr, res->ai_addrlen)) == -1) {
      fprintf(stderr, ERR "Failed to connect to the new internal\n");
      return;
    }*/

  /*freeaddrinfo(res);

  // Now store the connection details
  node->internal[node->internal_size]->fd = fd;

  if (node->external->fd == -1) // Update external if needed
    node->external->fd = fd;*/
}
