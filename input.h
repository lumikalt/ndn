#pragma once

#include "types.h"

#include <sys/select.h>

void user_in(Node *node);

void process_input_commands(Node *node, char *input);
