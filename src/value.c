// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#include "value.h"

#include <stdlib.h>
#include <string.h>

#include "util_buffer.h"
#include "util_str.h"

typedef struct {
  char *key;
  Value *value;
} ValueEntry;

typedef struct {
  ValueEntry *entries;
  size_t length;
  size_t capacity;
} ValueObject;

typedef struct {
  Value **items;
  size_t length;
  size_t capacity;
} ValueArray;

struct Value {
  ValueType type;
  union {
    char *string;
    ValueObject object;
    ValueArray array;
  } data;
};

static int ensure_object_capacity(ValueObject *object, size_t needed) {
  size_t new_capacity;
  ValueEntry *next;

  if (object->capacity >= needed) {
    return 1;
  }

  new_capacity = object->capacity == 0 ? 4 : object->capacity * 2;
  while (new_capacity < needed) {
    new_capacity *= 2;
  }

  next = (ValueEntry *)realloc(object->entries, new_capacity * sizeof(ValueEntry));
  if (!next) {
    return 0;
  }

  object->entries = next;
  object->capacity = new_capacity;
  return 1;
}

static int ensure_array_capacity(ValueArray *array, size_t needed) {
  size_t new_capacity;
  Value **next;

  if (array->capacity >= needed) {
    return 1;
  }

  new_capacity = array->capacity == 0 ? 4 : array->capacity * 2;
  while (new_capacity < needed) {
    new_capacity *= 2;
  }

  next = (Value **)realloc(array->items, new_capacity * sizeof(Value *));
  if (!next) {
    return 0;
  }

  array->items = next;
  array->capacity = new_capacity;
  return 1;
}

Value *value_create_null(void) {
  Value *value = (Value *)calloc(1, sizeof(Value));
  if (!value) {
    return NULL;
  }
  value->type = VALUE_NULL;
  return value;
}

Value *value_create_string(const char *text) {
  Value *value = (Value *)calloc(1, sizeof(Value));
  if (!value) {
    return NULL;
  }
  value->type = VALUE_STRING;
  value->data.string = su_strdup(text ? text : "");
  if (!value->data.string) {
    free(value);
    return NULL;
  }
  return value;
}

Value *value_create_object(void) {
  Value *value = (Value *)calloc(1, sizeof(Value));
  if (!value) {
    return NULL;
  }
  value->type = VALUE_OBJECT;
  value->data.object.entries = NULL;
  value->data.object.length = 0;
  value->data.object.capacity = 0;
  return value;
}

Value *value_create_array(void) {
  Value *value = (Value *)calloc(1, sizeof(Value));
  if (!value) {
    return NULL;
  }
  value->type = VALUE_ARRAY;
  value->data.array.items = NULL;
  value->data.array.length = 0;
  value->data.array.capacity = 0;
  return value;
}

int value_object_set(Value *object, const char *key, Value *value) {
  size_t index;
  ValueObject *obj;
  char *key_copy;

  if (!object || object->type != VALUE_OBJECT || !key || !value) {
    return 0;
  }

  obj = &object->data.object;

  for (index = 0; index < obj->length; index++) {
    if (strcmp(obj->entries[index].key, key) == 0) {
      value_free(obj->entries[index].value);
      obj->entries[index].value = value;
      return 1;
    }
  }

  if (!ensure_object_capacity(obj, obj->length + 1)) {
    return 0;
  }

  key_copy = su_strdup(key);
  if (!key_copy) {
    return 0;
  }

  obj->entries[obj->length].key = key_copy;
  obj->entries[obj->length].value = value;
  obj->length++;
  return 1;
}

Value *value_object_get(const Value *object, const char *key) {
  size_t index;
  const ValueObject *obj;

  if (!object || object->type != VALUE_OBJECT || !key) {
    return NULL;
  }

  obj = &object->data.object;
  for (index = 0; index < obj->length; index++) {
    if (strcmp(obj->entries[index].key, key) == 0) {
      return obj->entries[index].value;
    }
  }

  return NULL;
}

int value_array_push(Value *array, Value *value) {
  ValueArray *arr;

  if (!array || array->type != VALUE_ARRAY || !value) {
    return 0;
  }

  arr = &array->data.array;
  if (!ensure_array_capacity(arr, arr->length + 1)) {
    return 0;
  }

  arr->items[arr->length++] = value;
  return 1;
}

Value *value_get_path(const Value *root, const char *path) {
  const Value *current = root;
  const char *segment_start;
  const char *cursor;
  char *segment;

  if (!root || !path || path[0] == '\0') {
    return NULL;
  }

  segment_start = path;
  cursor = path;
  while (1) {
    if (*cursor == '.' || *cursor == '\0') {
      size_t length = (size_t)(cursor - segment_start);
      if (length == 0) {
        return NULL;
      }

      segment = su_strndup(segment_start, length);
      if (!segment) {
        return NULL;
      }

      if (!current || current->type != VALUE_OBJECT) {
        free(segment);
        return NULL;
      }

      current = value_object_get(current, segment);
      free(segment);

      if (*cursor == '\0' || !current) {
        break;
      }

      segment_start = cursor + 1;
    }

    if (*cursor == '\0') {
      break;
    }
    cursor++;
  }

  return (Value *)current;
}

static int json_append_escaped(StringBuffer *buffer, const char *text) {
  const unsigned char *ptr = (const unsigned char *)text;

  if (!string_buffer_append_char(buffer, '"')) {
    return 0;
  }

  while (*ptr) {
    unsigned char ch = *ptr++;
    switch (ch) {
      case '"':
        if (!string_buffer_append(buffer, "\\\"", 2)) {
          return 0;
        }
        break;
      case '\\':
        if (!string_buffer_append(buffer, "\\\\", 2)) {
          return 0;
        }
        break;
      case '\b':
        if (!string_buffer_append(buffer, "\\b", 2)) {
          return 0;
        }
        break;
      case '\f':
        if (!string_buffer_append(buffer, "\\f", 2)) {
          return 0;
        }
        break;
      case '\n':
        if (!string_buffer_append(buffer, "\\n", 2)) {
          return 0;
        }
        break;
      case '\r':
        if (!string_buffer_append(buffer, "\\r", 2)) {
          return 0;
        }
        break;
      case '\t':
        if (!string_buffer_append(buffer, "\\t", 2)) {
          return 0;
        }
        break;
      default:
        if (!string_buffer_append_char(buffer, (char)ch)) {
          return 0;
        }
        break;
    }
  }

  return string_buffer_append_char(buffer, '"');
}

static int value_append_json(StringBuffer *buffer, const Value *value) {
  size_t index;

  if (!value) {
    return string_buffer_append(buffer, "null", 4);
  }

  switch (value->type) {
    case VALUE_NULL:
      return string_buffer_append(buffer, "null", 4);
    case VALUE_STRING:
      return json_append_escaped(buffer, value->data.string ? value->data.string : "");
    case VALUE_ARRAY: {
      if (!string_buffer_append_char(buffer, '[')) {
        return 0;
      }
      for (index = 0; index < value->data.array.length; index++) {
        if (index > 0) {
          if (!string_buffer_append_char(buffer, ',')) {
            return 0;
          }
        }
        if (!value_append_json(buffer, value->data.array.items[index])) {
          return 0;
        }
      }
      return string_buffer_append_char(buffer, ']');
    }
    case VALUE_OBJECT: {
      if (!string_buffer_append_char(buffer, '{')) {
        return 0;
      }
      for (index = 0; index < value->data.object.length; index++) {
        if (index > 0) {
          if (!string_buffer_append_char(buffer, ',')) {
            return 0;
          }
        }
        if (!json_append_escaped(buffer, value->data.object.entries[index].key)) {
          return 0;
        }
        if (!string_buffer_append_char(buffer, ':')) {
          return 0;
        }
        if (!value_append_json(buffer, value->data.object.entries[index].value)) {
          return 0;
        }
      }
      return string_buffer_append_char(buffer, '}');
    }
    default:
      return 0;
  }
}

static int append_indent(StringBuffer *buffer, int indent) {
  int index;

  for (index = 0; index < indent; index++) {
    if (!string_buffer_append_char(buffer, ' ')) {
      return 0;
    }
  }

  return 1;
}

static int value_append_json_pretty(StringBuffer *buffer,
                                    const Value *value,
                                    int indent,
                                    int indent_step) {
  size_t index;

  if (!value) {
    return string_buffer_append(buffer, "null", 4);
  }

  switch (value->type) {
    case VALUE_NULL:
      return string_buffer_append(buffer, "null", 4);
    case VALUE_STRING:
      return json_append_escaped(buffer, value->data.string ? value->data.string : "");
    case VALUE_ARRAY: {
      if (!string_buffer_append_char(buffer, '[')) {
        return 0;
      }
      if (value->data.array.length == 0) {
        return string_buffer_append_char(buffer, ']');
      }
      if (!string_buffer_append_char(buffer, '\n')) {
        return 0;
      }
      for (index = 0; index < value->data.array.length; index++) {
        if (!append_indent(buffer, indent + indent_step)) {
          return 0;
        }
        if (!value_append_json_pretty(buffer,
                                      value->data.array.items[index],
                                      indent + indent_step,
                                      indent_step)) {
          return 0;
        }
        if (index + 1 < value->data.array.length) {
          if (!string_buffer_append_char(buffer, ',')) {
            return 0;
          }
        }
        if (!string_buffer_append_char(buffer, '\n')) {
          return 0;
        }
      }
      if (!append_indent(buffer, indent)) {
        return 0;
      }
      return string_buffer_append_char(buffer, ']');
    }
    case VALUE_OBJECT: {
      if (!string_buffer_append_char(buffer, '{')) {
        return 0;
      }
      if (value->data.object.length == 0) {
        return string_buffer_append_char(buffer, '}');
      }
      if (!string_buffer_append_char(buffer, '\n')) {
        return 0;
      }
      for (index = 0; index < value->data.object.length; index++) {
        if (!append_indent(buffer, indent + indent_step)) {
          return 0;
        }
        if (!json_append_escaped(buffer, value->data.object.entries[index].key)) {
          return 0;
        }
        if (!string_buffer_append(buffer, ": ", 2)) {
          return 0;
        }
        if (!value_append_json_pretty(buffer,
                                      value->data.object.entries[index].value,
                                      indent + indent_step,
                                      indent_step)) {
          return 0;
        }
        if (index + 1 < value->data.object.length) {
          if (!string_buffer_append_char(buffer, ',')) {
            return 0;
          }
        }
        if (!string_buffer_append_char(buffer, '\n')) {
          return 0;
        }
      }
      if (!append_indent(buffer, indent)) {
        return 0;
      }
      return string_buffer_append_char(buffer, '}');
    }
    default:
      return 0;
  }
}

char *value_to_string(const Value *value) {
  StringBuffer buffer;

  if (!value) {
    return NULL;
  }

  if (value->type == VALUE_STRING) {
    return su_strdup(value->data.string ? value->data.string : "");
  }

  if (value->type == VALUE_NULL) {
    return su_strdup("null");
  }

  string_buffer_init(&buffer);
  if (!value_append_json(&buffer, value)) {
    string_buffer_free(&buffer);
    return NULL;
  }

  return string_buffer_detach(&buffer);
}

char *value_to_json(const Value *value, int indent_step) {
  StringBuffer buffer;

  string_buffer_init(&buffer);

  if (indent_step > 0) {
    if (!value_append_json_pretty(&buffer, value, 0, indent_step)) {
      string_buffer_free(&buffer);
      return NULL;
    }
  } else {
    if (!value_append_json(&buffer, value)) {
      string_buffer_free(&buffer);
      return NULL;
    }
  }

  return string_buffer_detach(&buffer);
}

void value_free(Value *value) {
  size_t index;

  if (!value) {
    return;
  }

  switch (value->type) {
    case VALUE_STRING:
      free(value->data.string);
      break;
    case VALUE_OBJECT:
      for (index = 0; index < value->data.object.length; index++) {
        free(value->data.object.entries[index].key);
        value_free(value->data.object.entries[index].value);
      }
      free(value->data.object.entries);
      break;
    case VALUE_ARRAY:
      for (index = 0; index < value->data.array.length; index++) {
        value_free(value->data.array.items[index]);
      }
      free(value->data.array.items);
      break;
    case VALUE_NULL:
    default:
      break;
  }

  free(value);
}
