#include "util.h"
#include "commands.h"

#include <netdb.h>
#include <unistd.h>

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

void process_input_commands(Node* node ,char *input) {
  char net[4], ip[16], port[6], name[101];
  int pos;

  //---join---
  if ((sscanf(input, "join %3s%n", net, &pos) == 1 && input[pos] == '\0') ||
      (sscanf(input, "j %3s%n", net, &pos) == 1 && input[pos] == '\0')) {

    if (!is_valid_net(net)) {
      printf("Wrong input, it must be 3 digits.\n");
      return;
    }

    ndn_join(node, atoi(net) );
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
    printf("Deleted object '%s'\n", name);
    return;
  }
  //----------

  // if the command does not exist
  printf("That command does not exist. Please type 'help' for the list of "
         "commands\n");
}
