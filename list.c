#include "list.h"
#include <stdio.h>
#include <stdlib.h>

ObjectList *list_create() {
  ObjectList *list = malloc(sizeof(ObjectList));
  list->self = NULL;
  list->next = NULL;
  return list;
}

void list_add(ObjectList *list, Object object) {
  ObjectList *current = list;
  while (current->next != NULL) {
    current = current->next;
  }
  current->next = malloc(sizeof(ObjectList));
  current->next->self = object;
  current->next->next = NULL;
}

void list_remove(ObjectList *list, Object object) {
  ObjectList *current = list;
  while (current->next != NULL) {
    if (current->next->self == object) {
      ObjectList *temp = current->next;
      current->next = current->next->next;
      free(temp);
      return;
    }
    current = current->next;
  }
}

void list_destroy(ObjectList *list) {
  ObjectList *current = list;
  while (current != NULL) {
    ObjectList *temp = current;
    current = current->next;
    free(temp);
  }
}

void list_print(ObjectList *list) {
  ObjectList *current = list;
  while (current->next != NULL) {
    printf("\t%s\n", current->next->self);
    current = current->next;
  }
}

Object list_find(ObjectList *list, Object object) {
  ObjectList *current = list;
  while (current->next != NULL) {
    if (current->next->self == object) {
      return current->next->self;
    }
    current = current->next;
  }
  return NULL;
}

usize list_size(ObjectList *list) {
  usize size = 0;
  ObjectList *current = list;
  while (current->next != NULL) {
    size++;
    current = current->next;
  }
  return size;
}
