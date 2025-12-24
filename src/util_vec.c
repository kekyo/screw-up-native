// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#include "util_vec.h"

#include <stdlib.h>
#include <string.h>

static int ensure_capacity(void **items,
                           size_t *capacity,
                           size_t item_size,
                           size_t needed) {
  void *new_items;
  size_t new_capacity;

  if (*capacity >= needed) {
    return 1;
  }

  new_capacity = (*capacity == 0) ? 4 : (*capacity * 2);
  while (new_capacity < needed) {
    new_capacity *= 2;
  }

  new_items = realloc(*items, new_capacity * item_size);
  if (!new_items) {
    return 0;
  }

  *items = new_items;
  *capacity = new_capacity;
  return 1;
}

void string_array_init(StringArray *array) {
  array->items = NULL;
  array->length = 0;
  array->capacity = 0;
}

int string_array_push(StringArray *array, const char *value) {
  char *copy;

  copy = su_strdup(value);
  if (!copy) {
    return 0;
  }

  return string_array_push_owned(array, copy);
}

int string_array_push_owned(StringArray *array, char *value) {
  if (!ensure_capacity((void **)&array->items,
                       &array->capacity,
                       sizeof(char *),
                       array->length + 1)) {
    return 0;
  }

  array->items[array->length++] = value;
  return 1;
}

static int string_compare(const void *left, const void *right) {
  const char *const *l = (const char *const *)left;
  const char *const *r = (const char *const *)right;
  return strcmp(*l, *r);
}

void string_array_sort(StringArray *array) {
  if (array->length == 0) {
    return;
  }
  qsort(array->items, array->length, sizeof(char *), string_compare);
}

void string_array_free(StringArray *array) {
  size_t index;

  if (!array) {
    return;
  }

  for (index = 0; index < array->length; index++) {
    free(array->items[index]);
  }

  free(array->items);
  array->items = NULL;
  array->length = 0;
  array->capacity = 0;
}

int string_array_equals(const StringArray *array,
                        const char *const *expected,
                        size_t expected_length) {
  size_t index;

  if (array->length != expected_length) {
    return 0;
  }

  for (index = 0; index < expected_length; index++) {
    if (strcmp(array->items[index], expected[index]) != 0) {
      return 0;
    }
  }

  return 1;
}

void ptr_array_init(PtrArray *array) {
  array->items = NULL;
  array->length = 0;
  array->capacity = 0;
}

int ptr_array_push(PtrArray *array, void *value) {
  if (!ensure_capacity((void **)&array->items,
                       &array->capacity,
                       sizeof(void *),
                       array->length + 1)) {
    return 0;
  }

  array->items[array->length++] = value;
  return 1;
}

void ptr_array_free(PtrArray *array, void (*free_fn)(void *)) {
  size_t index;

  if (!array) {
    return;
  }

  if (free_fn) {
    for (index = 0; index < array->length; index++) {
      free_fn(array->items[index]);
    }
  }

  free(array->items);
  array->items = NULL;
  array->length = 0;
  array->capacity = 0;
}
