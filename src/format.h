// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#ifndef SCREW_UP_FORMAT_H
#define SCREW_UP_FORMAT_H

#include "logger.h"
#include "value.h"

Value *format_build_metadata(const char *repository_path,
                             int check_working_directory_status,
                             Logger *logger);

char *format_placeholders(const char *input,
                          const Value *metadata,
                          const char *open_bracket,
                          const char *close_bracket);

#endif
