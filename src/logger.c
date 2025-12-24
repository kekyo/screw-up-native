// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util_str.h"

typedef struct {
  char *prefix;
} ConsoleLoggerContext;

static void console_log(FILE *stream, const char *prefix, const char *msg) {
  if (prefix && prefix[0] != '\0') {
    fprintf(stream, "[%s]: %s\n", prefix, msg);
  } else {
    fprintf(stream, "%s\n", msg);
  }
}

static void console_debug(const char *msg, void *ctx) {
  (void)msg;
  (void)ctx;
}

static void console_info(const char *msg, void *ctx) {
  ConsoleLoggerContext *context = (ConsoleLoggerContext *)ctx;
  console_log(stdout, context ? context->prefix : NULL, msg);
}

static void console_warn(const char *msg, void *ctx) {
  ConsoleLoggerContext *context = (ConsoleLoggerContext *)ctx;
  console_log(stderr, context ? context->prefix : NULL, msg);
}

static void console_error(const char *msg, void *ctx) {
  ConsoleLoggerContext *context = (ConsoleLoggerContext *)ctx;
  console_log(stderr, context ? context->prefix : NULL, msg);
}

Logger logger_console_create(const char *prefix) {
  Logger logger;
  ConsoleLoggerContext *context = NULL;

  if (prefix) {
    context = (ConsoleLoggerContext *)calloc(1, sizeof(ConsoleLoggerContext));
    if (context) {
      context->prefix = su_strdup(prefix);
    }
  }

  logger.debug = console_debug;
  logger.info = console_info;
  logger.warn = console_warn;
  logger.error = console_error;
  logger.ctx = context;

  return logger;
}

Logger logger_null(void) {
  Logger logger;
  logger.debug = console_debug;
  logger.info = console_debug;
  logger.warn = console_debug;
  logger.error = console_debug;
  logger.ctx = NULL;
  return logger;
}

void logger_free(Logger *logger) {
  ConsoleLoggerContext *context;

  if (!logger) {
    return;
  }

  context = (ConsoleLoggerContext *)logger->ctx;
  if (context) {
    free(context->prefix);
    free(context);
  }
  logger->ctx = NULL;
}

static void logger_log(Logger *logger,
                       void (*fn)(const char *, void *),
                       const char *msg) {
  if (!logger || !fn || !msg) {
    return;
  }
  fn(msg, logger->ctx);
}

void logger_debug(Logger *logger, const char *msg) {
  logger_log(logger, logger ? logger->debug : NULL, msg);
}

void logger_info(Logger *logger, const char *msg) {
  logger_log(logger, logger ? logger->info : NULL, msg);
}

void logger_warn(Logger *logger, const char *msg) {
  logger_log(logger, logger ? logger->warn : NULL, msg);
}

void logger_error(Logger *logger, const char *msg) {
  logger_log(logger, logger ? logger->error : NULL, msg);
}

static void logger_vprintf(Logger *logger,
                           void (*fn)(const char *, void *),
                           const char *format,
                           va_list args) {
  char buffer[1024];
  int written;

  if (!logger || !fn || !format) {
    return;
  }

  written = vsnprintf(buffer, sizeof(buffer), format, args);
  if (written < 0) {
    return;
  }

  fn(buffer, logger->ctx);
}

void logger_debugf(Logger *logger, const char *format, ...) {
  va_list args;
  va_start(args, format);
  logger_vprintf(logger, logger ? logger->debug : NULL, format, args);
  va_end(args);
}

void logger_infof(Logger *logger, const char *format, ...) {
  va_list args;
  va_start(args, format);
  logger_vprintf(logger, logger ? logger->info : NULL, format, args);
  va_end(args);
}

void logger_warnf(Logger *logger, const char *format, ...) {
  va_list args;
  va_start(args, format);
  logger_vprintf(logger, logger ? logger->warn : NULL, format, args);
  va_end(args);
}

void logger_errorf(Logger *logger, const char *format, ...) {
  va_list args;
  va_start(args, format);
  logger_vprintf(logger, logger ? logger->error : NULL, format, args);
  va_end(args);
}
