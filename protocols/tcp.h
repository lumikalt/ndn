#pragma once

#include "../types.h"

/* Topology protocol */

// Node requesting link
void ndn_entry(Node *node, char *ip, char *tcp);

// Node sending its external link
void ndn_safe(Node *, char *ip, char *tcp);

/* Object protocol */

// Node interested in an object
void ndn_interest(Node *, Object);

// Node has the object
void ndn_object(Node *, Object);

// Node doesn't have the object
void ndn_noobject(Node *, Object);
