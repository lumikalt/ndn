#pragma once

#include "./types.h"

#include <netdb.h>

void ndn_help();

void ndn_join(Node *, u16 net);

void ndn_direct_join(Node *, u16 net, char *connectIP, char *connectTCP);

void ndn_create(Node *, const char *name);

void ndn_delete(Node *, const char *name);

void ndn_retrieve(Node *, const char *name);

void ndn_show_topology(Node *);

void ndn_show_names(Node *);

void ndn_show_interest_table(Node *);

void ndn_leave(Node *);

void ndn_exit(Node *);
