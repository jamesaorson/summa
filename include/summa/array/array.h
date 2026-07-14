#ifndef SUMMA_ARRAY_H
#define SUMMA_ARRAY_H

#include <stddef.h>

typedef struct {
    void*  value;
    size_t length;
    size_t capacity;
    size_t element_size;
} summa_array_t;
typedef summa_array_t* SummaArray;

SummaArray summa_array_make(const void* value, size_t num_elements, size_t element_size);
SummaArray summa_array_make_empty(size_t element_size);
void       summa_array_clear(SummaArray str);
bool       summa_array_copy(SummaArray dest, SummaArray src);
bool       summa_array_copy_raw(SummaArray dest, void* raw, size_t len);
void       summa_array_free(SummaArray arr);
void       summa_array_push(SummaArray arr, void* element);

#endif

#ifdef SUMMA_ARRAY_IMPLEMENTATION
#ifndef SUMMA_ARRAY_IMPLEMENTATION_ONCE
#define SUMMA_ARRAY_IMPLEMENTATION_ONCE

#define SUMMA_ARRAY_DEFAULT_CAPACITY 8

SummaArray summa_array_make(const void* value, size_t num_elements, size_t element_size) {
    SummaArray array = malloc(sizeof(summa_array_t));
    array->value     = calloc(num_elements, element_size);
    memcpy(array->value, value, num_elements * element_size);
    array->capacity     = num_elements;
    array->length       = num_elements;
    array->element_size = element_size;
    return array;
}
SummaArray summa_array_make_empty(size_t element_size) {
    SummaArray array    = malloc(sizeof(summa_array_t));
    array->value        = malloc(element_size * SUMMA_ARRAY_DEFAULT_CAPACITY);
    array->capacity     = SUMMA_ARRAY_DEFAULT_CAPACITY;
    array->length       = 0;
    array->element_size = element_size;
    return array;
}

void summa_array_clear(SummaArray arr) { arr->length = 0; }

bool summa_array_copy(SummaArray dest, SummaArray src) {
    if (dest->element_size != src->element_size) {
        return false;
    }
    size_t len = src->length;
    if (dest->capacity < len) {
        dest->value    = realloc(dest->value, dest->element_size * (len + 1));
        dest->capacity = len + 1;
    }
    memcpy(dest->value, src->value, dest->element_size * len);
    dest->length = len;
    return true;
}

bool summa_array_copy_raw(SummaArray dest, void* raw, size_t len) {
    if (dest->capacity < len) {
        dest->value    = realloc(dest->value, dest->element_size * (len + 1));
        dest->capacity = len + 1;
    }
    memcpy(dest->value, raw, dest->element_size * len);
    dest->length = len;
    return true;
}

void summa_array_free(SummaArray arr) {
    free(arr->value);
    free(arr);
}

void summa_array_push(SummaArray arr, void* element) {
    if (arr->capacity == 0) {
        arr->capacity = SUMMA_ARRAY_DEFAULT_CAPACITY;
        arr->value    = realloc(arr->value, arr->capacity * arr->element_size);
    } else if (arr->length >= arr->capacity) {
        arr->capacity *= 2;
        arr->value = realloc(arr->value, arr->capacity * arr->element_size);
    }
    memcpy((char*)arr->value + arr->length * arr->element_size, element, arr->element_size);
    arr->length++;
}

#endif /* SUMMA_ARRAY_IMPLEMENTATION_ONCE */
#endif /* SUMMA_ARRAY_IMPLEMENTATION */
