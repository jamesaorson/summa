#ifndef SUMMA_HASHMAP_H
#define SUMMA_HASHMAP_H

#include <stdbool.h>
#include <stddef.h>

#pragma region Hash codes

typedef unsigned long              SummaHashCodeMutable;
typedef const SummaHashCodeMutable SummaHashCode;

SummaHashCode summa_hash(void* value, size_t size);

#pragma endregion Hash codes

#pragma region Hash maps

typedef struct summa_hash_map_t* SummaHashMap;

#define SUMMA_ARRAY_IMPLEMENTATION
#include <summa/array/array.h>

SUMMA_ARRAY_GENERATE_TYPE_DEF(SummaHashIndices, hash_indices, SummaHashCode)
SUMMA_ARRAY_GENERATE_TYPE_DEF(SummaHashValues, hash_values, void*)

SummaHashMap summa_hash_map_make(void** values, size_t num_elements, size_t element_size);
SummaHashMap summa_hash_map_make_empty(size_t element_size);
void         summa_hash_map_clear(SummaHashMap map);
void         summa_hash_map_copy(SummaHashMap dest, SummaHashMap src);
void         summa_hash_map_free(SummaHashMap map);

#pragma endregion Hash maps

#endif

#ifdef SUMMA_HASHMAP_IMPLEMENTATION
#ifndef SUMMA_HASHMAP_IMPLEMENTATION_ONCE
#define SUMMA_HASHMAP_IMPLEMENTATION_ONCE

#include <stdlib.h>
#include <string.h>

#pragma region Hash codes

// Reference: https://www.cse.yorku.ca/~oz/hash.html

#define _SUMMA_HASH_MAGIC 5381

#define _SUMMA_HASH_IMPL(hash, c) (hash) = (((hash) << 5) + (hash)) + (c) /* hash * 33 + c */

SummaHashCode summa_hash(void* value, size_t size) {
    SummaHashCodeMutable hash = _SUMMA_HASH_MAGIC;

    for (size_t i = 0; i < size; i++) {
        int c = ((char*)value)[i];
        _SUMMA_HASH_IMPL(hash, c);
    }

    return hash;
}

#pragma endregion Hash codes

#pragma region Hash maps

SUMMA_ARRAY_GENERATE_TYPE_IMPL(SummaHashIndices, hash_indices, SummaHashCode)
SUMMA_ARRAY_GENERATE_TYPE_IMPL(SummaHashValues, hash_values, void*)

struct SummaHashMap {
    SummaHashIndices indices;
    SummaHashValues  values;
};

struct summa_hash_map_t {
    SummaHashIndices indices;
    SummaHashValues  values;
    size_t           element_size;
};

SummaHashMap summa_hash_map_make(void** values, size_t num_elements, size_t element_size) {
    SummaHashMap map  = (SummaHashMap)malloc(sizeof(struct summa_hash_map_t));
    map->element_size = element_size;

    map->indices = summa_hash_indices_make_empty();
    map->values  = summa_hash_values_make_empty();

    for (size_t i = 0; i < num_elements; i++) {
        SummaHashCode code = summa_hash(values[i], num_elements * element_size);
        if (!summa_hash_indices_contains(map->indices, &code)) {
            // TODO: Add to next empty index, adding a new slot to the end if needed
            // TODO: Add value to same i as new slot
        }
    }

    return map;
}

SummaHashMap summa_hash_map_make_empty(size_t element_size) {
    SummaHashMap map  = (SummaHashMap)malloc(sizeof(struct summa_hash_map_t));
    map->element_size = element_size;
    map->indices      = summa_hash_indices_make_empty();
    map->values       = summa_hash_values_make_empty();
    return map;
}

void summa_hash_map_clear(SummaHashMap map) {
    summa_hash_indices_clear(map->indices);
}

void summa_hash_map_copy(SummaHashMap dest, SummaHashMap src) {
    dest->element_size = src->element_size;
    if (dest->indices) {
        summa_hash_indices_free(dest->indices);
        dest->indices = summa_hash_indices_make_empty();
    }
    if (dest->values) {
        summa_hash_values_free(dest->values);
        dest->values = summa_hash_values_make_empty();
    }
    summa_hash_indices_copy(dest->indices, src->indices);
    summa_hash_values_copy(dest->values, src->values);
}

void summa_hash_map_free(SummaHashMap map) {
    summa_hash_values_free(map->values);
    map->values = nullptr;
    summa_hash_indices_free(map->indices);
    map->indices = nullptr;
    free(map);
}

#pragma endregion Hash maps

#endif /* SUMMA_HASHMAP_IMPLEMENTATION_ONCE */
#endif /* SUMMA_HASHMAP_IMPLEMENTATION */
