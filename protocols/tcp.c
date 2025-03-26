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
      perror(ERR "writing OBJECT");
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
        perror(ERR "writing OBJECT");
      }

      printf("sent\n");

      return;
    }
  }

  // check if fd is our only connection
  if (node->external->fd == fd &&
      (node->internal_index == 0 ||
       (node->internal_index == 1 && node->internal[0]->fd == fd))) {
    printf(NOTICE "No other connections, replying... ");

    sprintf(buffer, "NOOBJECT %s\n", object);
    if (write(fd, buffer, strlen(buffer)) < 0) {
      perror(ERR "writing NOOBJECT");
    }

    printf("sent\n");

    return;
  }

  // Request it from the network

  printf(NOTICE "Requesting object from network\n");

  // Add to interest list
  list_add(node->interests, object, fd, 0);

  sprintf(buffer, "INTEREST %s\n", object);

  // dont ask it from nodes already waiting for it
  ObjectList *interest = list_find(node->interests, object);

  // Ask all internals
  for (usize i = 0; i < node->internal_index; i++) {
    bool found_i = false;
    if (interest) {
      for (usize j = 0; j < interest->waiting_size; j++) {
        if (interest->waiting[j] == node->internal[i]->fd) {
          found_i = true;
          break;
        }
      }
    }

    if (found_i) {
      continue;
    }

    if (node->internal[i]->fd == -1 || node->internal[i]->fd == fd ||
        node->internal[i]->fd == node->external->fd) {
      continue;
    }

    if (write(node->internal[i]->fd, buffer, strlen(buffer)) < 0) {
      perror(ERR "writing INTEREST");
    }

    list_add(node->interests, object, 0, node->internal[i]->fd);
  }

  // Ask external
  if (node->external->fd != fd) {
    if (write(node->external->fd, buffer, strlen(buffer)) < 0) {
      perror(ERR "writing INTEREST");
    }

    list_add(node->interests, object, 0, node->external->fd);

    printf(NOTICE "Sent INTEREST to external\n");
  }

  interest = list_find(node->interests, object);
  for (usize i = 0; interest && i < interest->waiting_size; i++) {
    printf(NOTICE "Sent to %d\n", interest->waiting[i]);
  }
}

void ndn_object(Node *node, Object object, int senterfd) {
  cache_add(node, object);

  ObjectList *interest = list_find(node->interests, object);
  if (!interest) {
    return;
  }

  // Send to all interested nodes
  printf(NOTICE "Forwarding to... ");
  if (interest->response && interest->response_size > 0) {
    for (usize i = 0; i < interest->response_size; i++) {
      int from = interest->response[i];
      if (from == senterfd || from == -1) {
        continue;
      }

      char buffer[256];
      sprintf(buffer, "OBJECT %s\n", object);
      if (write(from, buffer, strlen(buffer)) < 0) {
        perror(ERR "writing OBJECT");
      }

      printf("%02d ", from);
    }
  }
  printf("\n");

  list_remove(node->interests, object);
}

void ndn_noobject(Node *node, Object object, int senderfd) {
  printf(NOTICE "Received NOOBJECT for object %s\n", object);

  ObjectList *interest = list_find(node->interests, object);
  if (interest == NULL) {
    printf(NOTICE "Not interested in this object\n");
    return;
  }

  // remove this fd from the waiting list
  for (usize i = 0; i < interest->waiting_size; i++) {
    if (interest->waiting[i] == senderfd) {
      for (usize j = i; j < interest->waiting_size - 1; j++) {
        interest->waiting[j] = interest->waiting[j + 1];
      }
      interest->waiting_size--;
      break;
    }
  }

  if (interest->waiting_size == 0) {
    char buffer[128];
    sprintf(buffer, "NOOBJECT %s\n", object);

    bool self_interest = false;

    printf(NOTICE "Failed to obtain object, propagating... ");
    for (usize i = 0; i < interest->response_size; i++) {
      if (interest->response[i] == -1) {
        self_interest = true;
        continue;
      }

      if (write(interest->response[i], buffer, strlen(buffer)) < 0) {
        perror("\n" ERR "writing NOOBJECT");
      }
    }
    printf("sent\n");

    if (self_interest)
      printf(ERR "Failed to retrieve: not in network\n");

    list_remove(node->interests, object);

    return;
  }

  printf(NOTICE "Still waiting for responses from other nodes\n");
}

/* TCP cancelation */

void ndn_exit__ext(Node *node);

void ndn_node_exit(Node *node, int fd) {
  // Remove this node from all interests
  ObjectList *prev = node->interests; // Start with sentinel node
  ObjectList *interest = prev->next;  // Skip sentinel node

  while (interest != NULL) {
    ObjectList *next_interest = interest->next;
    bool remove_interest = false;

    // Remove from interests

    // Remove from waiting list (noobject does this)
    ndn_noobject(node, interest->self, fd);

    // Remove from response list
    for (usize i = 0; i < interest->response_size; i++) {
      if (interest->response[i] == fd) {
        for (usize j = i; j < interest->response_size - 1; j++) {
          interest->response[j] = interest->response[j + 1];
        }
        interest->response_size--;
        break;
      }
    }

    // free the interest if no one is interested
    if (interest->response_size == 0) {
      if (!prev || !interest)
        return;

      // Update the previous node's next pointer to skip this node
      prev->next = interest->next;

      // Free all resources associated with this interest
      if (interest->self) {
        free(interest->self);
      }

      if (interest->waiting) {
        free(interest->waiting);
      }

      if (interest->response) {
        free(interest->response);
      }
    } else {
      prev = interest;
    }

    interest = next_interest;
  }

  if (node->external->fd == fd) {
    ndn_exit__ext(node);
    return;
  }

  if (node->safeguard->fd == fd) {
    printf(NOTICE "Safeguard disconnected\n");

    free(node->safeguard->ip);
    free(node->safeguard->tcp);
    node->safeguard->ip = NULL;
    node->safeguard->tcp = NULL;
    node->safeguard->fd = -1;
    // The other node will handle this
    return;
  }

  // It's an internal node or a node that has this one as safeguard

  for (usize i = 0; i < node->internal_index; i++) {
    if (node->internal[i]->fd == fd) {
      printf(NOTICE "Internal disconnected\n");

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
}

// The external node has disconnected
void ndn_exit__ext(Node *node) {
  printf(NOTICE "External disconnected\n");

  for (usize i = 0; i < node->internal_index; i++) {
    if (node->internal[i]->fd == node->external->fd) {
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
      node->safeguard->ip = NULL;
    }
    if (node->safeguard->tcp) {
      free(node->safeguard->tcp);
      node->safeguard->tcp = NULL;
    }

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
      // Find a valid internal to elevate
      int valid_internal_count = 0;
      usize *valid_indices = malloc(node->internal_index * sizeof(usize));

      if (!valid_indices) {
        perror(ERR "malloc");
        return;
      }

      // First, count valid internals and store their indices
      for (usize i = 0; i < node->internal_index; i++) {
        if (node->internal[i] && node->internal[i]->fd > 0) {
          valid_indices[valid_internal_count++] = i;
        }
      }

      if (valid_internal_count == 0) {
        // No valid internals to elevate
        printf(NOTICE
               "No valid internal nodes to elevate, becoming lone node\n");
        free(valid_indices);

        node->external->fd = -1;
        node->external->ip = strdup(node->ip);
        node->external->tcp = strdup(node->tcp);
        return;
      }

      // Choose a random valid internal to elevate
      usize rand_idx = rand() % valid_internal_count;
      usize i = valid_indices[rand_idx];
      free(valid_indices);

      // Choose a random internal to elevate to external
      printf(NOTICE "Elevating random internal to external\n");

      node->external->fd = node->internal[i]->fd;

      // Use strdup to create new copies of the strings
      if (node->external->ip)
        free(node->external->ip);
      if (node->external->tcp)
        free(node->external->tcp);

      node->external->ip = strdup(node->internal[i]->ip);
      node->external->tcp = strdup(node->internal[i]->tcp);

      // Now free the internal node
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

      // Verify we still have a valid connection before trying to write
      if (node->external->fd <= 0) {
        printf(ERR "Selected internal has invalid fd %d\n", node->external->fd);
        node->external->fd = -1;
        return;
      }

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
