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
  int fd;
  char *msg;
} FDMsg;

// The Node type (everything is here)
typedef struct Node {
  ObjectList *objects;
  ObjectList *interests;
  Object *cache;
  usize cache_size;

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
  int listener_fd;

  // IO handling thread
  pthread_t thread;
  bool exit;
} Node;
