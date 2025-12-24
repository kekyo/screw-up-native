// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#ifndef SCREW_UP_VALUE_H
#define SCREW_UP_VALUE_H

#include <stddef.h>

typedef struct Value Value;

typedef enum {
  VALUE_NULL,
  VALUE_STRING,
  VALUE_OBJECT,
  VALUE_ARRAY
} ValueType;

Value *value_create_null(void);
Value *value_create_string(const char *text);
Value *value_create_object(void);
Value *value_create_array(void);

int value_object_set(Value *object, const char *key, Value *value);
Value *value_object_get(const Value *object, const char *key);
int value_array_push(Value *array, Value *value);

Value *value_get_path(const Value *root, const char *path);
char *value_to_string(const Value *value);
char *value_to_json(const Value *value, int indent_step);

void value_free(Value *value);

#endif
