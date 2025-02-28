#pragma once

#include <stddef.h>

typedef size_t usize;

typedef char *Object;

typedef struct ObjectList {
  Object self;

  struct ObjectList *next;
} ObjectList;

ObjectList *list_create();
void list_add(ObjectList *list, Object object);
void list_remove(ObjectList *list, Object object);
void list_destroy(ObjectList *list);
void list_print(ObjectList *list);
Object list_find(ObjectList *list, Object object);
Object list_pop(ObjectList *list);
usize list_size(ObjectList *list);
