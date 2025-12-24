// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#include "util_str.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char *su_strdup(const char *value) {
  size_t length;
  char *copy;

  if (!value) {
    return NULL;
  }

  length = strlen(value);
  copy = (char *)malloc(length + 1);
  if (!copy) {
    return NULL;
  }

  memcpy(copy, value, length);
  copy[length] = '\0';
  return copy;
}

char *su_strndup(const char *value, size_t length) {
  char *copy;

  if (!value) {
    return NULL;
  }

  copy = (char *)malloc(length + 1);
  if (!copy) {
    return NULL;
  }

  memcpy(copy, value, length);
  copy[length] = '\0';
  return copy;
}

void su_str_trim_inplace(char *value) {
  char *start;
  char *end;
  size_t length;

  if (!value) {
    return;
  }

  start = value;
  while (*start && isspace((unsigned char)*start)) {
    start++;
  }

  end = start + strlen(start);
  while (end > start && isspace((unsigned char)*(end - 1))) {
    end--;
  }

  length = (size_t)(end - start);
  memmove(value, start, length);
  value[length] = '\0';
}

char *su_str_trim_copy(const char *value) {
  char *copy;

  if (!value) {
    return NULL;
  }

  copy = su_strdup(value);
  if (!copy) {
    return NULL;
  }

  su_str_trim_inplace(copy);
  return copy;
}
