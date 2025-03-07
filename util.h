#pragma once

#include "types.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

usize str_char_count(const char *s, char c);

Node *init_node(usize cache_size, char *node_IP, char *node_TCP, char *regIP,
                char *regUDP);

int is_valid_net(char *net);

int is_valid_ip(char *ip);

int is_valid_port(char *port);

int is_valid_name(char *name);

void errored(const char *msg, Node *node);
