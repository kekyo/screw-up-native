// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#ifndef SU_LOGGER_H
#define SU_LOGGER_H

#include <stdarg.h>

typedef struct Logger Logger;

struct Logger {
  void (*debug)(const char *msg, void *ctx);
  void (*info)(const char *msg, void *ctx);
  void (*warn)(const char *msg, void *ctx);
  void (*error)(const char *msg, void *ctx);
  void *ctx;
};

Logger logger_console_create(const char *prefix);
Logger logger_null(void);
void logger_free(Logger *logger);

void logger_debug(Logger *logger, const char *msg);
void logger_info(Logger *logger, const char *msg);
void logger_warn(Logger *logger, const char *msg);
void logger_error(Logger *logger, const char *msg);

void logger_debugf(Logger *logger, const char *format, ...);
void logger_infof(Logger *logger, const char *format, ...);
void logger_warnf(Logger *logger, const char *format, ...);
void logger_errorf(Logger *logger, const char *format, ...);

#endif
