#pragma once

#include "../types.h"

NodeList *ndn_nodes(Node *, u16 net);

void ndn_register(Node *, u16 net);

void ndn_unregister(Node *, u16 net);
