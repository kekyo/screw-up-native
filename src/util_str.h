// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#ifndef SU_UTIL_STR_H
#define SU_UTIL_STR_H

#include <stddef.h>

char *su_strdup(const char *value);
char *su_strndup(const char *value, size_t length);
void su_str_trim_inplace(char *value);
char *su_str_trim_copy(const char *value);

#endif
