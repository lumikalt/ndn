#include "list.h"
#include "util.h"

#include <arpa/inet.h>
#include <netdb.h>
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
  list->fd = -1;
  list->next = NULL;
  return list;
}

void list_add(ObjectList *list, Object object, int fd) {
  if (list_find(list, object) != NULL) {
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
  new_node->self = object;
  new_node->fd = fd;
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

void list_remove_connection(ObjectList *list, int fd) {
  ObjectList *current = list;
  ObjectList *prev = NULL;

  while (current != NULL) {
    if (current->fd == fd) {
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
    ObjectList *temp = current;
    current = current->next;
    free(temp);
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
    printf(RESET "\t> `%s` => fd_%02d%s\n", current->self, current->fd,
           current->fd == externalfd ? " (external)"
           : current->fd == -1       ? " (self)"
                                     : "");
    current = current->next;
  }
}

ObjectList *list_find(ObjectList *list, Object object) {
  // Static variables to remember state between calls
  static ObjectList *last_position = NULL;
  static char *last_object = NULL;
  ObjectList *current;

  // If non-NULL list is provided, start a new search
  if (list != NULL) {
    // Free previous saved object if any
    free(last_object);
    last_object = strdup(object);
    last_position = NULL;
    current = list;
  } else {
    // Continue previous search
    if (last_position == NULL || last_position->next == NULL) {
      return NULL;
    }
    current = last_position->next;
    object = last_object; // Use the stored object
  }

  // Search for the object
  while (current != NULL) {
    if (strcmp(current->self, object) == 0) {
      last_position = current;
      return current;
    }
    current = current->next;
  }

  return NULL;
}

// Fixed: compare by fd instead of ip/tcp strings
ObjectList *list_find_connection(ObjectList *list, int fd) {
  ObjectList *current = list;
  while (current != NULL) {
    if (current->fd == fd) {
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
