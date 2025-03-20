#pragma once

#include "types.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define CYAN "\033[0;36m"
#define WHITE "\033[0;37m"
#define RESET "\033[0m"
#define CLEAR "\033[H\033[J"

#define ERR RED "ERR\t" RESET
#define WARN YELLOW "WARN\t" RESET
#define OK GREEN "OK\t" RESET
#define NOTICE CYAN "INFO\t" RESET

Node *init_node(usize cache_size, char *node_IP, char *node_TCP, char *regIP,
                char *regUDP);

void clean_node(Node *node);

int is_valid_net(char *net);
int is_valid_ip(char *ip);
int is_valid_port(char *port);
int is_valid_name(char *name);

usize str_char_count(const char *s, char c);
char *str_escape(const char *s);

void clean_nodelist(NodeList *nodes);

void grow_internal(Node *node);
