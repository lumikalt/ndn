#include "./commands.h"
#include <stdio.h>

void ndn_help() {
  printf("Commands:\n"
         "  (h)  help - show this message\n"
         "  (j)  join <net> - join the network (000-999)\n"
         "  (st) show_topology - show the network topology\n"
         "  (x)  exit - close the program\n");
}

void ndn_join(u16 net) { printf("Joining network %d\n", net); }

void ndn_show_topology() { printf("Showing network topology\n"); }

void ndn_exit() { printf("Exiting program\n"); }
