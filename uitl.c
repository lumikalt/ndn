#include "util.h"

usize str_char_count(const char *s, char c) {
  usize count = 0;
  for (usize i = 0; s[i]; s[i] == c ? count++, i++ : i++)
    ;
  return count;
}
