#pragma once

//void *user_in(void *);

#include <sys/select.h>
#include "types.h"

void user_in(Node* node, fd_set *read_fds, int listener_fd);
