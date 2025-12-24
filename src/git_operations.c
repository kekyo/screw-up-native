// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#include "git_operations.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util_vec.h"
#include "util_str.h"

static TagInfo *tag_info_create(const char *name,
                                const char *hash,
                                Version *version) {
  TagInfo *tag = (TagInfo *)calloc(1, sizeof(TagInfo));
  if (!tag) {
    return NULL;
  }

  tag->name = su_strdup(name);
  tag->hash = su_strdup(hash);
  tag->version = version;

  if (!tag->name || !tag->hash) {
    free(tag->name);
    free(tag->hash);
    free(tag);
    return NULL;
  }

  return tag;
}

static void version_free(Version *version) {
  if (!version) {
    return;
  }
  free(version->original);
  free(version);
}

static void tag_info_free(void *ptr) {
  TagInfo *tag = (TagInfo *)ptr;
  if (!tag) {
    return;
  }
  free(tag->name);
  free(tag->hash);
  version_free(tag->version);
  free(tag);
}

static TagInfoList *tag_info_list_create(void) {
  TagInfoList *list = (TagInfoList *)calloc(1, sizeof(TagInfoList));
  if (!list) {
    return NULL;
  }
  ptr_array_init(&list->items);
  return list;
}

static void tag_info_list_free(TagInfoList *list) {
  if (!list) {
    return;
  }
  ptr_array_free(&list->items, tag_info_free);
  free(list);
}

void tag_info_list_free_ptr(void *ptr) {
  tag_info_list_free((TagInfoList *)ptr);
}

static int tag_info_list_push(TagInfoList *list, TagInfo *tag) {
  return ptr_array_push(&list->items, tag);
}

static int tag_info_compare(const void *left, const void *right) {
  const TagInfo *const *l = (const TagInfo *const *)left;
  const TagInfo *const *r = (const TagInfo *const *)right;
  return strcmp((*l)->name, (*r)->name);
}

static void tag_info_list_sort(TagInfoList *list) {
  if (!list || list->items.length == 0) {
    return;
  }
  qsort(list->items.items,
        list->items.length,
        sizeof(void *),
        tag_info_compare);
}

static void sort_tags_cb(const char *key, void *value, void *ctx) {
  TagInfoList *list = (TagInfoList *)value;
  (void)key;
  (void)ctx;
  tag_info_list_sort(list);
}

static int resolve_tag_commit(git_repository *repo,
                              const char *tag_name,
                              char *commit_oid,
                              size_t commit_oid_len) {
  char ref_name[1024];
  git_reference *ref = NULL;
  git_object *peeled = NULL;
  int error;

  if (snprintf(ref_name, sizeof(ref_name), "refs/tags/%s", tag_name) < 0) {
    return -1;
  }

  error = git_reference_lookup(&ref, repo, ref_name);
  if (error < 0) {
    return error;
  }

  error = git_reference_peel(&peeled, ref, GIT_OBJECT_COMMIT);
  if (error < 0) {
    git_reference_free(ref);
    return error;
  }

  git_oid_tostr(commit_oid, commit_oid_len, git_object_id(peeled));

  git_object_free(peeled);
  git_reference_free(ref);
  return 0;
}

int build_complete_tag_cache(git_repository *repo,
                             ParseVersionFunc parse_version,
                             Logger *logger,
                             Map *out_commit_to_tags) {
  git_strarray tags = {0};
  int error;
  size_t index;

  if (!repo || !parse_version || !out_commit_to_tags) {
    return -1;
  }

  error = git_tag_list(&tags, repo);
  if (error < 0) {
    logger_warnf(logger, "[git-ops] git_tag_list failed: %d", error);
    return error;
  }

  map_init(out_commit_to_tags, tags.count * 2 + 1);

  for (index = 0; index < tags.count; index++) {
    const char *tag_name = tags.strings[index];
    char commit_oid[GIT_OID_HEXSZ + 1];
    Version *version = NULL;
    TagInfo *tag;
    TagInfoList *list;

    if (resolve_tag_commit(repo, tag_name, commit_oid, sizeof(commit_oid)) < 0) {
      continue;
    }

    version = parse_version(tag_name);
    tag = tag_info_create(tag_name, commit_oid, version);
    if (!tag) {
      version_free(version);
      continue;
    }

    list = (TagInfoList *)map_get(out_commit_to_tags, commit_oid);
    if (!list) {
      list = tag_info_list_create();
      if (!list) {
        tag_info_free(tag);
        continue;
      }
      map_put(out_commit_to_tags, commit_oid, list);
    }

    tag_info_list_push(list, tag);
  }

  map_foreach(out_commit_to_tags, sort_tags_cb, NULL);
  logger_debugf(logger,
                "[git-ops] Built cache with %zu unique commits",
                out_commit_to_tags->size);

  git_strarray_dispose(&tags);
  return 0;
}
