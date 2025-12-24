// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli.h"
#include "format.h"
#include "logger.h"
#include "util_buffer.h"
#include "util_str.h"
#include "value.h"
#if defined(SCREW_UP_USE_VERSION_HEADER)
#include "version.h"
#else
#ifndef SCREW_UP_VERSION
#define SCREW_UP_VERSION "0.0.0"
#endif
#ifndef SCREW_UP_GIT_HASH
#define SCREW_UP_GIT_HASH "unknown"
#endif
#endif

typedef struct {
  const char *input_path;
  const char *output_path;
  const char *bracket_option;
  int disable_wds;
} FormatArgs;

typedef struct {
  const char *directory;
  int disable_wds;
} DumpArgs;

static void show_version(void) {
  printf(
    "screw-up (native) [%s-%s]\n"
    "Easy package metadata inserter CLI\n"
    "Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)\n"
    "Repository: https://github.com/kekyo/screw-up-native\n"
    "License: Under MIT\n"
    "\n",
    SCREW_UP_VERSION,
    SCREW_UP_GIT_HASH
  );
}

static void show_help(void) {
  show_version();
  printf(
    "Usage: screw-up <format|dump> [options]\n"
    "\n"
    "Options:\n"
    "  -h, --help, -?                Show help\n"
  );
}

static void show_format_help(void) {
  show_version();
  printf(
    "Usage: screw-up format [options] [output]\n"
    "\n"
    "Options:\n"
    "  -i, --input <path>            Input template file (default: stdin)\n"
    "  -b, --bracket <open,close>    Placeholder brackets (default: {,})\n"
    "  --no-wds                      Do not check working directory status\n"
    "  -h, --help, -?                Show help\n"
  );
}

static void show_dump_help(void) {
  show_version();
  printf(
    "Usage: screw-up dump [options] [directory]\n"
    "\n"
    "Dump computed metadata as JSON\n"
    "\n"
    "Arguments:\n"
    "  directory                     Directory to dump metadata from (default: current directory)\n"
    "\n"
    "Options:\n"
    "  --no-wds                      Do not check working directory status\n"
    "  -h, --help, -?                Show help\n"
  );
}

static int is_help_option(const char *arg) {
  if (!arg) {
    return 0;
  }
  return strcmp(arg, "-h") == 0 ||
         strcmp(arg, "--help") == 0 ||
         strcmp(arg, "-?") == 0;
}

static int has_help_option_from(int argc, char **argv, int start_index) {
  int index;
  for (index = start_index; index < argc; index++) {
    if (is_help_option(argv[index])) {
      return 1;
    }
  }
  return 0;
}

static char *read_stdin_all(void) {
  StringBuffer buffer;
  char chunk[4096];
  size_t bytes_read;

  string_buffer_init(&buffer);

  while ((bytes_read = fread(chunk, 1, sizeof(chunk), stdin)) > 0) {
    if (!string_buffer_append(&buffer, chunk, bytes_read)) {
      string_buffer_free(&buffer);
      return NULL;
    }
  }

  return string_buffer_detach(&buffer);
}

static char *read_file_all(const char *path) {
  FILE *file;
  char *buffer;
  long length;
  size_t read_length;

  file = fopen(path, "rb");
  if (!file) {
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }

  length = ftell(file);
  if (length < 0) {
    fclose(file);
    return NULL;
  }

  rewind(file);

  buffer = (char *)calloc((size_t)length + 1, 1);
  if (!buffer) {
    fclose(file);
    return NULL;
  }

  read_length = fread(buffer, 1, (size_t)length, file);
  buffer[read_length] = '\0';
  fclose(file);
  return buffer;
}

static int write_file_all(const char *path, const char *content) {
  FILE *file;
  size_t length;
  size_t written;

  file = fopen(path, "wb");
  if (!file) {
    return 0;
  }

  length = strlen(content);
  written = fwrite(content, 1, length, file);
  fclose(file);
  return written == length;
}

static int parse_bracket_option(const char *option,
                                char **open_out,
                                char **close_out) {
  const char *comma;
  size_t open_length;

  if (!option) {
    return 0;
  }

  comma = strchr(option, ',');
  if (!comma) {
    return 0;
  }

  open_length = (size_t)(comma - option);
  if (open_length == 0 || comma[1] == '\0') {
    return 0;
  }

  *open_out = su_strndup(option, open_length);
  *close_out = su_strdup(comma + 1);
  if (!*open_out || !*close_out) {
    free(*open_out);
    free(*close_out);
    *open_out = NULL;
    *close_out = NULL;
    return 0;
  }

  return 1;
}

static int parse_format_args(int argc,
                             char **argv,
                             int start_index,
                             FormatArgs *out_args) {
  int index = start_index;

  memset(out_args, 0, sizeof(*out_args));

  while (index < argc) {
    const char *arg = argv[index];
    if (strcmp(arg, "-i") == 0 || strcmp(arg, "--input") == 0) {
      if (index + 1 >= argc) {
        return 0;
      }
      out_args->input_path = argv[++index];
    } else if (strcmp(arg, "-b") == 0 || strcmp(arg, "--bracket") == 0) {
      if (index + 1 >= argc) {
        return 0;
      }
      out_args->bracket_option = argv[++index];
    } else if (strcmp(arg, "--no-wds") == 0) {
      out_args->disable_wds = 1;
    } else if (arg[0] == '-') {
      return 0;
    } else {
      if (out_args->output_path) {
        return 0;
      }
      out_args->output_path = arg;
    }
    index++;
  }

  return 1;
}

static int parse_dump_args(int argc,
                           char **argv,
                           int start_index,
                           DumpArgs *out_args) {
  int index = start_index;

  memset(out_args, 0, sizeof(*out_args));

  while (index < argc) {
    const char *arg = argv[index];
    if (strcmp(arg, "--no-wds") == 0) {
      out_args->disable_wds = 1;
    } else if (arg[0] == '-') {
      return 0;
    } else {
      if (out_args->directory) {
        return 0;
      }
      out_args->directory = arg;
    }
    index++;
  }

  return 1;
}

static int run_format(Logger *logger,
                      const FormatArgs *args) {
  Value *metadata;
  char *input;
  char *output;
  char *open_bracket = NULL;
  char *close_bracket = NULL;
  const char *open = "{";
  const char *close = "}";
  int result = 1;

  metadata = NULL;
  input = NULL;
  output = NULL;

  if (args && args->bracket_option) {
    if (!parse_bracket_option(args->bracket_option, &open_bracket, &close_bracket)) {
      fprintf(stderr,
              "format: Invalid bracket option, expected \"open,close\" pattern.\n");
      goto cleanup;
    }
    open = open_bracket;
    close = close_bracket;
  }

  if (args && args->input_path) {
    input = read_file_all(args->input_path);
  } else {
    input = read_stdin_all();
  }
  if (!input) {
    goto cleanup;
  }

  metadata = format_build_metadata(".", args ? !args->disable_wds : 1, logger);
  if (!metadata) {
    goto cleanup;
  }

  output = format_placeholders(input, metadata, open, close);
  if (!output) {
    goto cleanup;
  }

  if (args && args->output_path) {
    if (!write_file_all(args->output_path, output)) {
      goto cleanup;
    }
  }

  fwrite(output, 1, strlen(output), stdout);
  result = 0;

cleanup:
  free(open_bracket);
  free(close_bracket);
  free(output);
  value_free(metadata);
  free(input);
  return result;
}

static int run_dump(Logger *logger,
                    const DumpArgs *args) {
  Value *metadata;
  char *json;
  const char *target_dir;
  int result = 1;

  metadata = NULL;
  json = NULL;
  target_dir = ".";

  if (args && args->directory) {
    target_dir = args->directory;
  }

  metadata = format_build_metadata(target_dir, args ? !args->disable_wds : 1, logger);
  if (!metadata) {
    fprintf(stderr, "dump: Failed to dump metadata\n");
    goto cleanup;
  }

  json = value_to_json(metadata, 2);
  if (!json) {
    fprintf(stderr, "dump: Failed to dump metadata\n");
    goto cleanup;
  }

  fwrite(json, 1, strlen(json), stdout);
  fputc('\n', stdout);
  result = 0;

cleanup:
  free(json);
  value_free(metadata);
  return result;
}

int screw_up_main(int argc, char **argv) {
  Logger logger = logger_null();
  FormatArgs args;
  DumpArgs dump_args;
  int start_index = 1;

  if (argc >= 2 && is_help_option(argv[1])) {
    show_help();
    return 0;
  }

  if (argc >= 2 && strcmp(argv[1], "dump") == 0) {
    start_index = 2;
    if (has_help_option_from(argc, argv, start_index)) {
      show_dump_help();
      return 0;
    }
    if (!parse_dump_args(argc, argv, start_index, &dump_args)) {
      fprintf(stderr, "dump: Invalid arguments\n");
      return 1;
    }
    return run_dump(&logger, &dump_args);
  }

  if (argc >= 2 && strcmp(argv[1], "format") == 0) {
    start_index = 2;
    if (has_help_option_from(argc, argv, start_index)) {
      show_format_help();
      return 0;
    }
    if (!parse_format_args(argc, argv, start_index, &args)) {
      fprintf(stderr, "format: Invalid arguments\n");
      return 1;
    }
    return run_format(&logger, &args);
  }

  if (argc == 1) {
    start_index = 1;
    if (!parse_format_args(argc, argv, start_index, &args)) {
      fprintf(stderr, "format: Invalid arguments\n");
      return 1;
    }
    return run_format(&logger, &args);
  }

  fprintf(stderr, "Usage: screw-up format|dump\n");
  return 1;
}
