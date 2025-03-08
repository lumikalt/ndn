#include "list.h"
#include "types.h"
#include "util.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ObjectList *list_create() {
  ObjectList *list = malloc(sizeof(ObjectList));
  if (!list) {
    perror(RED "ERR" RESET "\tmalloc");
    exit(1);
  }
  list->self = NULL;
  list->ip = NULL;
  list->tcp = NULL;
  list->next = NULL;
  return list;
}

void list_add(ObjectList *list, Object object, char *ip, char *tcp) {
  if (list_find(list, object) != NULL) {
    return;
  }

  ObjectList *current = list;
  while (current->next != NULL) {
    current = current->next;
  }

  ObjectList *new_node = malloc(sizeof(ObjectList));
  if (!new_node) {
    perror(RED "ERR" RESET "\tmalloc");
    exit(1);
  }
  new_node->self = object;
  new_node->ip = ip ? strdup(ip) : NULL;
  new_node->tcp = tcp ? strdup(tcp) : NULL;
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
      free(current->ip);
      free(current->tcp);
      free(current);
      return;
    }
    prev = current;
    current = current->next;
  }
}

void list_remove_connection(ObjectList *list, char *ip, char *tcp) {
  ObjectList *current = list;
  ObjectList *prev = NULL;

  while (current != NULL) {
    if (strcmp(current->ip, ip) == 0 && strcmp(current->tcp, tcp) == 0) {
      if (prev == NULL) {
        list = current->next;
      } else {
        prev->next = current->next;
      }
      free(current->ip);
      free(current->tcp);
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
    free(temp->ip);
    free(temp->tcp);
    free(temp);
  }
}

void list_print(ObjectList *list) {
  ObjectList *current = list;
  while (current != NULL) {
    printf(RESET "\t> `%s`\n", current->self);
    current = current->next;
  }
}

void list_print_interests(ObjectList *list) {
  ObjectList *current = list;
  while (current != NULL) {
    printf(RESET "\t> `%s` (%s:%s)\n", current->self, current->ip, current->tcp);
    current = current->next;
  }
}

ObjectList *list_find(ObjectList *list, Object object) {
  ObjectList *current = list;
  while (current != NULL) {
    if (current->self == object) {
      return current;
    }
    current = current->next;
  }
  return NULL;
}

ObjectList *list_find_connection(ObjectList *list, char *ip, char *tcp) {
  ObjectList *current = list;
  while (current != NULL) {
    if (strcmp(current->ip, ip) == 0 && strcmp(current->tcp, tcp) == 0) {
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
