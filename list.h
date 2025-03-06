#pragma once

#include <stddef.h>

typedef size_t usize;

typedef char *Object;

typedef struct ObjectList {
  Object self;

  // interested node
  char *ip;
  char *tcp;

  struct ObjectList *next;
} ObjectList;

ObjectList *list_create();
void list_add(ObjectList *list, Object object, char *ip, char *tcp);
void list_remove(ObjectList *list, Object object);
void list_destroy(ObjectList *list);
void list_print(ObjectList *list);
void list_print_interests(ObjectList *list);
ObjectList *list_find(ObjectList *list, Object object);
usize list_size(ObjectList *list);
