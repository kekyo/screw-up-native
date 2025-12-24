// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#ifndef SCREW_UP_GIT_OPERATIONS_H
#define SCREW_UP_GIT_OPERATIONS_H

#include <git2.h>

#include "analyzer.h"
#include "logger.h"
#include "util_map.h"
#include "util_vec.h"

typedef Version *(*ParseVersionFunc)(const char *tag_name);

typedef struct {
  PtrArray items;
} TagInfoList;

int build_complete_tag_cache(git_repository *repo,
                             ParseVersionFunc parse_version,
                             Logger *logger,
                             Map *out_commit_to_tags);

void tag_info_list_free_ptr(void *ptr);

#endif
