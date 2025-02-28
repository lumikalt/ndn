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

typedef struct {
  struct addrinfo *addr;
  int fd;
} Server;

typedef struct {
  struct addrinfo *addr;
  int fd;

  usize node_index;
} AdjacentNode;

typedef struct {
  char **ip;
  char **tcp;

  // Ugly
  usize size;
  usize capacity;
} NodeList;

typedef char *Object;

typedef struct {
  ObjectList *objects;
  ObjectList *interests;
  Object *cache;
  usize cache_size;

  NodeList *network;
  AdjacentNode *safeguard;
  AdjacentNode *external;
  AdjacentNode **internal;

  Server *server;

  // self address
  char *ip;
  char *tcp;
} Node;
