#pragma once

#include "types.h"

#include <sys/select.h>

void user_in(Node *node, fd_set *read_fds, int listener_fd);
