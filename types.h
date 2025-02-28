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
  struct addrinfo *udp;
  int fd;

  char *IP;
  char *TCP;
} Server;

typedef struct {
  struct addrinfo *tcp;
  int listenerfd;

  usize node_index;
} AdjacentNode;

typedef struct {
  char **IP;
  char **TCP;

  // Sadly this needs to be dynamic, the response doesn't include the number of
  // nodes
  usize size;
  usize capacity;
} NodeList;

typedef char *Object;

typedef struct {
  ObjectList *objects;
  ObjectList *interests;
  Object *cache;
  usize cache_size;

  AdjacentNode *safeguard;
  AdjacentNode *external;
  AdjacentNode **internal;

  Server *server;

  // self connection address
  char *IP;
  char *TCP;
} Node;
