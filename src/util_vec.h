// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#ifndef SU_UTIL_VEC_H
#define SU_UTIL_VEC_H

#include <stddef.h>

#include "util_str.h"

typedef struct {
  char **items;
  size_t length;
  size_t capacity;
} StringArray;

void string_array_init(StringArray *array);
int string_array_push(StringArray *array, const char *value);
int string_array_push_owned(StringArray *array, char *value);
void string_array_sort(StringArray *array);
void string_array_free(StringArray *array);

int string_array_equals(const StringArray *array,
                        const char *const *expected,
                        size_t expected_length);

typedef struct {
  void **items;
  size_t length;
  size_t capacity;
} PtrArray;

void ptr_array_init(PtrArray *array);
int ptr_array_push(PtrArray *array, void *value);
void ptr_array_free(PtrArray *array, void (*free_fn)(void *));

#endif
