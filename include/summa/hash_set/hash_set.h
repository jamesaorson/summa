#ifndef SUMMA_HASH_SET_H
#define SUMMA_HASH_SET_H

#include <stdbool.h>
#include <stddef.h>

#pragma region Hash codes

typedef unsigned long        SummaHashCodeMutable;
typedef SummaHashCodeMutable SummaHashCode;

SummaHashCode summa_hash(const void* value, const size_t size);

#pragma endregion Hash codes

#pragma region Hash sets

typedef struct summa_hash_set_t* SummaHashSet;

#define SUMMA_ARRAY_IMPLEMENTATION
#include <summa/array/array.h>

SUMMA_ARRAY_GENERATE_TYPE_DEF(SummaHashSetKeys, hash_set_keys, SummaHashCode)
SUMMA_ARRAY_GENERATE_TYPE_DEF(SummaHashSetValues, hash_set_values, void*)

SummaHashSet summa_hash_set_make(void** values, size_t num_elements, size_t element_size);
SummaHashSet summa_hash_set_make_empty(size_t element_size);
bool         summa_hash_set_contains(SummaHashSet set, void* element);
bool         summa_hash_set_add(SummaHashSet set, void* element);
bool         summa_hash_set_remove(SummaHashSet set, void* element);
void         summa_hash_set_clear(SummaHashSet set);
void         summa_hash_set_copy(SummaHashSet dest, SummaHashSet src);
void         summa_hash_set_free(SummaHashSet set);

#pragma endregion Hash sets

#endif

#ifdef SUMMA_HASH_SET_IMPLEMENTATION
#ifndef SUMMA_HASH_SET_IMPLEMENTATION_ONCE
#define SUMMA_HASH_SET_IMPLEMENTATION_ONCE

#include <stdlib.h>
#include <string.h>

#pragma region Hash codes

// Reference: https://www.cse.yorku.ca/~oz/hash.html

#define _SUMMA_HASH_MAGIC 5381

#define _SUMMA_HASH_IMPL(hash, c) (hash) = (((hash) << 5) + (hash)) + (c) /* hash * 33 + c */

SummaHashCode summa_hash(const void* value, const size_t size) {
    SummaHashCodeMutable hash = _SUMMA_HASH_MAGIC;

    for (size_t i = 0; i < size; i++) {
        int c = ((char*)value)[i];
        _SUMMA_HASH_IMPL(hash, c);
    }

    return hash;
}

#pragma endregion Hash codes

#pragma region Hash sets

SUMMA_ARRAY_GENERATE_TYPE_IMPL(SummaHashSetKeys, hash_set_keys, SummaHashCode)
SUMMA_ARRAY_GENERATE_TYPE_IMPL(SummaHashSetValues, hash_set_values, void*)

struct SummaHashSet {
    SummaHashSetKeys   keys;
    SummaHashSetValues values;
};

struct summa_hash_set_t {
    SummaHashSetKeys   keys;
    SummaHashSetValues values;
    size_t             element_size;
};

SummaHashSet summa_hash_set_make(void** elements, size_t num_elements, size_t element_size) {
    SummaHashSet set  = (SummaHashSet)malloc(sizeof(struct summa_hash_set_t));
    set->element_size = element_size;

    set->keys   = summa_hash_set_keys_make_empty();
    set->values = summa_hash_set_values_make_empty();

    for (size_t i = 0; i < num_elements; i++) {
        summa_hash_set_add(set, elements[i]);
    }

    return set;
}

SummaHashSet summa_hash_set_make_empty(size_t element_size) {
    SummaHashSet set  = (SummaHashSet)malloc(sizeof(struct summa_hash_set_t));
    set->element_size = element_size;
    set->keys         = summa_hash_set_keys_make_empty();
    set->values       = summa_hash_set_values_make_empty();
    return set;
}

bool summa_hash_set_contains(SummaHashSet set, void* element) {
    SummaHashCode code = summa_hash(element, set->element_size);
    return summa_hash_set_keys_contains(set->keys, &code);
}

bool summa_hash_set_add(SummaHashSet set, void* element) {
    SummaHashCode code = summa_hash(element, set->element_size);
    if (summa_hash_set_keys_contains(set->keys, &code)) {
        return false;
    }
    summa_hash_set_keys_push(set->keys, &code);
    summa_hash_set_values_push(set->values, &element);
    return true;
}

bool summa_hash_set_remove(SummaHashSet set, void* element) {
    SummaHashCode code = summa_hash(element, set->element_size);
    for (size_t i = 0; i < set->keys->length; i++) {
        if (set->keys->value[i] == code) {
            summa_hash_set_keys_remove_at(set->keys, i);
            summa_hash_set_values_remove_at(set->values, i);
            return true;
        }
    }
    return false;
}

void summa_hash_set_clear(SummaHashSet set) {
    summa_hash_set_keys_clear(set->keys);
}

void summa_hash_set_copy(SummaHashSet dest, SummaHashSet src) {
    dest->element_size = src->element_size;
    if (dest->keys) {
        summa_hash_set_keys_free(dest->keys);
        dest->keys = summa_hash_set_keys_make_empty();
    }
    if (dest->values) {
        summa_hash_set_values_free(dest->values);
        dest->values = summa_hash_set_values_make_empty();
    }
    summa_hash_set_keys_copy(dest->keys, src->keys);
    summa_hash_set_values_copy(dest->values, src->values);
}

void summa_hash_set_free(SummaHashSet set) {
    summa_hash_set_values_free(set->values);
    set->values = nullptr;
    summa_hash_set_keys_free(set->keys);
    set->keys = nullptr;
    free(set);
}

#pragma endregion HASH_SETs

#endif /* SUMMA_HASH_SET_IMPLEMENTATION_ONCE */
#endif /* SUMMA_HASH_SET_IMPLEMENTATION */
