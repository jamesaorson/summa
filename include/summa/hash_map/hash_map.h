#ifndef SUMMA_HASH_MAP_H
#define SUMMA_HASH_MAP_H

#include <stdbool.h>
#include <stddef.h>

#pragma region Hash maps

typedef struct summa_hash_map_t* SummaHashMap;

#define SUMMA_HASH_SET_IMPLEMENTATION
#include <summa/hash_set/hash_set.h>

SUMMA_ARRAY_GENERATE_TYPE_DEF(SummaHashMapKeys, hash_map_keys, SummaHashCode)
SUMMA_ARRAY_GENERATE_TYPE_DEF(SummaHashMapValues, hash_map_values, void*)

SummaHashMap summa_hash_map_make_empty(size_t key_size);
bool         summa_hash_map_contains(SummaHashMap map, void* key);
bool         summa_hash_map_get(SummaHashMap map, void* key, void** out_value);
bool         summa_hash_map_put(SummaHashMap map, void* key, void* value);
bool         summa_hash_map_remove(SummaHashMap map, void* key);
void         summa_hash_map_clear(SummaHashMap map);
void         summa_hash_map_copy(SummaHashMap dest, SummaHashMap src);
void         summa_hash_map_free(SummaHashMap map);

#pragma endregion Hash maps

#endif

#ifdef SUMMA_HASH_MAP_IMPLEMENTATION
#ifndef SUMMA_HASH_MAP_IMPLEMENTATION_ONCE
#define SUMMA_HASH_MAP_IMPLEMENTATION_ONCE

#include <stdlib.h>

#pragma region Hash maps

SUMMA_ARRAY_GENERATE_TYPE_IMPL(SummaHashMapKeys, hash_map_keys, SummaHashCode)
SUMMA_ARRAY_GENERATE_TYPE_IMPL(SummaHashMapValues, hash_map_values, void*)

struct summa_hash_map_t {
    SummaHashMapKeys   keys;
    SummaHashMapValues values;
    size_t             key_size;
};

SummaHashMap summa_hash_map_make_empty(size_t key_size) {
    SummaHashMap map = (SummaHashMap)malloc(sizeof(struct summa_hash_map_t));
    map->key_size    = key_size;
    map->keys        = summa_hash_map_keys_make_empty();
    map->values      = summa_hash_map_values_make_empty();
    return map;
}

bool summa_hash_map_contains(SummaHashMap map, void* key) {
    SummaHashCode code = summa_hash(key, map->key_size);
    return summa_hash_map_keys_contains(map->keys, &code);
}

bool summa_hash_map_get(SummaHashMap map, void* key, void** out_value) {
    SummaHashCode code = summa_hash(key, map->key_size);
    size_t        index;
    if (!summa_hash_map_keys_index_of(map->keys, &code, &index)) {
        return false;
    }
    if (out_value) {
        *out_value = map->values->value[index];
    }
    return true;
}

bool summa_hash_map_put(SummaHashMap map, void* key, void* value) {
    SummaHashCode code = summa_hash(key, map->key_size);
    size_t        index;
    if (summa_hash_map_keys_index_of(map->keys, &code, &index)) {
        summa_hash_map_values_set_at(map->values, index, &value);
        return false;
    }
    summa_hash_map_keys_push(map->keys, &code);
    summa_hash_map_values_push(map->values, &value);
    return true;
}

bool summa_hash_map_remove(SummaHashMap map, void* key) {
    SummaHashCode code = summa_hash(key, map->key_size);
    size_t        index;
    if (!summa_hash_map_keys_index_of(map->keys, &code, &index)) {
        return false;
    }
    summa_hash_map_keys_remove_at(map->keys, index);
    summa_hash_map_values_remove_at(map->values, index);
    return true;
}

void summa_hash_map_clear(SummaHashMap map) {
    summa_hash_map_keys_clear(map->keys);
    summa_hash_map_values_clear(map->values);
}

void summa_hash_map_copy(SummaHashMap dest, SummaHashMap src) {
    dest->key_size = src->key_size;
    summa_hash_map_keys_copy(dest->keys, src->keys);
    summa_hash_map_values_copy(dest->values, src->values);
}

void summa_hash_map_free(SummaHashMap map) {
    summa_hash_map_values_free(map->values);
    map->values = nullptr;
    summa_hash_map_keys_free(map->keys);
    map->keys = nullptr;
    free(map);
}

#pragma endregion Hash maps

#endif /* SUMMA_HASH_MAP_IMPLEMENTATION_ONCE */
#endif /* SUMMA_HASH_MAP_IMPLEMENTATION */
