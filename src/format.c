// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#define _POSIX_C_SOURCE 200809L

#include "format.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "analyzer.h"
#include "util_buffer.h"
#include "util_str.h"

static char *format_time_iso(time_t timestamp, int append_z) {
  struct tm local_tm;
  char date_buffer[32];
  char offset_buffer[8];
  char offset_colon[8];
  size_t offset_len;
  size_t total_len;
  char *result;

  if (!localtime_r(&timestamp, &local_tm)) {
    return NULL;
  }

  if (strftime(date_buffer, sizeof(date_buffer), "%Y-%m-%dT%H:%M:%S", &local_tm) == 0) {
    return NULL;
  }

  if (strftime(offset_buffer, sizeof(offset_buffer), "%z", &local_tm) == 0) {
    return NULL;
  }

  offset_len = strlen(offset_buffer);
  if (offset_len == 5) {
    offset_colon[0] = offset_buffer[0];
    offset_colon[1] = offset_buffer[1];
    offset_colon[2] = offset_buffer[2];
    offset_colon[3] = ':';
    offset_colon[4] = offset_buffer[3];
    offset_colon[5] = offset_buffer[4];
    offset_colon[6] = '\0';
  } else {
    snprintf(offset_colon, sizeof(offset_colon), "%s", offset_buffer);
  }

  total_len = strlen(date_buffer) + strlen(offset_colon) + (append_z ? 1 : 0);
  result = (char *)malloc(total_len + 1);
  if (!result) {
    return NULL;
  }

  if (append_z) {
    snprintf(result, total_len + 1, "%s%sZ", date_buffer, offset_colon);
  } else {
    snprintf(result, total_len + 1, "%s%s", date_buffer, offset_colon);
  }

  return result;
}

static int value_object_set_string(Value *object, const char *key, const char *value) {
  Value *node = value_create_string(value);
  if (!node) {
    return 0;
  }
  if (!value_object_set(object, key, node)) {
    value_free(node);
    return 0;
  }
  return 1;
}

Value *format_build_metadata(const char *repository_path,
                             int check_working_directory_status,
                             Logger *logger) {
  Metadata *git_metadata;
  Value *root;
  Value *git_object;
  Value *commit_object;
  Value *tags_array;
  Value *branches_array;
  char *build_date;
  const char *version;
  size_t index;

  root = value_create_object();
  if (!root) {
    return NULL;
  }

  git_object = value_create_object();
  if (!git_object) {
    value_free(root);
    return NULL;
  }

  value_object_set(root, "git", git_object);

  git_metadata = get_git_metadata(repository_path,
                                  check_working_directory_status,
                                  logger);

  version = NULL;
  if (git_metadata && git_metadata->has_git && git_metadata->git.version) {
    version = git_metadata->git.version;
  }
  if (!version) {
    version = "0.0.1";
  }

  value_object_set_string(root, "version", version);

  build_date = format_time_iso(time(NULL), 0);
  if (build_date) {
    value_object_set_string(root, "buildDate", build_date);
    free(build_date);
  }

  value_object_set_string(git_object, "version", version);

  commit_object = value_create_object();
  if (!commit_object) {
    metadata_free(git_metadata);
    return root;
  }

  if (git_metadata && git_metadata->has_git && git_metadata->git.has_commit) {
    value_object_set_string(commit_object, "hash", git_metadata->git.commit.hash);
    value_object_set_string(commit_object, "shortHash", git_metadata->git.commit.short_hash);
    if (git_metadata->git.commit.date) {
      value_object_set_string(commit_object, "date", git_metadata->git.commit.date);
    }
    if (git_metadata->git.commit.message) {
      value_object_set_string(commit_object, "message", git_metadata->git.commit.message);
    }
  } else {
    value_object_set_string(commit_object, "hash", "unknown");
    value_object_set_string(commit_object, "shortHash", "unknown");
  }

  value_object_set(git_object, "commit", commit_object);

  tags_array = value_create_array();
  branches_array = value_create_array();
  if (!tags_array || !branches_array) {
    value_free(tags_array);
    value_free(branches_array);
    metadata_free(git_metadata);
    return root;
  }

  if (git_metadata && git_metadata->has_git) {
    for (index = 0; index < git_metadata->git.tags.length; index++) {
      value_array_push(tags_array, value_create_string(git_metadata->git.tags.items[index]));
    }
    for (index = 0; index < git_metadata->git.branches.length; index++) {
      value_array_push(branches_array, value_create_string(git_metadata->git.branches.items[index]));
    }
  }

  value_object_set(git_object, "tags", tags_array);
  value_object_set(git_object, "branches", branches_array);

  metadata_free(git_metadata);
  return root;
}

char *format_placeholders(const char *input,
                          const Value *metadata,
                          const char *open_bracket,
                          const char *close_bracket) {
  StringBuffer buffer;
  size_t length;
  size_t open_length;
  size_t close_length;
  size_t index = 0;

  if (!input || !open_bracket || !close_bracket) {
    return NULL;
  }

  string_buffer_init(&buffer);
  length = strlen(input);
  open_length = strlen(open_bracket);
  close_length = strlen(close_bracket);

  while (index < length) {
    if (open_length > 0 &&
        index + open_length <= length &&
        strncmp(input + index, open_bracket, open_length) == 0) {
      size_t cursor = index + open_length;
      size_t close_index = 0;
      int found = 0;

      while (cursor < length) {
        if (input[cursor] == '\n' || input[cursor] == '\r') {
          break;
        }
        if (close_length > 0 &&
            cursor + close_length <= length &&
            strncmp(input + cursor, close_bracket, close_length) == 0) {
          found = 1;
          close_index = cursor;
          break;
        }
        cursor++;
      }

      if (found) {
        size_t key_len = close_index - index - open_length;
        char *key = su_strndup(input + index + open_length, key_len);
        char *replacement = NULL;

        if (key) {
          su_str_trim_inplace(key);
          if (key[0] != '\0') {
            Value *node = value_get_path(metadata, key);
            replacement = value_to_string(node);
          }
        }

        if (replacement) {
          string_buffer_append_str(&buffer, replacement);
          free(replacement);
        } else {
          string_buffer_append(&buffer,
                               input + index,
                               close_index - index + close_length);
        }

        free(key);
        index = close_index + close_length;
        continue;
      }
    }

    string_buffer_append_char(&buffer, input[index]);
    index++;
  }

  return string_buffer_detach(&buffer);
}
