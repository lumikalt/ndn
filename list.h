#pragma once

#include <stddef.h>

typedef size_t usize;

typedef char *Object;

typedef struct ObjectList {
  Object self;
  // Who is interested in this object
  int *response;
  usize response_size;
  // Who it was sent to
  int *waiting;
  usize waiting_size;

  struct ObjectList *next;
} ObjectList;

ObjectList *list_create();
void list_add(ObjectList *list, Object object, int response, int waiting);
void list_remove(ObjectList *list, Object object);
void list_destroy(ObjectList *list);
void list_print(ObjectList *list);
void list_print_interests(int externalfd, ObjectList *list);
ObjectList *list_find(ObjectList *list, Object object);
usize list_size(ObjectList *list);
