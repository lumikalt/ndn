#pragma once

#include "../types.h"

NodeList *ndn_nodes(Node *node, u16 net);

void ndn_register(Node *node, u16 net);

void ndn_unregister(Node *node, u16 net);
