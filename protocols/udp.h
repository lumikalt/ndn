#pragma once

#include "../types.h"

/// Get the list of nodes in the network
NodeList *ndn_nodes(Node *);

/// Register the node in the network
void ndn_register(Node *);

/// Unregister the node from the network
void ndn_unregister(Node *);

/// Test if the server is responsive
bool ndn_ping_server(Node *);
