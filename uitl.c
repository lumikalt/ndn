#include "util.h"


usize str_char_count(const char *s, char c) {
  usize count = 0;
  for (usize i = 0; s[i]; s[i] == c ? count++, i++ : i++);
  return count;
}


int is_valid_net(char *net) {
  return strlen(net) == 3 && isdigit(net[0]) && isdigit(net[1]) && isdigit(net[2]);
}

int is_valid_ip(char *ip) {
  struct sockaddr_in sa;
  return inet_pton(AF_INET, ip, &sa.sin_addr);
}

int is_valid_port(char *port) {
  char *end;
  long p = strtol(port, &end, 10);
  return *end == '\0' && p > 0 && p <= 65535;//NOTE: confirmar se é o numero certo
}

int is_valid_name(char *name) {
  size_t len = strlen(name);
  if (len == 0 || len > 100) return 0;
  for (size_t i = 0; i < len; i++) {
    if (!isalnum(name[i])) return 0;
  }
  return 1;
}



void process_input_commands(char *input) {
  char net[4], ip[16], port[6], name[101];
  int pos;

  //---join---
  if ((sscanf(input, "join %3s%n", net, &pos) == 1 && input[pos] == '\0') ||(sscanf(input, "j %3s%n", net, &pos) == 1 && input[pos] == '\0')) {

    if (!is_valid_net(net)) {
      printf("Wrong input, it must be 3 digits.\n");
      return;
    }

    printf("Joining network %s...\n", net);
    return;
    }
  //----------


  //---direct join---
  if ((sscanf(input, "direct join %3s %15s %5s%n", net, ip, port, &pos) == 3 && input[pos] == '\0') || (sscanf(input, "dj %3s %15s %5s%n", net, ip, port, &pos) == 3 && input[pos] == '\0')) {

    if (!is_valid_net(net)) {
      printf("Invalid network ID\n");
    } else if (!is_valid_ip(ip)) {
      printf("Invalid IP address\n");
    } else if (!is_valid_port(port)) {
      printf("Invalid port number\n");
    } else {
      printf("Direct joining %s via %s:%s\n", net, ip, port);


      if (strcmp(ip, "0.0.0.0") == 0) {
        printf("Created new network\n");
      }

    }
    return;
  }

  //----------

   //---create---
    if ((sscanf(input, "create %100s%n", name, &pos) == 1 && input[pos] == '\0') ||(sscanf(input, "c %100s%n", name, &pos) == 1 && input[pos] == '\0')) {

      if (!is_valid_name(name)) {
        printf("Invalid name (alphanumeric, 1-100 chars)\n");
        return;
      }

      printf("Created object '%s'\n", name);
      return;
    }
    //----------

    //---delete---
    if ((sscanf(input, "delete %100s%n", name, &pos) == 1 && input[pos] == '\0') || (sscanf(input, "dl %100s%n", name, &pos) == 1 && input[pos] == '\0')) {

      if (!is_valid_name(name)) {
        printf("Invalid name\n");
        return;

      }
      printf("Deleted object '%s'\n", name);
    return;
    }
    //----------


    //if the command does not exist
    printf("That command does not exist. Please type 'help' for the list of commands\n");
}
