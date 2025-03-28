#pragma once

#include "list.h"

#include <netdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef size_t usize;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// UDP server connection
typedef struct {
  struct addrinfo *addr;
  int fd;

  char *ip;
  char *udp;
} Server;

// Adjacent node TCP connection
typedef struct {
  int fd;

  char *ip;
  char *tcp;
} AdjacentNode;

// List of nodes in the current network
typedef struct {
  char **ip;
  char **tcp;

  usize size;
} NodeList;

// Object type
typedef char *Object;

typedef struct {
  int udp_timeout;
  int tcp_timeout;
} Config;

// The Node type (everything is here)
typedef struct Node {
  ObjectList *objects;
  ObjectList *interests;
  Object *cache; // Circular buffer
  usize cache_size;
  usize cache_head;  // Points to oldest element
  usize cache_count; // Current number of elements

  AdjacentNode *safeguard;
  AdjacentNode *external;
  AdjacentNode **internal;
  usize internal_index;
  usize internal_capacity;

  Server *server;

  // self address
  char *ip;
  char *tcp;
  bool in_net;
  usize net;

  // TCP listener
  int listener_fd, max_fd;
  fd_set master_fds;
  char **last_msgs;
  usize last_msgs_capacity;

  // IO handling thread
  pthread_t thread;
  bool exit;

  Config config;
} Node;
