// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#ifndef SU_UTIL_BUFFER_H
#define SU_UTIL_BUFFER_H

#include <stddef.h>

typedef struct {
  char *data;
  size_t length;
  size_t capacity;
} StringBuffer;

void string_buffer_init(StringBuffer *buffer);
int string_buffer_append(StringBuffer *buffer, const char *data, size_t length);
int string_buffer_append_str(StringBuffer *buffer, const char *text);
int string_buffer_append_char(StringBuffer *buffer, char ch);
char *string_buffer_detach(StringBuffer *buffer);
void string_buffer_free(StringBuffer *buffer);

#endif
