#pragma once

#include "../types.h"

/* Topology protocol */

void ndn_entry(Node *);

void ndn_safe(Node *);

/* Object protocol */

void ndn_interest(Node *);

void ndn_object(Node *);

void ndn_noobject(Node *);
