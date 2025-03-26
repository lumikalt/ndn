#include "udp.h"
#include "../util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void set_udp_timeout(int sockfd, int timeout_sec) {
  struct timeval tv;
  tv.tv_sec = timeout_sec;
  tv.tv_usec = 0;

  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    perror(ERR "setsockopt timeout");
  }
}

ssize_t udp_send_with_retry(Node *node, const char *send_buffer,
                            char *response_buffer, size_t response_size,
                            const char *expected_prefix, int max_retries) {
  Server *s = node->server;
  ssize_t n;
  int retries = 0;
  int timeout_sec = 1; // Start with 1 second timeout

  while (retries < max_retries) {
    // Send request
    if ((n = sendto(s->fd, send_buffer, strlen(send_buffer), 0,
                    s->addr->ai_addr, s->addr->ai_addrlen)) <= 0) {
      fprintf(stderr, ERR "Failed to send request (attempt %d/%d)\n",
              retries + 1, max_retries);
      retries++;
      timeout_sec *= 2;
      continue;
    }

    // Set up for select()
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(s->fd, &read_fds);

    struct timeval tv;
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;

    int select_ret = select(s->fd + 1, &read_fds, NULL, NULL, &tv);

    if (select_ret == 0) {
      // Timeout occurred
      fprintf(stderr, WARN "Timeout, retry %d/%d\n", retries + 1, max_retries);
      retries++;
      timeout_sec *= 2;
      continue;
    } else if (select_ret < 0) {
      // Error in select()
      perror("select");
      retries++;
      timeout_sec *= 2;
      continue;
    }

    // Data available, receive response
    socklen_t addr_len = s->addr->ai_addrlen;
    if ((n = recvfrom(s->fd, response_buffer, response_size - 1, 0,
                      s->addr->ai_addr, &addr_len)) <= 0) {
      fprintf(stderr, ERR "Failed to receive response (attempt %d/%d)\n",
              retries + 1, max_retries);
      retries++;
      timeout_sec *= 2;
      continue;
    }

    // Null-terminate and validate response
    response_buffer[n] = '\0';
    if (expected_prefix && strncmp(response_buffer, expected_prefix,
                                   strlen(expected_prefix)) != 0) {
      fprintf(stderr, ERR "Unexpected response format\n");
      retries++;
      timeout_sec *= 2;
      continue;
    }

    return n;
  }

  fprintf(stderr, ERR "Max retries (%d) reached, giving up\n", max_retries);
  return -1;
}

NodeList *ndn_nodes(Node *node) {
  Server *s = node->server;
  u16 net = node->net;
  NodeList *nodes = malloc(sizeof(NodeList));
  if (!nodes) {
    perror(ERR "malloc");
    return NULL;
  }
  nodes->size = 0;
  nodes->ip = NULL;
  nodes->tcp = NULL;

  ssize_t n;
  char buffer[256];

  sprintf(buffer, "NODES %03d", net);

  // Set a 3-second timeout for UDP responses
  set_udp_timeout(s->fd, 3);

  char *response = calloc(4096, sizeof(char));
  if (!response) {
    perror(ERR "calloc");
    clean_nodelist(nodes);
    return NULL;
  }

  // Use the retry mechanism for sending/receiving
  char ok_prefix[17];
  sprintf(ok_prefix, "NODESLIST %03d\n", net);

  n = udp_send_with_retry(node, buffer, response, 4096, ok_prefix, 3);
  if (n <= 0) {
    fprintf(stderr, ERR "Failed to get nodes list after retries\n");
    free(response);
    clean_nodelist(nodes);
    return NULL;
  }

  char *string = response + strlen(ok_prefix);

  printf(NOTICE "Parsing nodes\n");

  /* Parse the string */

  usize newlines = str_char_count(string, '\n');

  nodes->ip = malloc(newlines * sizeof(char *));
  nodes->tcp = malloc(newlines * sizeof(char *));
  if (!nodes->ip || !nodes->tcp) {
    perror(ERR "malloc");
    free(response);
    clean_nodelist(nodes);
    return NULL;
  }

  for (usize i = 0; i < newlines; i++) {
    // Every line is in the format IP TCP\n
    char ip[100] = {0}; // Initialize arrays to zeros
    char tcp[6] = {0};

    // Check if sscanf was successful
    int parsed = sscanf(string, "%99s %5s\n", ip, tcp);
    if (parsed != 2) {
      fprintf(stderr, ERR "Failed to parse node entry: '%s'\n",
              str_escape(string));
      continue; // Skip this entry
    }

    // Validate IP and port before continuing
    if (!is_valid_ip(ip) || !is_valid_port(tcp)) {
      fprintf(stderr, ERR "Invalid IP/port: %s %s\n", ip, tcp);
      continue;
    }

    string += strlen(ip) + strlen(tcp) + 2;

    nodes->ip[nodes->size] = malloc(strlen(ip) + 1);
    nodes->tcp[nodes->size] = malloc(strlen(tcp) + 1);
    if (!nodes->ip[nodes->size] || !nodes->tcp[nodes->size]) {
      perror(ERR "malloc");
      free(response);
      clean_nodelist(nodes);
      return NULL;
    }
    strcpy(nodes->ip[nodes->size], ip);
    strcpy(nodes->tcp[nodes->size], tcp);

    nodes->size++;
  }

  free(response);

  return nodes;
}

void ndn_register(Node *node) {
  char buffer[256];
  char response_buffer[128];
  u16 net = node->net;

  sprintf(buffer, "REG %03u %s %s", net, node->ip, node->tcp);

  printf(NOTICE "Requesting registration in net %03u\n", net);

  // Use the retry mechanism
  ssize_t n = udp_send_with_retry(node, buffer, response_buffer,
                                  sizeof(response_buffer), "OKREG", 3);

  if (n <= 0) {
    fprintf(stderr, ERR "Failed to register after multiple attempts\n");
    return;
  }

  printf(OK "Successfully registered\n");
}

void ndn_unregister(Node *node) {
  char buffer[256];
  char response_buffer[128];

  sprintf(buffer, "UNREG %03zu %s %s", node->net, node->ip, node->tcp);

  printf(NOTICE "Requesting unregistration\n");

  // Use the retry mechanism
  ssize_t n = udp_send_with_retry(node, buffer, response_buffer,
                                  sizeof(response_buffer), "OKUNREG", 3);

  if (n <= 0) {
    fprintf(stderr, ERR "Failed to unregister after multiple attempts\n");
    return;
  }

  printf(OK "Successfully left network %zu\n", node->net);

  node->in_net = false;
  node->net = 1000;
}
