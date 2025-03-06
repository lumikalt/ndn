#include "list.h"
#include "types.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>

ObjectList *list_create() {
  ObjectList *list = malloc(sizeof(ObjectList));
  list->self = NULL;
  list->ip = NULL;
  list->tcp = NULL;
  list->next = NULL;
  return list;
}

void list_add(ObjectList *list, Object object, char *ip, char *tcp) {
  ObjectList *current = list;
  while (current->next != NULL) {
    current = current->next;
  }
  current->next = malloc(sizeof(ObjectList));
  current->next->self = object;
  current->next->ip = ip;
  current->next->tcp = tcp;
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

void list_print_interests(ObjectList *list) {
  ObjectList *current = list;
  while (current->next != NULL) {
    printf("\t%s %s:%s\n", current->next->self,
           current->next->ip,
           current->next->tcp);
    current = current->next;
  }
}

ObjectList *list_find(ObjectList *list, Object object) {
  ObjectList *current = list;
  while (current->next != NULL) {
    if (current->next->self == object) {
      return current->next;
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
