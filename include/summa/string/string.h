#ifndef SUMMA_STRING_H
#define SUMMA_STRING_H

#define SUMMA_ARRAY_IMPLEMENTATION
#include <summa/array/array.h>

SUMMA_ARRAY_GENERATE_TYPE_DEF(SummaString, string, char)

SummaString summa_string_make(const char* value);
SummaString summa_string_make_empty();
void        summa_string_clear(SummaString str);
void        summa_string_copy(SummaString dest, SummaString src);
void        summa_string_copy_cstr(SummaString dest, const char* src);
void        summa_string_free(SummaString str);
int         summa_string_cmp(SummaString left, SummaString right);

#endif

#ifdef SUMMA_STRING_IMPLEMENTATION
#ifndef SUMMA_STRING_IMPLEMENTATION_ONCE
#define SUMMA_STRING_IMPLEMENTATION_ONCE

#include <string.h>

SummaString summa_string_make(const char* value) {
    SummaString str = (SummaString)summa_array_make(value, strlen(value) + 1, sizeof(char));
    str->length--;
    return str;
}
SummaString summa_string_make_empty() {
    return (SummaString)summa_array_make_empty(sizeof(char));
}
void summa_string_clear(SummaString str) {
    summa_array_clear((SummaArray)str);
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
int summa_string_cmp(SummaString left, SummaString right) {
    return strcmp(left->value, right->value);
}

/* Add your function definitions here */

#endif /* SUMMA_STRING_IMPLEMENTATION_ONCE */
#endif /* SUMMA_STRING_IMPLEMENTATION */
