// screw-up-native - Native CLI that Git versioning metadata dumper/formatter.
// Copyright (c) Kouji Matsui (@kekyo@mi.kekyo.net)
// Under MIT.
// https://github.com/kekyo/screw-up-native/

#ifndef SU_UTIL_MAP_H
#define SU_UTIL_MAP_H

#include <stddef.h>

typedef struct MapEntry MapEntry;

typedef struct {
  MapEntry **buckets;
  size_t bucket_count;
  size_t size;
} Map;

void map_init(Map *map, size_t bucket_count);
void map_free(Map *map, void (*free_value)(void *));
void *map_get(Map *map, const char *key);
int map_has(Map *map, const char *key);
void *map_put(Map *map, const char *key, void *value);

void map_foreach(Map *map,
                 void (*fn)(const char *key, void *value, void *ctx),
                 void *ctx);

#endif
