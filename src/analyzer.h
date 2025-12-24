// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#ifndef SCREW_UP_ANALYZER_H
#define SCREW_UP_ANALYZER_H

#include "logger.h"
#include "util_vec.h"

typedef struct {
  int major;
  int minor;
  int build;
  int revision;
  int has_minor;
  int has_build;
  int has_revision;
  char *original;
} Version;

typedef struct {
  char *name;
  char *hash;
  Version *version;
} TagInfo;

typedef struct {
  char *hash;
  char *short_hash;
  char *date;
  char *message;
} GitCommitMetadata;

typedef struct {
  char *version;
  StringArray tags;
  StringArray branches;
  GitCommitMetadata commit;
  int has_commit;
} GitMetadata;

typedef struct {
  GitMetadata git;
  char *version;
  int has_git;
} Metadata;

typedef struct GitMetadataFetcher GitMetadataFetcher;

Metadata *get_git_metadata(const char *repository_path,
                           int check_working_directory_status,
                           Logger *logger);

GitMetadataFetcher *get_fetch_git_metadata(const char *target_dir,
                                           int check_working_directory_status,
                                           Logger *logger);
Metadata *git_metadata_fetch(GitMetadataFetcher *fetcher);
void git_metadata_fetcher_free(GitMetadataFetcher *fetcher);

void metadata_free(Metadata *metadata);

#endif
