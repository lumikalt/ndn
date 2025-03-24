#include "list.h"
#include "util.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ObjectList *list_create() {
  ObjectList *list = malloc(sizeof(ObjectList));
  if (!list) {
    perror(ERR "malloc");
    exit(1);
  }
  list->self = NULL;
  list->by = NULL;
  list->by_size = 0;
  list->to = NULL;
  list->to_size = 0;
  list->next = NULL;
  return list;
}

void list_add(ObjectList *list, Object object, int by, int to) {
  ObjectList *any = list_find(list, object);
  if (any != NULL) {
    any->by_size++;

    if (by != 0) {
      any->by = realloc(any->by, any->by_size * sizeof(int));
      if (!any->by) {
        perror(ERR "realloc");
        exit(1);
      }
      any->by[any->by_size - 1] = by;
    }

    if (to == 0) {
      any->to_size++;
      any->to = realloc(any->to, any->to_size * sizeof(int));
      if (!any->to) {
        perror(ERR "realloc");
        exit(1);
      }
      any->to[any->to_size - 1] = to;
    }

    return;
  }

  ObjectList *current = list;
  while (current->next != NULL) {
    current = current->next;
  }

  ObjectList *new_node = malloc(sizeof(ObjectList));
  if (!new_node) {
    perror(ERR "malloc");
    exit(1);
  }

  // Create a copy of the string on the heap
  new_node->self = strdup(object);
  if (!new_node->self) {
    perror(ERR "strdup");
    free(new_node);
    exit(1);
  }

  new_node->by = malloc(sizeof(int));
  new_node->by[0] = by;
  new_node->by_size = 1;
  new_node->to = malloc(sizeof(int));
  new_node->to[0] = to;
  new_node->to_size = 1;
  new_node->next = NULL;
  current->next = new_node;
}

void list_remove(ObjectList *list, Object object) {
  ObjectList *current = list;
  ObjectList *prev = NULL;

  while (current != NULL) {
    if (current->self == object) {
      if (prev == NULL) {
        list = current->next;
      } else {
        prev->next = current->next;
      }
      free(current);
      return;
    }
    prev = current;
    current = current->next;
  }
}

void list_destroy(ObjectList *list) {
  ObjectList *current = list;
  while (current != NULL) {
    ObjectList *next = current->next;

    if (current->self)
      free(current->self);
    if (current->by)
      free(current->by);
    if (current->to)
      free(current->to);
    free(current);
    current = next;
  }
}

void list_print(ObjectList *list) {
  ObjectList *current = list;
  while (current != NULL) {
    printf(NOTICE "> `%s`\n", current->self);
    current = current->next;
  }
}

void list_print_interests(int externalfd, ObjectList *list) {
  ObjectList *current = list;
  while (current != NULL) {
    printf(RESET "\t- `%s`\n", current->self);

    for (usize i = 0; i < current->by_size; i++) {
      printf("\t  -> fd_%02d%s", current->by[i],
             current->by[i] == externalfd ? " (external)"
             : current->by[i] == -1       ? " (self)"
                                          : "");
    }

    for (usize i = 0; i < current->to_size; i++) {
      printf("\t  <- fd_%02d%s\n", current->to[i],
             current->to[i] == -1 ? " (external)" : "");
    }

    current = current->next;
  }
}

ObjectList *list_find(ObjectList *list, Object object) {
  ObjectList *current = list;

  while (current != NULL) {
    if (current->self != NULL && strcmp(current->self, object) == 0) {
      return current;
    }
    current = current->next;
  }

  return NULL;
}

usize list_size(ObjectList *list) {
  usize size = 0;
  ObjectList *current = list;
  while (current != NULL) {
    size++;
    current = current->next;
  }
  return size;
}
