#include "tcp.h"
#include "../util.h"

#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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

void ndn_entry(Node *node, char *ip, char *tcp, int enteringfd) {
  char buffer[128];

  grow_internal(node);
  node->internal[node->internal_index] = malloc(sizeof(AdjacentNode));
  if (!node->internal[node->internal_index]) {
    fprintf(stderr, ERR "Failed to allocate internal node\n");
    return;
  }

  node->internal[node->internal_index]->ip = strdup(ip);
  node->internal[node->internal_index]->tcp = strdup(tcp);
  node->internal[node->internal_index]->fd = enteringfd;
  node->internal_index++;

  printf(OK "Added new internal %s:%s\n", ip, tcp);

  // Check if we need an external
  if (node->external->fd == -1) {
    printf(NOTICE "No external yet, choosing this connection\n");

    node->external->ip = strdup(ip);
    node->external->tcp = strdup(tcp);
    node->external->fd = enteringfd;

    sprintf(buffer, "SAFE %s %s\n",
            node->external->ip ? node->external->ip : "0.0.0.0",
            node->external->tcp ? node->external->tcp : "0");

    if (write(enteringfd, buffer, strlen(buffer)) < 0) {
      perror(ERR "writing SAFE");
    }

    sprintf(buffer, "ENTRY %s %s\n", node->ip, node->tcp);
    if (write(enteringfd, buffer, strlen(buffer)) < 0) {
      perror(ERR "writing ENTRY");
    } else {
      printf(NOTICE "Sending it an ENTRY message\n");
    }

    return;
  }

  // Se já tiver externo
  sprintf(buffer, "SAFE %s %s\n",
          node->external->ip ? node->external->ip : "0.0.0.0",
          node->external->tcp ? node->external->tcp : "0");

  if (write(enteringfd, buffer, strlen(buffer)) < 0) {
    perror(ERR "writing SAFE");
  }

  printf(NOTICE "Sent safeguard\n");

  // enviar msg de salvaguarda por fd;
  // return;
}

/* Object */

void ndn_interest(Node *node, Object object, int fd) {
  char buffer[128];

  // Check if we have the object
  if (list_find(node->objects, object)) { // we got it
    printf(NOTICE "Object belongs to self... ");

    sprintf(buffer, "OBJECT %s\n", object);
    if (write(fd, buffer, strlen(buffer)) < 0) {
      perror("\n" ERR "writing OBJECT");
    }

    printf("sent\n");

    return;
  }

  // Check if we have it cached
  for (usize i = 0; i < node->cache_count; i++) {
    usize pos = (node->cache_head + i) % node->cache_size;
    if (node->cache[pos] &&
        strcmp(node->cache[pos], object) == 0) { // its cached
      printf(NOTICE "Object is cached... ");

      sprintf(buffer, "OBJECT %s\n", object);
      if (write(fd, buffer, strlen(buffer)) < 0) {
        perror("\n" ERR "writing OBJECT");
      }

      printf("sent\n");

      return;
    }
  }

  // check if fd is our only connection
  if (node->external->fd == fd &&
      (node->internal_index == 0 ||
       (node->internal_index == 1 && node->internal[0]->fd == fd))) {
    printf(NOTICE "No other connections, can't help\n");

    sprintf(buffer, "NOOBJECT %s\n", object);
    if (write(fd, buffer, strlen(buffer)) < 0) {
      perror("\n" ERR "writing NOOBJECT");
    }

    return;
  }

  // Request it from the network

  printf(NOTICE "Requesting object from network\n");

  // Add to interest list
  list_add(node->interests, object, fd, 0);

  sprintf(buffer, "INTEREST %s\n", object);

  // Ask all internals
  for (usize i = 0; i < node->internal_index; i++) {
    // dont ask it from nodes already waiting for it
    ObjectList *found = list_find(node->interests, object);
    bool found_i = false;
    if (found) {
      for (usize j = 0; j < found->to_size; j++) {
        if (found->to[j] == node->internal[i]->fd) {
          found_i = true;
          break;
        }
      }
    }

    if (found_i) {
      continue;
    }

    if (node->internal[i]->fd == -1 || node->internal[i]->fd == fd || node->internal[i]->fd == node->external->fd) {
      continue;
    }

    if (write(node->internal[i]->fd, buffer, strlen(buffer)) < 0) {
      perror("\n" ERR "writing INTEREST");
    }

    list_add(node->interests, object, 0, node->internal[i]->fd);
  }

  // Ask external
  if (node->external->fd != fd) {
    if (write(node->external->fd, buffer, strlen(buffer)) < 0) {
      perror("\n" ERR "writing INTEREST");
    }

    printf(NOTICE "Sent INTEREST to external\n");
  }
}

void ndn_object(Node *node, Object object, int fd) {
  // Check if this is the object we're waiting for
  if (node->current_retrieval && strcmp(node->current_retrieval, object) == 0) {
    printf(OK "Received requested object\n");
    node->retrieval_done = true;
  }

  // Add to cache
  cache_add(node, object);
  printf(OK "Cached object\n");

  // Find the interest for this object
  ObjectList *interest = list_find(node->interests, object);
  if (!interest) {
    // No interest record found, nothing more to do
    return;
  }

  // Safely forward the object to interested nodes
  if (interest->by && interest->by_size > 0) {
    for (usize i = 0; i < interest->by_size; i++) {
      int from = interest->by[i];
      if (from == fd || from == -1) {
        continue; // Don't send back to sender or to self
      }

      printf(NOTICE "Sending fd_%02d this object... ", from);

      char buffer[256];
      sprintf(buffer, "OBJECT %s\n", object);
      if (write(from, buffer, strlen(buffer)) < 0) {
        perror("\n" ERR "writing OBJECT");
      } else {
        printf("sent\n");
      }
    }
  }

  // Remove interest
  list_remove(node->interests, object);
}

void ndn_noobject(Node *node, Object object, int senderfd) {
  ObjectList *interest = list_find(node->interests, object);
  if (interest == NULL) {
    printf(NOTICE "Not interested in this object\n");
    return;
  }

  // remove sender from interest->to
  bool all_negative = true;
  for (usize i = 0; i < interest->to_size; i++) {
    if (interest->to[i] == senderfd) {
      interest->to[i] = -1;
      break;
    }

    if (interest->to[i] != -1) {
      all_negative = false;
    }
  }

  if (!all_negative)
    return;

  list_remove(node->interests, object);

  printf(NOTICE "No one has the object, reporting to the interested nodes... ");

  // Write NOOBJECT to all other interests
  char buffer[128];
  sprintf(buffer, "NOOBJECT %s\n", object);

  bool self_interest = false;

  for (usize i = 0; i < interest->by_size; i++) {
    if (interest->by[i] == -1) {
      self_interest = true;
      continue;
    }

    if (write(interest->by[i], buffer, strlen(buffer)) < 0) {
      perror("\n" ERR "writing NOOBJECT");
    }
  }

  printf("sent\n");

  if (self_interest) {
    printf(ERR "Could not find requested object in net\n");
  }
}

/* TCP cancelation */

void ndn_exit__ext(Node *node);

void ndn_node_exit(Node *node, int fd) {
  // Remove from internals
  for (usize i = 0; i < node->internal_index; i++) {
    if (node->internal[i]->fd == node->external->fd) {
      printf(NOTICE "External was also an internal\n");

      free(node->internal[i]->ip);
      free(node->internal[i]->tcp);
      free(node->internal[i]);
      node->internal[i] = NULL;

      // Shift remaining nodes down to fill the gap
      for (usize k = i; k < node->internal_index - 1; k++) {
        node->internal[k] = node->internal[k + 1];
      }
      node->internal_index--;
      node->internal[node->internal_index] = NULL;
      break;
    }
  }

  if (node->external->fd == fd) {
    ndn_exit__ext(node);
    return;
  }

  if (node->safeguard->fd == fd) {
    printf(NOTICE "Safeguard disconnected\n");
    // Not specified...
    return;
  }

  // It's an internal node, already removed
  printf(NOTICE "Internal disconnected\n");
}

// The external node has disconnected
void ndn_exit__ext(Node *node) {
  printf(NOTICE "External disconnected\n");

  free(node->external->ip);
  free(node->external->tcp);
  node->external->ip = NULL;
  node->external->tcp = NULL;
  node->external->fd = -1;

  // Now there are 2 cases: either we're our own safeguard or not
  if (node->safeguard->fd != -1) {
    // Send ENTRY to safeguard
    node->external->fd = node->safeguard->fd;

    // Make copies of strings instead of direct pointer assignment
    if (node->external->ip) {
      free(node->external->ip);
    }
    if (node->external->tcp) {
      free(node->external->tcp);
    }

    node->external->ip = strdup(node->safeguard->ip);
    node->external->tcp = strdup(node->safeguard->tcp);

    node->safeguard->fd = -1;

    // Now free the safeguard pointers
    if (node->safeguard->ip) {
      free(node->safeguard->ip);
      node->safeguard->ip = NULL;
    }
    if (node->safeguard->tcp) {
      free(node->safeguard->tcp);
      node->safeguard->tcp = NULL;
    }

    printf(NOTICE "Elevating safeguard to external... ");

    char buffer[128];
    sprintf(buffer, "ENTRY %s %s\n", node->ip, node->tcp);
    if (write(node->external->fd, buffer, strlen(buffer)) < 0) {
      perror("\n" ERR "writing ENTRY");
      return;
    }
    printf("done\n");

    printf(NOTICE "Communicating changes to internals... ");

    for (usize i = 0; i < node->internal_index; i++) {
      if (node->internal[i]->fd == -1) {
        continue;
      }

      sprintf(buffer, "SAFE %s %s\n", node->external->ip, node->external->tcp);
      if (write(node->internal[i]->fd, buffer, strlen(buffer)) < 0) {
        perror("\n" ERR "writing SAFE");
      }
    }
    printf("sent\n");
  } else {
    // We're our own safeguard
    if (node->safeguard->ip) {
      free(node->safeguard->ip);
      node->safeguard->ip = NULL; // Set to NULL after freeing
    }
    free(node->safeguard->tcp);
    node->safeguard->ip = NULL;
    node->safeguard->tcp = NULL;

    if (node->internal_index == 0) {
      printf(NOTICE "Elevating self to external (lone node state)\n");

      if (node->external->ip) {
        free(node->external->ip);
      }
      if (node->external->tcp) {
        free(node->external->tcp);
      }

      node->external->fd = -1;
      node->external->ip = strdup(node->ip);
      node->external->tcp = strdup(node->tcp);
    } else {
      // Choose a random internal to elevate to external
      printf(NOTICE "Elevating random internal to external\n");

      usize i = rand() % node->internal_index;

      node->external->fd = node->internal[i]->fd;
      node->external->ip = node->internal[i]->ip;
      node->external->tcp = node->internal[i]->tcp;

      free(node->internal[i]->ip);
      free(node->internal[i]->tcp);
      free(node->internal[i]);
      node->internal[i] = NULL;

      // Shift remaining nodes down to fill the gap
      for (usize k = i; k < node->internal_index - 1; k++) {
        node->internal[k] = node->internal[k + 1];
      }
      node->internal_index--;
      node->internal[node->internal_index] = NULL;

      printf(NOTICE "Elevated internal %zu to external\n", i);
      printf(NOTICE "Communicating to new external... ");

      char buffer[128];
      sprintf(buffer, "ENTRY %s %s\n", node->ip, node->tcp);
      if (write(node->external->fd, buffer, strlen(buffer)) < 0) {
        perror("\n" ERR "writing ENTRY");
        return;
      }
      printf("done\n");

      printf(NOTICE "Communicating changes to internals... ");

      for (usize i = 0; i < node->internal_index; i++) {
        if (node->internal[i]->fd == -1) {
          continue;
        }

        sprintf(buffer, "SAFE %s %s\n", node->external->ip,
                node->external->tcp);
        if (write(node->internal[i]->fd, buffer, strlen(buffer)) < 0) {
          perror("\n" ERR "writing SAFE");
        }
        printf("sent\n");
      }
    }
  }
}
