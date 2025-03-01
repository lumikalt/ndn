#pragma once

#include "types.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

usize str_char_count(const char *s, char c);

void process_input_commands(Node* node ,char *input);

int is_valid_net(char *net);

int is_valid_ip(char *ip);

int is_valid_port(char *port);

int is_valid_name(char *name);
