// Test server for the UDP communication

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char **argv) {
  // Create a UDP server
  int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (server_fd == -1) {
    perror("socket");
    exit(1);
  }

  struct sockaddr_in server_addr;

  // use getaddrinfo to get the address info

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(argv[1]));
  server_addr.sin_addr.s_addr = INADDR_ANY;

  // Bind the socket to the address

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror("bind");
    exit(1);
  }

  // Receive data from the client
  char buffer[128];
  struct sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  int bytes_received =
      recvfrom(server_fd, buffer, sizeof(buffer), 0,
               (struct sockaddr *)&client_addr, &client_addr_len);
  if (bytes_received == -1) {
    perror("recvfrom");
    exit(1);
  }

  printf("Received %d bytes from %s:%d\n~>\t`%s`\n", bytes_received,
         inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

  // Send response to the client
  char response[] = "NODESLIST 123\n"
                    "123.456.789 321431\n"
                    "879.423.789 214354\n"
                    "432.456.890 408321\n";

  printf("Sending:\n---\n%s\n---\n", response);

  int bytes_sent = sendto(server_fd, response, sizeof(response), 0,
                          (struct sockaddr *)&client_addr, client_addr_len);
  if (bytes_sent == -1) {
    perror("sendto");
    exit(1);
  }

  printf("Sent %d bytes to %s:%d\n", bytes_sent,
         inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

  int code;
loop:
  scanf("%d", &code);
  if (code != 1)
    goto loop;

  close(server_fd);
  return 0;
}
