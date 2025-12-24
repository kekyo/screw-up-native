// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#include "util_buffer.h"

#include <stdlib.h>
#include <string.h>

static int ensure_capacity(StringBuffer *buffer, size_t needed) {
  size_t new_capacity;
  char *next;

  if (buffer->capacity >= needed) {
    return 1;
  }

  new_capacity = buffer->capacity == 0 ? 64 : buffer->capacity * 2;
  while (new_capacity < needed) {
    new_capacity *= 2;
  }

  next = (char *)realloc(buffer->data, new_capacity);
  if (!next) {
    return 0;
  }

  buffer->data = next;
  buffer->capacity = new_capacity;
  return 1;
}

void string_buffer_init(StringBuffer *buffer) {
  buffer->data = NULL;
  buffer->length = 0;
  buffer->capacity = 0;
}

int string_buffer_append(StringBuffer *buffer, const char *data, size_t length) {
  if (!buffer || !data) {
    return 0;
  }

  if (!ensure_capacity(buffer, buffer->length + length + 1)) {
    return 0;
  }

  memcpy(buffer->data + buffer->length, data, length);
  buffer->length += length;
  buffer->data[buffer->length] = '\0';
  return 1;
}

int string_buffer_append_str(StringBuffer *buffer, const char *text) {
  if (!text) {
    return 0;
  }
  return string_buffer_append(buffer, text, strlen(text));
}

int string_buffer_append_char(StringBuffer *buffer, char ch) {
  if (!ensure_capacity(buffer, buffer->length + 2)) {
    return 0;
  }
  buffer->data[buffer->length++] = ch;
  buffer->data[buffer->length] = '\0';
  return 1;
}

char *string_buffer_detach(StringBuffer *buffer) {
  char *result;

  if (!buffer) {
    return NULL;
  }

  result = buffer->data;
  buffer->data = NULL;
  buffer->length = 0;
  buffer->capacity = 0;
  return result;
}

void string_buffer_free(StringBuffer *buffer) {
  if (!buffer) {
    return;
  }
  free(buffer->data);
  buffer->data = NULL;
  buffer->length = 0;
  buffer->capacity = 0;
}
