// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#define _POSIX_C_SOURCE 200809L

#include "analyzer.h"

#include <git2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "git_operations.h"
#include "util_map.h"
#include "util_str.h"
#include "util_vec.h"

typedef struct {
  char *hash;
  char *short_hash;
  time_t author_time;
  char *message;
  StringArray parents;
} CommitInfo;

typedef struct {
  char *hash;
  StringArray parents;
} ScheduledCommit;

struct GitMetadataFetcher {
  char *target_dir;
  int check_working_directory_status;
  Logger *logger;
  Metadata *cached;
};

static void commit_info_free(CommitInfo *commit) {
  if (!commit) {
    return;
  }
  free(commit->hash);
  free(commit->short_hash);
  free(commit->message);
  string_array_free(&commit->parents);
  free(commit);
}

static ScheduledCommit *scheduled_commit_create(const char *hash,
                                                const StringArray *parents) {
  ScheduledCommit *scheduled;
  size_t index;

  scheduled = (ScheduledCommit *)calloc(1, sizeof(ScheduledCommit));
  if (!scheduled) {
    return NULL;
  }

  scheduled->hash = su_strdup(hash);
  if (!scheduled->hash) {
    free(scheduled);
    return NULL;
  }

  string_array_init(&scheduled->parents);
  if (parents) {
    for (index = 0; index < parents->length; index++) {
      if (!string_array_push(&scheduled->parents, parents->items[index])) {
        string_array_free(&scheduled->parents);
        free(scheduled->hash);
        free(scheduled);
        return NULL;
      }
    }
  }

  return scheduled;
}

static void scheduled_commit_free(ScheduledCommit *scheduled) {
  if (!scheduled) {
    return;
  }
  free(scheduled->hash);
  string_array_free(&scheduled->parents);
  free(scheduled);
}

static int parse_version_component(const char *value,
                                   size_t length,
                                   int *out_value) {
  size_t index;
  int number = 0;

  if (length == 0) {
    return 0;
  }

  for (index = 0; index < length; index++) {
    char ch = value[index];
    if (ch < '0' || ch > '9') {
      return 0;
    }
    number = number * 10 + (ch - '0');
    if (number > 65535) {
      return 0;
    }
  }

  *out_value = number;
  return 1;
}

static Version *version_clone(const Version *version) {
  Version *copy;

  if (!version) {
    return NULL;
  }

  copy = (Version *)calloc(1, sizeof(Version));
  if (!copy) {
    return NULL;
  }

  *copy = *version;
  copy->original = su_strdup(version->original ? version->original : "");
  if (!copy->original) {
    free(copy);
    return NULL;
  }

  return copy;
}

static void version_free_ptr(void *ptr) {
  Version *version = (Version *)ptr;
  if (!version) {
    return;
  }
  free(version->original);
  free(version);
}

static Version *parse_version(const char *tag_name) {
  const char *cursor;
  const char *segment_start;
  size_t segment_length;
  int values[4] = {0, 0, 0, 0};
  size_t count = 0;
  Version *version;

  if (!tag_name) {
    return NULL;
  }

  cursor = tag_name;
  if (*cursor == 'v' || *cursor == 'V') {
    cursor++;
  }

  segment_start = cursor;
  while (*cursor != '\0') {
    if (*cursor == '.') {
      segment_length = (size_t)(cursor - segment_start);
      if (count >= 4 ||
          !parse_version_component(segment_start, segment_length, &values[count])) {
        return NULL;
      }
      count++;
      segment_start = cursor + 1;
    }
    cursor++;
  }

  segment_length = (size_t)(cursor - segment_start);
  if (count >= 4 ||
      !parse_version_component(segment_start, segment_length, &values[count])) {
    return NULL;
  }
  count++;

  version = (Version *)calloc(1, sizeof(Version));
  if (!version) {
    return NULL;
  }

  version->major = values[0];
  version->has_minor = count >= 2;
  version->has_build = count >= 3;
  version->has_revision = count >= 4;
  if (version->has_minor) {
    version->minor = values[1];
  }
  if (version->has_build) {
    version->build = values[2];
  }
  if (version->has_revision) {
    version->revision = values[3];
  }
  version->original = su_strdup(tag_name);
  if (!version->original) {
    free(version);
    return NULL;
  }

  return version;
}

static int is_valid_version(const Version *version) {
  if (!version) {
    return 0;
  }
  return version->major >= 0 && (!version->has_minor || version->minor >= 0);
}

static int compare_versions(const Version *left, const Version *right) {
  int left_minor;
  int right_minor;
  int left_build;
  int right_build;
  int left_revision;
  int right_revision;

  if (left->major != right->major) {
    return right->major - left->major;
  }

  left_minor = left->has_minor ? left->minor : 0;
  right_minor = right->has_minor ? right->minor : 0;
  if (left_minor != right_minor) {
    return right_minor - left_minor;
  }

  left_build = left->has_build ? left->build : 0;
  right_build = right->has_build ? right->build : 0;
  if (left_build != right_build) {
    return right_build - left_build;
  }

  left_revision = left->has_revision ? left->revision : 0;
  right_revision = right->has_revision ? right->revision : 0;
  if (left_revision != right_revision) {
    return right_revision - left_revision;
  }

  return 0;
}

static Version *increment_last_version_component(const Version *version) {
  Version *next;
  char buffer[32];

  if (!version) {
    return NULL;
  }

  next = version_clone(version);
  if (!next) {
    return NULL;
  }

  if (next->has_revision) {
    next->revision += 1;
    return next;
  }
  if (next->has_build) {
    next->build += 1;
    return next;
  }
  if (next->has_minor) {
    next->minor += 1;
    return next;
  }

  next->major += 1;
  snprintf(buffer, sizeof(buffer), "%d", next->major);
  free(next->original);
  next->original = su_strdup(buffer);

  return next;
}

static char *format_version(const Version *version) {
  char buffer[64];
  size_t length = 0;

  if (!version) {
    return NULL;
  }

  length += snprintf(buffer + length,
                     sizeof(buffer) - length,
                     "%d",
                     version->major);

  if (version->has_minor) {
    length += snprintf(buffer + length,
                       sizeof(buffer) - length,
                       ".%d",
                       version->minor);

    if (version->has_build) {
      length += snprintf(buffer + length,
                         sizeof(buffer) - length,
                         ".%d",
                         version->build);

      if (version->has_revision) {
        length += snprintf(buffer + length,
                           sizeof(buffer) - length,
                           ".%d",
                           version->revision);
      }
    }
  }

  return su_strdup(buffer);
}

static CommitInfo *get_commit(git_repository *repo, const char *hash) {
  git_commit *commit = NULL;
  git_oid oid;
  const git_signature *author;
  CommitInfo *info;
  size_t parent_count;
  size_t index;

  if (!repo || !hash) {
    return NULL;
  }

  if (git_oid_fromstr(&oid, hash) < 0) {
    return NULL;
  }

  if (git_commit_lookup(&commit, repo, &oid) < 0) {
    return NULL;
  }

  info = (CommitInfo *)calloc(1, sizeof(CommitInfo));
  if (!info) {
    git_commit_free(commit);
    return NULL;
  }

  info->hash = su_strdup(hash);
  if (!info->hash) {
    commit_info_free(info);
    git_commit_free(commit);
    return NULL;
  }

  info->short_hash = su_strndup(hash, 7);
  author = git_commit_author(commit);
  info->author_time = author ? author->when.time : 0;

  info->message = su_str_trim_copy(git_commit_message(commit));
  if (!info->message) {
    info->message = su_strdup("");
  }

  string_array_init(&info->parents);
  parent_count = git_commit_parentcount(commit);
  for (index = 0; index < parent_count; index++) {
    const git_oid *parent_oid = git_commit_parent_id(commit, index);
    char parent_hash[GIT_OID_HEXSZ + 1];
    if (!parent_oid) {
      continue;
    }
    git_oid_tostr(parent_hash, sizeof(parent_hash), parent_oid);
    string_array_push(&info->parents, parent_hash);
  }

  git_commit_free(commit);
  return info;
}

static CommitInfo *get_current_commit(git_repository *repo) {
  git_reference *head = NULL;
  const git_oid *oid;
  char hash[GIT_OID_HEXSZ + 1];
  CommitInfo *commit;

  if (!repo) {
    return NULL;
  }

  if (git_repository_head(&head, repo) < 0) {
    return NULL;
  }

  oid = git_reference_target(head);
  if (!oid) {
    git_reference_free(head);
    return NULL;
  }

  git_oid_tostr(hash, sizeof(hash), oid);
  commit = get_commit(repo, hash);

  git_reference_free(head);
  return commit;
}

static TagInfoList *get_related_tags_from_map(Map *commit_to_tags,
                                              const char *commit_hash) {
  return (TagInfoList *)map_get(commit_to_tags, commit_hash);
}

static int get_related_branches(git_repository *repo,
                                const char *commit_hash,
                                StringArray *out_branches) {
  git_branch_iterator *iter = NULL;
  git_reference *ref = NULL;
  git_branch_t type;
  int error;

  if (!repo || !commit_hash || !out_branches) {
    return -1;
  }

  error = git_branch_iterator_new(&iter, repo, GIT_BRANCH_LOCAL);
  if (error < 0) {
    return error;
  }

  while (git_branch_next(&ref, &type, iter) == 0) {
    const git_oid *oid = git_reference_target(ref);
    if (oid) {
      char hash[GIT_OID_HEXSZ + 1];
      git_oid_tostr(hash, sizeof(hash), oid);
      if (strcmp(hash, commit_hash) == 0) {
        const char *name = git_reference_shorthand(ref);
        if (name) {
          string_array_push(out_branches, name);
        }
      }
    }
    git_reference_free(ref);
    ref = NULL;
  }

  git_branch_iterator_free(iter);
  return 0;
}

static int collect_modified_files(git_repository *repo, StringArray *out_files) {
  git_status_options options = GIT_STATUS_OPTIONS_INIT;
  git_status_list *status_list = NULL;
  size_t count;
  size_t index;
  int error;

  if (!repo) {
    return 0;
  }

  options.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
  options.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
                  GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;

  error = git_status_list_new(&status_list, repo, &options);
  if (error < 0) {
    return 0;
  }

  count = git_status_list_entrycount(status_list);
  for (index = 0; index < count; index++) {
    const git_status_entry *entry = git_status_byindex(status_list, index);
    const char *path = NULL;

    if (!entry || entry->status == GIT_STATUS_CURRENT) {
      continue;
    }

    if (entry->head_to_index && entry->head_to_index->new_file.path) {
      path = entry->head_to_index->new_file.path;
    } else if (entry->index_to_workdir && entry->index_to_workdir->new_file.path) {
      path = entry->index_to_workdir->new_file.path;
    }

    if (path) {
      string_array_push(out_files, path);
    }
  }

  git_status_list_free(status_list);
  return out_files->length > 0;
}

static char *format_commit_date(time_t timestamp) {
  struct tm local_tm;
  char date_buffer[32];
  char offset_buffer[8];
  char offset_colon[8];
  size_t offset_len;
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

  result = (char *)malloc(strlen(date_buffer) + strlen(offset_colon) + 1);
  if (!result) {
    return NULL;
  }

  snprintf(result,
           strlen(date_buffer) + strlen(offset_colon) + 1,
           "%s%s",
           date_buffer,
           offset_colon);

  return result;
}

static Version *lookup_version_label_recursive(git_repository *repo,
                                               CommitInfo *commit,
                                               Map *reached_commits,
                                               Map *commit_to_tags) {
  PtrArray scheduled_stack;
  Version *version;
  CommitInfo *current_commit;
  int owns_current = 0;

  ptr_array_init(&scheduled_stack);

  version = (Version *)calloc(1, sizeof(Version));
  if (!version) {
    return NULL;
  }
  version->major = 0;
  version->minor = 0;
  version->build = 1;
  version->has_minor = 1;
  version->has_build = 1;
  version->original = su_strdup("0.0.1");

  current_commit = commit;

  while (current_commit) {
    Version *cached;
    TagInfoList *related_tags;
    TagInfo *best_tag = NULL;
    size_t index;

    cached = (Version *)map_get(reached_commits, current_commit->hash);
    if (cached) {
      Version *clone = version_clone(cached);
      if (clone) {
        free(version->original);
        free(version);
        version = clone;
      }
      break;
    }

    related_tags = get_related_tags_from_map(commit_to_tags, current_commit->hash);
    if (related_tags) {
      for (index = 0; index < related_tags->items.length; index++) {
        TagInfo *tag = (TagInfo *)related_tags->items.items[index];
        if (!tag || !tag->version || !is_valid_version(tag->version)) {
          continue;
        }
        if (!tag->version->has_minor) {
          continue;
        }
        if (!best_tag || compare_versions(tag->version, best_tag->version) < 0) {
          best_tag = tag;
        }
      }
    }

    if (best_tag) {
      Version *clone = version_clone(best_tag->version);
      if (clone) {
        free(version->original);
        free(version);
        version = clone;
        map_put(reached_commits, current_commit->hash, version_clone(version));
      }
      break;
    }

    if (current_commit->parents.length == 0) {
      map_put(reached_commits, current_commit->hash, version_clone(version));
      break;
    }

    {
      ScheduledCommit *scheduled = scheduled_commit_create(current_commit->hash,
                                                          &current_commit->parents);
      if (scheduled) {
        ptr_array_push(&scheduled_stack, scheduled);
      }
    }

    {
      CommitInfo *next_commit = get_commit(repo, current_commit->parents.items[0]);
      if (owns_current) {
        commit_info_free(current_commit);
      }
      current_commit = next_commit;
      owns_current = 1;
    }
  }

  if (owns_current && current_commit) {
    commit_info_free(current_commit);
  }

  while (scheduled_stack.length > 0) {
    ScheduledCommit *scheduled = (ScheduledCommit *)scheduled_stack.items[scheduled_stack.length - 1];
    scheduled_stack.length--;

    if (scheduled->parents.length >= 2) {
      size_t index;
      for (index = 1; index < scheduled->parents.length; index++) {
        CommitInfo *alternate_commit = get_commit(repo, scheduled->parents.items[index]);
        Version *alternate_version;
        if (!alternate_commit) {
          continue;
        }
        alternate_version = lookup_version_label_recursive(repo,
                                                           alternate_commit,
                                                           reached_commits,
                                                           commit_to_tags);
        if (alternate_version && compare_versions(alternate_version, version) < 0) {
          free(version->original);
          free(version);
          version = alternate_version;
        } else if (alternate_version) {
          free(alternate_version->original);
          free(alternate_version);
        }
        commit_info_free(alternate_commit);
      }
    }

    {
      Version *incremented = increment_last_version_component(version);
      if (incremented) {
        free(version->original);
        free(version);
        version = incremented;
        map_put(reached_commits, scheduled->hash, version_clone(version));
      }
    }

    scheduled_commit_free(scheduled);
  }

  free(scheduled_stack.items);
  return version;
}

Metadata *get_git_metadata(const char *repository_path,
                           int check_working_directory_status,
                           Logger *logger) {
  Metadata *metadata;
  git_repository *repo = NULL;
  CommitInfo *current_commit = NULL;
  Map commit_to_tags;
  Map reached_commits;
  Version *version = NULL;
  StringArray modified_files;

  metadata = (Metadata *)calloc(1, sizeof(Metadata));
  if (!metadata) {
    return NULL;
  }

  string_array_init(&metadata->git.tags);
  string_array_init(&metadata->git.branches);

  if (git_libgit2_init() < 0) {
    return metadata;
  }

#if defined(LIBGIT2_VER_MAJOR) && defined(LIBGIT2_VER_MINOR)
#if (LIBGIT2_VER_MAJOR > 1) || (LIBGIT2_VER_MAJOR == 1 && LIBGIT2_VER_MINOR >= 5)
  git_libgit2_opts(GIT_OPT_SET_OWNER_VALIDATION, 0);
#endif
#endif

  if (git_repository_open_ext(&repo, repository_path, 0, NULL) < 0) {
    git_libgit2_shutdown();
    return metadata;
  }

  current_commit = get_current_commit(repo);
  if (!current_commit) {
    git_repository_free(repo);
    git_libgit2_shutdown();
    return metadata;
  }

  if (build_complete_tag_cache(repo, parse_version, logger, &commit_to_tags) < 0) {
    commit_info_free(current_commit);
    git_repository_free(repo);
    git_libgit2_shutdown();
    return metadata;
  }

  map_init(&reached_commits, 128);

  version = lookup_version_label_recursive(repo,
                                           current_commit,
                                           &reached_commits,
                                           &commit_to_tags);

  metadata->has_git = 1;

  if (version) {
    if (check_working_directory_status) {
      string_array_init(&modified_files);
      if (collect_modified_files(repo, &modified_files)) {
        Version *incremented = increment_last_version_component(version);
        if (incremented) {
          free(version->original);
          free(version);
          version = incremented;
        }
      }
      string_array_free(&modified_files);
    }

    metadata->git.version = format_version(version);
    if (metadata->git.version) {
      metadata->version = su_strdup(metadata->git.version);
    }
  }

  metadata->git.commit.hash = su_strdup(current_commit->hash);
  metadata->git.commit.short_hash = su_strdup(current_commit->short_hash);
  metadata->git.commit.message = su_strdup(current_commit->message);
  metadata->git.commit.date = format_commit_date(current_commit->author_time);
  metadata->git.has_commit = 1;

  {
    TagInfoList *related_tags = get_related_tags_from_map(&commit_to_tags,
                                                          current_commit->hash);
    if (related_tags) {
      size_t index;
      for (index = 0; index < related_tags->items.length; index++) {
        TagInfo *tag = (TagInfo *)related_tags->items.items[index];
        if (tag && tag->name) {
          string_array_push(&metadata->git.tags, tag->name);
        }
      }
    }
  }

  get_related_branches(repo, current_commit->hash, &metadata->git.branches);

  if (version) {
    free(version->original);
    free(version);
  }

  map_free(&reached_commits, version_free_ptr);
  map_free(&commit_to_tags, tag_info_list_free_ptr);
  commit_info_free(current_commit);

  git_repository_free(repo);
  git_libgit2_shutdown();

  return metadata;
}

GitMetadataFetcher *get_fetch_git_metadata(const char *target_dir,
                                           int check_working_directory_status,
                                           Logger *logger) {
  GitMetadataFetcher *fetcher;

  fetcher = (GitMetadataFetcher *)calloc(1, sizeof(GitMetadataFetcher));
  if (!fetcher) {
    return NULL;
  }

  fetcher->target_dir = su_strdup(target_dir);
  fetcher->check_working_directory_status = check_working_directory_status;
  fetcher->logger = logger;
  fetcher->cached = NULL;

  return fetcher;
}

Metadata *git_metadata_fetch(GitMetadataFetcher *fetcher) {
  if (!fetcher) {
    return NULL;
  }

  if (!fetcher->cached) {
    fetcher->cached = get_git_metadata(fetcher->target_dir,
                                       fetcher->check_working_directory_status,
                                       fetcher->logger);
  }

  return fetcher->cached;
}

void git_metadata_fetcher_free(GitMetadataFetcher *fetcher) {
  if (!fetcher) {
    return;
  }
  free(fetcher->target_dir);
  metadata_free(fetcher->cached);
  free(fetcher);
}

void metadata_free(Metadata *metadata) {
  if (!metadata) {
    return;
  }

  free(metadata->version);
  free(metadata->git.version);
  string_array_free(&metadata->git.tags);
  string_array_free(&metadata->git.branches);
  if (metadata->git.has_commit) {
    free(metadata->git.commit.hash);
    free(metadata->git.commit.short_hash);
    free(metadata->git.commit.date);
    free(metadata->git.commit.message);
  }
  free(metadata);
}
