#include "./commands.h"
#include "./types.h"

#include <stdio.h>

void ndn_help() {
  printf("Commands:\n"
         "\t(h)  help - show this message\n"
         "\t(j)  join <net> - join the network (000-999)\n"
         "\t(dj) direct join <net> <IP> <TCP> - directly join a network\n"
         "\t(c)  create <name> - create an object\n"
         "\t(dl) delete <name> - delete an object\n"
         "\t(r)  retrieve <name> - retrieve an object\n"
         "\t(st) show_topology - show the neighbourhood's topology\n"
         "\t(sn) show_names - show the object names in this node\n"
         "\t(si) show_interest_table - show the interest table\n"
         "\t(l)  leave - leave the network\n"
         "\t(x)  exit - close the program\n");
}

void ndn_join(Server *s, u16 net) {}

void ndn_direct_join(u16 net, char *connectIP, char *connectTCP) {
  printf("Directly joining network %d, linking to %s:%s\n", net, connectIP,
         connectTCP);
}

void ndn_create(const char *name) { printf("Creating object %s\n", name); }

void ndn_delete(const char *name) { printf("Deleting object %s\n", name); }

void ndn_retrieve(const char *name) { printf("Retrieving object %s\n", name); }

void ndn_show_topology() { printf("Showing network topology\n"); }

void ndn_show_names() { printf("Showing object names\n"); }

void ndn_show_interest_table() { printf("Showing interest table\n"); }

void ndn_leave() { printf("Leaving network\n"); }

void ndn_exit() { printf("Exiting program\n"); }
