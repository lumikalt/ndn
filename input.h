#pragma once

#include "types.h"

#include <sys/select.h>

void ndn_run(Node *node);

void process_user_input(Node *node, char *input);
