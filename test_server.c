// Test server for the UDP communication

#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

int main() {
    // Create a UDP server
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in server_addr;


}
