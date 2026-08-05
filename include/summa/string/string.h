#ifndef SUMMA_STRING_H
#define SUMMA_STRING_H

#define SUMMA_ARRAY_IMPLEMENTATION
#include <summa/array/array.h>

SUMMA_ARRAY_GENERATE_TYPE_DEF(SummaString, string, char)

/* A SummaString is a SummaArray of char with one added invariant: a null byte
 * always sits at `value[length]`, so `value` is a valid C string at any point
 * and `capacity` is never less than `length + 1`.
 *
 * That invariant is why these exist rather than the generated array functions
 * being used directly -- summa_array_push grows only enough for the element it
 * is adding, which would leave no room for the terminator. */

SummaString summa_string_make(const char* value);
SummaString summa_string_make_empty();
void        summa_string_clear(SummaString str);

/* The same string, in a header the caller already has -- `summa_array_init_with_capacity`
 * and `summa_array_dispose` for a type whose invariant the raw pair would not
 * maintain. `init` fills a header it did not allocate; `dispose` releases the
 * characters and leaves the header behind emptied.
 *
 * One caveat `dispose` inherits from the array: an emptied string has a null
 * `value`, so it is no longer a readable C string. That is teardown's business
 * and nobody else's -- an owner disposing a string is on its way out. */
void summa_string_init(SummaString str, const char* value);
void summa_string_dispose(SummaString str);
void summa_string_copy(SummaString dest, SummaString src);
void summa_string_copy_cstr(SummaString dest, const char* src);
void summa_string_free(SummaString str);

/* Appends one character, growing as needed. */
void summa_string_push(SummaString str, char c);

/* The SummaArray operations, taking a char by value rather than by address. */
bool summa_string_contains(SummaString str, char c);
bool summa_string_index_of(SummaString str, char c, size_t* out_index);

/* Both ignore an out-of-range index, as the array versions do. */
void summa_string_remove_at(SummaString str, size_t index);
void summa_string_set_at(SummaString str, size_t index, char c);

int summa_string_cmp(SummaString left, SummaString right);

#endif

#ifdef SUMMA_STRING_IMPLEMENTATION
#ifndef SUMMA_STRING_IMPLEMENTATION_ONCE
#define SUMMA_STRING_IMPLEMENTATION_ONCE

#include <stdlib.h>
#include <string.h>

SummaString summa_string_make(const char* value) {
    SummaString str = (SummaString)summa_array_make(value, strlen(value) + 1, sizeof(char));
    str->length--;
    return str;
}
SummaString summa_string_make_empty() {
    SummaString str = (SummaString)summa_array_make_empty(sizeof(char));
    /* summa_array_make_empty leaves its allocation uninitialized, which for a
     * string would mean `value` reads as whatever was on the heap. */
    str->value[0] = '\0';
    return str;
}
/* Capacity is length + 1 rather than length, which is the string invariant: the
 * terminator has to have somewhere to live. */
void summa_string_init(SummaString str, const char* value) {
    const size_t length = strlen(value);
    summa_array_init_with_capacity((SummaArray)str, sizeof(char), length + 1);
    memcpy(str->value, value, length + 1);
    str->length = length;
}
void summa_string_dispose(SummaString str) {
    summa_array_dispose((SummaArray)str);
}
void summa_string_clear(SummaString str) {
    summa_array_clear((SummaArray)str);
    /* Length alone is not enough: without this the old contents still read back
     * through `value`. */
    str->value[0] = '\0';
}
void summa_string_copy(SummaString dest, SummaString src) {
    /* Copy length+1 bytes (like summa_string_make) so the null terminator that
     * lives just past `length` is preserved, then correct the length back down. */
    summa_array_copy_raw((SummaArray)dest, src->value, src->length + 1);
    dest->length--;
}
void summa_string_copy_cstr(SummaString dest, const char* src) {
    size_t len = strlen(src);
    summa_array_copy_raw((SummaArray)dest, (void*)src, len + 1);
    dest->length--;
}
void summa_string_free(SummaString str) {
    summa_array_free((SummaArray)str);
}
void summa_string_push(SummaString str, char c) {
    /* Room for the character and the terminator that follows it, which is the
     * one place summa_array_push cannot be reused: it stops growing at
     * `length`, leaving the null byte nowhere to go. */
    if (str->capacity < str->length + 2) {
        size_t capacity =
            str->capacity < SUMMA_ARRAY_DEFAULT_CAPACITY ? SUMMA_ARRAY_DEFAULT_CAPACITY : str->capacity * 2;
        if (capacity < str->length + 2) {
            capacity = str->length + 2;
        }
        str->value    = realloc(str->value, capacity * sizeof(char));
        str->capacity = capacity;
    }
    str->value[str->length] = c;
    str->length++;
    str->value[str->length] = '\0';
}

bool summa_string_contains(SummaString str, char c) {
    return summa_array_contains((SummaArray)str, &c);
}

bool summa_string_index_of(SummaString str, char c, size_t* out_index) {
    return summa_array_index_of((SummaArray)str, &c, out_index);
}

void summa_string_remove_at(SummaString str, size_t index) {
    if (index >= str->length) {
        return;
    }
    summa_array_remove_at((SummaArray)str, index);
    /* The shift moved the old terminator down with the tail, so it is rewritten
     * at the length the removal left behind. */
    str->value[str->length] = '\0';
}

void summa_string_set_at(SummaString str, size_t index, char c) {
    summa_array_set_at((SummaArray)str, index, &c);
}

int summa_string_cmp(SummaString left, SummaString right) {
    return strcmp(left->value, right->value);
}

#endif /* SUMMA_STRING_IMPLEMENTATION_ONCE */
#endif /* SUMMA_STRING_IMPLEMENTATION */
