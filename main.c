#include "input.h"
#include "util.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc < 4) {
    fprintf(stderr,
            RED "ERR" RESET "\tUsage: %s <cache> <IP> <TCP> "
                "[regIP=193.136.138.142] [regUDP=59000]\n"
                "  cache  - size of the node cache\n"
                "  IP     - IP address of the server\n"
                "  TCP    - TCP port to listen on\n"
                "  regIP  - IP address of the node\n"
                "  regUDP - UDP port of the node\n",
            argv[0]);
    return 1;
  }

  /* Connect via UDP to the server */

  usize cache = atoi(argv[1]);
  char *IP = argv[2];
  char *TCP = argv[3];
  char *regIP = argc > 4 ? argv[4] : "193.136.138.142";
  char *regUDP = argc > 5 ? argv[5] : "59000";

  Node *node = init_node(cache, IP, TCP, regIP, regUDP);

  // Start the single-threaded event loop
  ndn_run(node);

  // We only get here if ndn_run returns, which happens on exit
  return 0;
}
