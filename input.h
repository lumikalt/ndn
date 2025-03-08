#pragma once

#include "types.h"

#include <sys/select.h>

void ndn_inputs(Node *node);

void *user_input(void *node);
