// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#include "util_map.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util_str.h"

struct MapEntry {
  char *key;
  void *value;
  MapEntry *next;
};

#if SIZE_MAX == UINT32_MAX
#define SU_FNV_OFFSET_BASIS ((size_t)2166136261u)
#define SU_FNV_PRIME ((size_t)16777619u)
#elif SIZE_MAX == UINT64_MAX
#define SU_FNV_OFFSET_BASIS ((size_t)14695981039346656037ULL)
#define SU_FNV_PRIME ((size_t)1099511628211ULL)
#else
#error "Unsupported size_t size for FNV-1a."
#endif

static size_t hash_string(const char *value) {
  size_t hash = SU_FNV_OFFSET_BASIS;
  const unsigned char *ptr = (const unsigned char *)value;

  while (*ptr) {
    hash ^= (size_t)(*ptr++);
    hash *= SU_FNV_PRIME;
  }

  return hash;
}

void map_init(Map *map, size_t bucket_count) {
  map->bucket_count = bucket_count ? bucket_count : 128;
  map->buckets = (MapEntry **)calloc(map->bucket_count, sizeof(MapEntry *));
  map->size = 0;
}

static void map_entry_free(MapEntry *entry, void (*free_value)(void *)) {
  MapEntry *next;

  while (entry) {
    next = entry->next;
    free(entry->key);
    if (free_value) {
      free_value(entry->value);
    }
    free(entry);
    entry = next;
  }
}

void map_free(Map *map, void (*free_value)(void *)) {
  size_t index;

  if (!map || !map->buckets) {
    return;
  }

  for (index = 0; index < map->bucket_count; index++) {
    map_entry_free(map->buckets[index], free_value);
  }

  free(map->buckets);
  map->buckets = NULL;
  map->bucket_count = 0;
  map->size = 0;
}

void *map_get(Map *map, const char *key) {
  size_t bucket;
  MapEntry *entry;

  if (!map || !key || !map->buckets) {
    return NULL;
  }

  bucket = hash_string(key) % map->bucket_count;
  entry = map->buckets[bucket];

  while (entry) {
    if (strcmp(entry->key, key) == 0) {
      return entry->value;
    }
    entry = entry->next;
  }

  return NULL;
}

int map_has(Map *map, const char *key) {
  return map_get(map, key) != NULL;
}

void *map_put(Map *map, const char *key, void *value) {
  size_t bucket;
  MapEntry *entry;

  if (!map || !key || !map->buckets) {
    return NULL;
  }

  bucket = hash_string(key) % map->bucket_count;
  entry = map->buckets[bucket];

  while (entry) {
    if (strcmp(entry->key, key) == 0) {
      void *old_value = entry->value;
      entry->value = value;
      return old_value;
    }
    entry = entry->next;
  }

  entry = (MapEntry *)calloc(1, sizeof(MapEntry));
  if (!entry) {
    return NULL;
  }

  entry->key = su_strdup(key);
  if (!entry->key) {
    free(entry);
    return NULL;
  }

  entry->value = value;
  entry->next = map->buckets[bucket];
  map->buckets[bucket] = entry;
  map->size++;

  return NULL;
}

void map_foreach(Map *map,
                 void (*fn)(const char *key, void *value, void *ctx),
                 void *ctx) {
  size_t index;
  MapEntry *entry;

  if (!map || !fn || !map->buckets) {
    return;
  }

  for (index = 0; index < map->bucket_count; index++) {
    entry = map->buckets[index];
    while (entry) {
      fn(entry->key, entry->value, ctx);
      entry = entry->next;
    }
  }
}
