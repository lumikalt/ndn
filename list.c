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
  list->response = NULL;
  list->response_size = 0;
  list->waiting = NULL;
  list->waiting_size = 0;
  list->next = NULL;
  return list;
}

void list_add(ObjectList *list, Object object, int response, int waiting) {
  ObjectList *any = list_find(list, object);
  if (any != NULL) {
    if (response != 0) {
      any->response_size++;
      any->response = realloc(any->response, any->response_size * sizeof(int));
      if (!any->response) {
        perror(ERR "realloc");
        exit(1);
      }
      any->response[any->response_size - 1] = response;
    }

    if (waiting != 0) {
      any->waiting_size++;
      any->waiting = realloc(any->waiting, any->waiting_size * sizeof(int));
      if (!any->waiting) {
        perror(ERR "realloc");
        exit(1);
      }
      any->waiting[any->waiting_size - 1] = waiting;
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

  new_node->response = malloc(sizeof(int));
  if (!new_node->response) {
    perror(ERR "malloc");
    free(new_node->self);
    free(new_node);
    exit(1);
  }
  new_node->response_size = 0;
  if (response != 0) {
    new_node->response[0] = response;
    new_node->response_size = 1;
  }
  new_node->waiting = malloc(sizeof(int));
  if (!new_node->waiting) {
    perror(ERR "malloc");
    free(new_node->response);
    free(new_node->self);
    free(new_node);
    exit(1);
  }
  new_node->waiting_size = 0;
  if (waiting != 0) {
    new_node->waiting[0] = waiting;
    new_node->waiting_size = 1;
  }
  new_node->next = NULL;
  current->next = new_node;
}

void list_remove(ObjectList *list, Object object) {
  ObjectList *current = list;

  while (current->next != NULL) {
    // Use strcmp to compare string content instead of comparing pointers
    if (strcmp(current->next->self, object) == 0) {
      // Remove the node after current
      ObjectList *to_remove = current->next;
      current->next = to_remove->next;

      // Free resources
      free(to_remove->self);
      if (to_remove->response)
        free(to_remove->response);
      if (to_remove->waiting)
        free(to_remove->waiting);
      free(to_remove);
      return;
    }
    current = current->next;
  }
}

void list_destroy(ObjectList *list) {
  ObjectList *current = list;
  while (current != NULL) {
    ObjectList *next = current->next;

    if (current->self)
      free(current->self);
    if (current->response)
      free(current->response);
    if (current->waiting)
      free(current->waiting);
    free(current);
    current = next;
  }
}

void list_print(ObjectList *list) {
  ObjectList *current = list->next;

  if (current == NULL) {
    printf(RESET "\t(empty)\n");
    return;
  }

  while (current != NULL) {
    printf(NOTICE "> `%s`\n", current->self);
    current = current->next;
  }
}

void list_print_interests(int externalfd, ObjectList *list) {
  ObjectList *current = list->next;

  if (current == NULL) {
    printf(RESET "\t(empty)\n");
    return;
  }

  while (current != NULL) {
    printf(RESET "\t- `%s`\n", current->self);

    for (usize i = 0; i < current->response_size; i++) {
      printf("\t  -> fd_%02d%s", current->response[i],
             current->response[i] == externalfd ? " (external)"
             : current->response[i] == -1       ? " (self)"
                                                : "");
    }

    for (usize i = 0; i < current->waiting_size; i++) {
      printf("\t  <- fd_%02d%s\n", current->waiting[i],
             current->waiting[i] == -1 ? " (external)" : "");
    }

    current = current->next;
  }
}

ObjectList *list_find(ObjectList *list, Object object) {
  ObjectList *current = list->next;

  // Also add NULL check on object
  if (object == NULL)
    return NULL;

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
