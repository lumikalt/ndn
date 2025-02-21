#pragma once

#include "./types.h"

#include <netdb.h>

void ndn_help();

void ndn_join(Server *s, u16 net);

void ndn_direct_join(u16 net, char *connectIP, char *connectTCP);

void ndn_create(const char *name);

void ndn_delete(const char *name);

void ndn_retrieve(const char *name);

void ndn_show_topology();

void ndn_show_names();

void ndn_show_interest_table();

void ndn_leave();

void ndn_exit();
