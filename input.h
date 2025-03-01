#pragma once

//void *user_in(void *);

#include <sys/select.h>
void user_in(fd_set *read_fds, int listener_fd);
