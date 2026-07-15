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

typedef struct SummaHashMap     SummaHashMap;
typedef struct SummaHashIndices SummaHashIndices;
typedef struct SummaHashValues  SummaHashValues;

SummaHashMap summa_hash_map_make(void** values, size_t num_elements, size_t element_size);
SummaHashMap summa_hash_map_make_empty();
void         summa_hash_map_clear(SummaHashMap map);
void         summa_hash_map_copy(SummaHashMap dest, SummaHashMap src);
void         summa_hash_map_free(SummaHashMap map);

#pragma endregion Hash maps

#endif

#ifdef SUMMA_HASHMAP_IMPLEMENTATION
#ifndef SUMMA_HASHMAP_IMPLEMENTATION_ONCE
#define SUMMA_HASHMAP_IMPLEMENTATION_ONCE

#pragma region Hash codes

// Reference: https://www.cse.yorku.ca/~oz/hash.html

#define _SUMMA_HASH_MAGIC 5381

#define _SUMMA_HASH_IMPL(hash, c) (hash) = (((hash) << 5) + (hash)) + (c) /* hash * 33 + c */

SummaHashCode summa_hash(void* value, size_t size) {
    SummaHashCodeMutable hash = _SUMMA_HASH_MAGIC;

    for (int i = 0; i < size; i++) {
        int c = ((char*)value)[i];
        _SUMMA_HASH_IMPL(hash, c);
    }

    return hash;
}

#pragma endregion Hash codes

#pragma region Hash maps

#define SUMMA_ARRAY_IMPLEMENTATION
#include <summa/array/array.h>

typedef struct {
    SummaHashCode* value;
    size_t         length;
    size_t         capacity;
} summa_hash_indices_t;
typedef summa_hash_indices_t* SummaHashIndices;

SummaHashIndices summa_hash_indices_make_empty() {
    SummaHashIndices indices = (SummaHashIndices)summa_array_make_empty(sizeof(SummaHashCode));
    return indices;
}

void summa_hash_indices_clear(SummaHashIndices indices) {
    summa_array_clear((SummaHashIndices)indices);
}

void summa_hash_indices_copy(SummaHashIndices dest, SummaHashIndices src) {
    summa_array_copy((SummaHashIndices)dest, (SummaHashIndices)src);
}

void summa_hash_indices_free(SummaHashIndices indices) {
    summa_array_free((SummaHashIndices)indices);
}

bool summa_hash_indices_contains_hash_code(SummaHashIndices indices, SummaHashCode code) {
    for (int i = 0; i < indices->length; i++) {
        if (indices->value[i] == code) {
            return true;
        }
    }
    return false;
}

typedef struct {
    void*  value;
    size_t length;
    size_t capacity;
} _summa_hash_values_t;
typedef _summa_hash_values_t* SummaHashValues;

SummaHashValues summa_hash_values_make_empty() {
    SummaHashValues values = (SummaHashValues)summa_array_make_empty(sizeof(SummaHashCode));
    return values;
}
void summa_hash_values_clear(SummaHashValues values) {
    summa_array_clear((SummaHashValues)values);
}
void summa_hash_values_copy(SummaHashValues dest, SummaHashValues src) {
    summa_array_copy((SummaHashValues)dest, (SummaHashValues)src);
}
void summa_hash_values_free(SummaHashValues values) {
    summa_array_free((SummaHashValues)values);
}

struct SummaHashMap {
    SummaHashIndices indices;
    SummaHashValues  values;
};

typedef struct {
    SummaHashIndices indices;
    SummaHashValues  values;
    size_t           element_size;
} summa_hash_map_t;
typedef summa_hash_map_t* SummaHashMap;

SummaHashMap summa_hash_map_make(void** values, size_t num_elements, size_t element_size) {
    SummaHashMap map  = (SummaHashMap)malloc(sizeof(summa_hash_map_t));
    map->element_size = element_size;

    map->indices = summa_hash_indices_make_empty();
    map->values  = summa_hash_values_make_empty();

    for (int i = 0; i < num_elements; i++) {
        SummaHashCode code = summa_hash(values[i], num_elements * element_size);
        if (!summa_hash_indices_contains_hash_code(map->indices, code)) {
            // TODO: Add to next empty index, adding a new slot to the end if needed
            // TODO: Add value to same i as new slot
        }
    }

    return map;
}

SummaHashMap summa_hash_map_make_empty() {}

void summa_hash_map_clear(SummaHashMap map) {}

void summa_hash_map_copy(SummaHashMap dest, SummaHashMap src) {}

void summa_hash_map_free(SummaHashMap map) {}

#pragma endregion Hash maps

#endif /* SUMMA_HASHMAP_IMPLEMENTATION_ONCE */
#endif /* SUMMA_HASHMAP_IMPLEMENTATION */
