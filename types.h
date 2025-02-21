#pragma once

#include <netdb.h>
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
    int serverfd;

    char *IP;
    char *TCP;
} Server;

typedef struct {
    struct addrinfo *tcp;
    int listenerfd;

    char *IP;
    char *TCP;
} Adjacencies;
