#pragma once

#include <stddef.h>

typedef size_t usize;

typedef char *Object;

typedef struct ObjectList {
  Object self;
  int fd;

  struct ObjectList *next;
} ObjectList;

ObjectList *list_create();
void list_add(ObjectList *list, Object object, int fd);
void list_remove(ObjectList *list, Object object);
void list_remove_connection(ObjectList *list, int fd);
void list_destroy(ObjectList *list);
void list_print(ObjectList *list);
void list_print_interests(int externalfd, ObjectList *list);
ObjectList *list_find(ObjectList *list, Object object);
ObjectList *list_find_connection(ObjectList *list, int fd);
usize list_size(ObjectList *list);
