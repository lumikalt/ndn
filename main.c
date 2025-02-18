#include "./input.h"
#include "./types.h"

#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  if (argc < 4) {
    printf(
        "Usage: %s <cache> <IP> <TCP> [regIP=193.136.138.142] [regUDP=59000]\n"
        "  cache  - size of the node cache\n"
        "  IP     - IP address of the server\n"
        "  TCP    - TCP port to listen on\n"
        "  regIP  - IP address of the node\n"
        "  regUDP - UDP port of the node\n",
        argv[0]);
    return 1;
  }

  usize cache = atoi(argv[1]);
  char *IP = argv[2];
  usize TCP = atoi(argv[3]);
  char *regIP = argc > 4 ? argv[4] : "193.136.138.142";
  usize regUDP = argc > 5 ? atoi(argv[5]) : 59000;

  pthread_t input_thread;
  pthread_create(&input_thread, NULL, user_in, NULL);

  /*
   * TODO: Ensure graceful shutdown of the program.
   */

  pthread_join(input_thread, NULL);

  return 0;
}
