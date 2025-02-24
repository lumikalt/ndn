#pragma once

#include "../types.h"

NetNode *ndn_nodes(Server *s, u16 net);

void ndn_register(Node *node, u16 net);

void ndn_unregister(Node *node, u16 net);
