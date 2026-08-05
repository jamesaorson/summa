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
SummaArray summa_array_make_with_capacity(size_t element_size, size_t capacity);
void       summa_array_reserve(SummaArray arr, size_t capacity);
void       summa_array_clear(SummaArray str);
bool       summa_array_copy(SummaArray dest, SummaArray src);
bool       summa_array_copy_raw(SummaArray dest, void* raw, size_t len);
void       summa_array_free(SummaArray arr);
void       summa_array_push(SummaArray arr, void* element);
bool       summa_array_contains(SummaArray arr, void* element);
void       summa_array_remove_at(SummaArray arr, size_t index);
bool       summa_array_index_of(SummaArray arr, void* element, size_t* out_index);
void       summa_array_set_at(SummaArray arr, size_t index, void* element);

/* The same array, in a header the caller already has. `init` is
 * `make_with_capacity` writing into storage it did not allocate, and `dispose`
 * is `free` releasing the elements and leaving the header alone -- so an array
 * can be a field of a bigger struct rather than a second allocation beside it.
 * Everything else above works on it unchanged, growth included. */
void summa_array_init_with_capacity(SummaArray arr, size_t element_size, size_t capacity);
void summa_array_dispose(SummaArray arr);

#endif

#ifdef SUMMA_ARRAY_IMPLEMENTATION
#ifndef SUMMA_ARRAY_IMPLEMENTATION_ONCE
#define SUMMA_ARRAY_IMPLEMENTATION_ONCE

#include <stdlib.h>
#include <string.h>

#define SUMMA_ARRAY_DEFAULT_CAPACITY 8

SummaArray summa_array_make(const void* elements, size_t num_elements, size_t element_size) {
    SummaArray array = malloc(sizeof(SummaArray_t));
    array->elements  = calloc(num_elements, element_size);
    if (num_elements > 0) {
        memcpy(array->elements, elements, num_elements * element_size);
    }
    array->capacity     = num_elements;
    array->length       = num_elements;
    array->element_size = element_size;
    return array;
}

SummaArray summa_array_make_empty(size_t element_size) {
    return summa_array_make_with_capacity(element_size, SUMMA_ARRAY_DEFAULT_CAPACITY);
}

/* The default capacity is a guess for a caller that has none. A caller that
 * knows how many elements are coming says so here instead, and pays for exactly
 * that many.
 *
 * Capacity 0 allocates no element storage at all -- `elements` stays null,
 * which every growth path already handles, since realloc(null, n) is how the
 * standard spells a first allocation. */
SummaArray summa_array_make_with_capacity(size_t element_size, size_t capacity) {
    SummaArray array = malloc(sizeof(SummaArray_t));
    summa_array_init_with_capacity(array, element_size, capacity);
    return array;
}

/* An array whose header is not its own allocation. The header is the caller's
 * -- a field, a local, anything with the right lifetime -- and only the element
 * storage is malloc'd, so an owner that would otherwise pay a second
 * malloc/free pair per instance pays none.
 *
 * Nothing downstream can tell the difference: `elements` is an ordinary heap
 * pointer that grows through `realloc` exactly as it always did. Only the
 * teardown differs, and `summa_array_dispose` is the whole of it. */
void summa_array_init_with_capacity(SummaArray arr, size_t element_size, size_t capacity) {
    arr->elements     = capacity > 0 ? malloc(element_size * capacity) : nullptr;
    arr->capacity     = capacity;
    arr->length       = 0;
    arr->element_size = element_size;
}

/* Releases the element storage and leaves the header where it is, emptied. The
 * counterpart to `init`, and the two-line difference from `free`.
 *
 * Emptied rather than merely freed, so a header that outlives its contents --
 * one embedded in a struct being torn down around it -- reads as a zero-length
 * array rather than as a dangling pointer. */
void summa_array_dispose(SummaArray arr) {
    free(arr->elements);
    arr->elements = nullptr;
    arr->capacity = 0;
    arr->length   = 0;
}

/* Grows to hold at least `capacity` elements, and never shrinks -- an array
 * that already has the room is left exactly as it was, so this is safe to call
 * on a hot path where the answer is usually "already big enough". */
void summa_array_reserve(SummaArray arr, size_t capacity) {
    if (capacity <= arr->capacity) {
        return;
    }
    arr->elements = realloc(arr->elements, capacity * arr->element_size);
    arr->capacity = capacity;
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
    /* Guarded because a zero-capacity array's `elements` is null, and memcpy is
     * undefined on a null pointer even for a length of zero. */
    if (len > 0) {
        memcpy(dest->elements, src->elements, dest->element_size * len);
    }
    dest->length = len;
    return true;
}

bool summa_array_copy_raw(SummaArray dest, void* raw, size_t len) {
    if (dest->capacity < len) {
        dest->elements = realloc(dest->elements, dest->element_size * (len + 1));
        dest->capacity = len + 1;
    }
    if (len > 0) {
        memcpy(dest->elements, raw, dest->element_size * len);
    }
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
    memcpy((char*)arr->elements + (arr->length * arr->element_size), element, arr->element_size);
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

void summa_array_remove_at(SummaArray arr, size_t index) {
    if (index >= arr->length) {
        return;
    }
    size_t tail_count = arr->length - index - 1;
    if (tail_count > 0) {
        memmove((char*)arr->elements + (index * arr->element_size),
                (char*)arr->elements + ((index + 1) * arr->element_size),
                tail_count * arr->element_size);
    }
    arr->length--;
}

bool summa_array_index_of(SummaArray arr, void* element, size_t* out_index) {
    for (size_t i = 0; i < arr->length; i++) {
        if (memcmp((char*)arr->elements + (i * arr->element_size), element, arr->element_size) == 0) {
            if (out_index) {
                *out_index = i;
            }
            return true;
        }
    }
    return false;
}

void summa_array_set_at(SummaArray arr, size_t index, void* element) {
    if (index >= arr->length) {
        return;
    }
    memcpy((char*)arr->elements + (index * arr->element_size), element, arr->element_size);
}

#include <summa/macros/macros.h>

#define SUMMA_ARRAY_GENERATE_TYPE_DEF(NewType, NewTypeNameForFunctions, ValueType) \
    typedef struct {                                                               \
        ValueType* value;                                                          \
        size_t     length;                                                         \
        size_t     capacity;                                                       \
        size_t     element_size;                                                   \
    } SUMMA_TOKEN_CONCAT2(NewType, _t);                                            \
    typedef SUMMA_TOKEN_CONCAT2(NewType, _t) * NewType;

#define SUMMA_ARRAY_GENERATE_TYPE_IMPL(NewType, NewTypeNameForFunctions, ValueType)                                  \
    NewType SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _make)(ValueType * elements, size_t num_elements) { \
        return (NewType)summa_array_make(elements, num_elements, sizeof(ValueType));                                 \
    }                                                                                                                \
    NewType SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _make_empty)() {                                    \
        return (NewType)summa_array_make_empty(sizeof(ValueType));                                                   \
    }                                                                                                                \
    NewType SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _make_with_capacity)(size_t capacity) {             \
        return (NewType)summa_array_make_with_capacity(sizeof(ValueType), capacity);                                 \
    }                                                                                                                \
    void SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _init_with_capacity)(NewType arr, size_t capacity) {   \
        summa_array_init_with_capacity((SummaArray)arr, sizeof(ValueType), capacity);                                \
    }                                                                                                                \
    void SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _dispose)(NewType arr) {                               \
        summa_array_dispose((SummaArray)arr);                                                                        \
    }                                                                                                                \
    void SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _reserve)(NewType arr, size_t capacity) {              \
        summa_array_reserve((SummaArray)arr, capacity);                                                              \
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
    }                                                                                                                \
    void SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _remove_at)(NewType arr, size_t index) {               \
        summa_array_remove_at((SummaArray)arr, index);                                                               \
    }                                                                                                                \
    bool SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _index_of)(                                            \
        NewType arr, ValueType * element, size_t* out_index) {                                                       \
        return summa_array_index_of((SummaArray)arr, (void*)element, out_index);                                     \
    }                                                                                                                \
    void SUMMA_TOKEN_CONCAT3(summa_, NewTypeNameForFunctions, _set_at)(                                              \
        NewType arr, size_t index, ValueType* element) {                                                             \
        summa_array_set_at((SummaArray)arr, index, (void*)element);                                                  \
    }

#define SUMMA_ARRAY_GENERATE_TYPE(NewType, NewTypeNameForFunctions, ValueType) \
    SUMMA_ARRAY_GENERATE_TYPE_DEF(NewType, NewTypeNameForFunctions, ValueType) \
    SUMMA_ARRAY_GENERATE_TYPE_IMPL(NewType, NewTypeNameForFunctions, ValueType)
#endif /* SUMMA_ARRAY_IMPLEMENTATION_ONCE */
#endif /* SUMMA_ARRAY_IMPLEMENTATION */
