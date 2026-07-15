#ifndef SUMMA_ARRAY_H
#define SUMMA_ARRAY_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    void*  elements;
    size_t length;
    size_t capacity;
    size_t element_size;
} SummaArray_t;
typedef SummaArray_t* SummaArray;

SummaArray summa_array_make(const void* elements, size_t num_elements, size_t element_size);
SummaArray summa_array_make_empty(size_t element_size);
void       summa_array_clear(SummaArray str);
bool       summa_array_copy(SummaArray dest, SummaArray src);
bool       summa_array_copy_raw(SummaArray dest, void* raw, size_t len);
void       summa_array_free(SummaArray arr);
void       summa_array_push(SummaArray arr, void* element);
bool       summa_array_contains(SummaArray arr, void* element);

#endif

#ifdef SUMMA_ARRAY_IMPLEMENTATION
#ifndef SUMMA_ARRAY_IMPLEMENTATION_ONCE
#define SUMMA_ARRAY_IMPLEMENTATION_ONCE

#define SUMMA_ARRAY_DEFAULT_CAPACITY 8

SummaArray summa_array_make(const void* elements, size_t num_elements, size_t element_size) {
    SummaArray array = malloc(sizeof(SummaArray_t));
    array->elements  = calloc(num_elements, element_size);
    memcpy(array->elements, elements, num_elements * element_size);
    array->capacity     = num_elements;
    array->length       = num_elements;
    array->element_size = element_size;
    return array;
}

SummaArray summa_array_make_empty(size_t element_size) {
    SummaArray array    = malloc(sizeof(SummaArray_t));
    array->elements     = malloc(element_size * SUMMA_ARRAY_DEFAULT_CAPACITY);
    array->capacity     = SUMMA_ARRAY_DEFAULT_CAPACITY;
    array->length       = 0;
    array->element_size = element_size;
    return array;
}

void summa_array_clear(SummaArray arr) {
    arr->length = 0;
}

bool summa_array_copy(SummaArray dest, SummaArray src) {
    if (dest->element_size != src->element_size) {
        return false;
    }
    size_t len = src->length;
    if (dest->capacity < len) {
        dest->elements = realloc(dest->elements, dest->element_size * (len + 1));
        dest->capacity = len + 1;
    }
    memcpy(dest->elements, src->elements, dest->element_size * len);
    dest->length = len;
    return true;
}

bool summa_array_copy_raw(SummaArray dest, void* raw, size_t len) {
    if (dest->capacity < len) {
        dest->elements = realloc(dest->elements, dest->element_size * (len + 1));
        dest->capacity = len + 1;
    }
    memcpy(dest->elements, raw, dest->element_size * len);
    dest->length = len;
    return true;
}

void summa_array_free(SummaArray arr) {
    free(arr->elements);
    free(arr);
}

void summa_array_push(SummaArray arr, void* element) {
    if (arr->capacity == 0) {
        arr->capacity = SUMMA_ARRAY_DEFAULT_CAPACITY;
        arr->elements = realloc(arr->elements, arr->capacity * arr->element_size);
    } else if (arr->length >= arr->capacity) {
        arr->capacity *= 2;
        arr->elements = realloc(arr->elements, arr->capacity * arr->element_size);
    }
    memcpy((char*)arr->elements + arr->length * arr->element_size, element, arr->element_size);
    arr->length++;
}

bool summa_array_contains(SummaArray arr, void* element) {
    for (size_t i = 0; i < arr->length; i++) {
        if (memcmp((char*)arr->elements + (i * arr->element_size), element, arr->element_size) == 0) {
            return true;
        }
    }
    return false;
}

#include <summa/macros/macros.h>

#define SUMMA_ARRAY_GENERATE_TYPE_DEF(NewType, NewTypeNameForFunctions, ValueType) \
    typedef struct {                                                               \
        ValueType* value;                                                          \
        size_t     length;                                                         \
        size_t     capacity;                                                       \
    } SUMMA_TOKEN_CONCAT2(NewType, _t);                                            \
    typedef SUMMA_TOKEN_CONCAT2(NewType, _t) * NewType;

#define SUMMA_ARRAY_GENERATE_TYPE_IMPL(NewType, NewTypeNameForFunctions, ValueType)                                  \
    NewType SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _make)(ValueType * elements, size_t num_elements) { \
        return (NewType)summa_array_make(elements, num_elements, sizeof(ValueType));                                 \
    }                                                                                                                \
    NewType SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _make_empty)() {                                    \
        return (NewType)summa_array_make_empty(sizeof(ValueType));                                                   \
    }                                                                                                                \
    void SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _clear)(NewType arr) {                                 \
        summa_array_clear((SummaArray)arr);                                                                          \
    }                                                                                                                \
    void SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _copy)(NewType dest, NewType src) {                    \
        summa_array_copy((SummaArray)dest, (SummaArray)src);                                                         \
    }                                                                                                                \
    void SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _free)(NewType arr) {                                  \
        summa_array_free((SummaArray)arr);                                                                           \
    }                                                                                                                \
    void SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _push)(NewType arr, ValueType * element) {             \
        summa_array_push((SummaArray)arr, (void*)element);                                                           \
    }                                                                                                                \
    bool SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _contains)(NewType arr, ValueType * element) {         \
        return summa_array_contains((SummaArray)arr, (void*)element);                                                \
    }

#define SUMMA_ARRAY_GENERATE_TYPE(NewType, NewTypeNameForFunctions, ValueType) \
    SUMMA_ARRAY_GENERATE_TYPE_DEF(NewType, NewTypeNameForFunctions, ValueType) \
    SUMMA_ARRAY_GENERATE_TYPE_IMPL(NewType, NewTypeNameForFunctions, ValueType)
#endif /* SUMMA_ARRAY_IMPLEMENTATION_ONCE */
#endif /* SUMMA_ARRAY_IMPLEMENTATION */
