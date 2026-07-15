#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#include <stdint.h>
#include <stdlib.h>

#define HELLO_WORLD "hello world"

void test_string_make() {
    SummaString str = summa_string_make(HELLO_WORLD);
    SUMMA_TEST_ASSERT_NOT_NULL(str);
    SUMMA_TEST_ASSERT_EQ_STR(HELLO_WORLD, str->value);
    SUMMA_TEST_ASSERT_EQ(strlen(HELLO_WORLD), str->length);
    /* Capacity reserves room for the trailing null byte, but length does not
     * count it: capacity == length + 1 for a freshly-made string. */
    SUMMA_TEST_ASSERT_EQ(str->length + 1, str->capacity);
    SUMMA_TEST_ASSERT_EQ('\0', str->value[str->length]);
    summa_string_free(str);
}

void test_string_make_empty_cstr() {
    SummaString str = summa_string_make("");
    SUMMA_TEST_ASSERT_NOT_NULL(str);
    SUMMA_TEST_ASSERT_EQ(0u, str->length);
    SUMMA_TEST_ASSERT_EQ(1u, str->capacity);
    SUMMA_TEST_ASSERT_EQ('\0', str->value[0]);
    summa_string_free(str);
}

void test_string_make_empty() {
    SummaString str = summa_string_make_empty();
    SUMMA_TEST_ASSERT_NOT_NULL(str);
    SUMMA_TEST_ASSERT_EQ(0u, str->length);
    SUMMA_TEST_ASSERT_EQ(SUMMA_ARRAY_DEFAULT_CAPACITY, str->capacity);
    /* summa_array_make_empty eagerly allocates the default capacity, so
     * value must not be a dangling/garbage pointer. */
    SUMMA_TEST_ASSERT_NOT_NULL(str->value);
    summa_string_free(str);
}

void test_string_clear() {
    SummaString str = summa_string_make(HELLO_WORLD);
    size_t      cap = str->capacity;
    summa_string_clear(str);
    SUMMA_TEST_ASSERT_EQ(0u, str->length);
    /* Clearing is a logical reset, not a dealloc: capacity (and the room it
     * reserves for a null byte) is untouched. */
    SUMMA_TEST_ASSERT_EQ(cap, str->capacity);
    summa_string_free(str);
}

void test_string_copy_grows_destination() {
    SummaString dest = summa_string_make("x");
    SummaString src  = summa_string_make(HELLO_WORLD);
    summa_string_copy(dest, src);
    SUMMA_TEST_ASSERT_EQ_STR(HELLO_WORLD, dest->value);
    SUMMA_TEST_ASSERT_EQ(strlen(HELLO_WORLD), dest->length);
    /* A growing copy reallocates, so capacity only has to leave room for the
     * null byte -- it isn't required to be the tightest possible fit. */
    SUMMA_TEST_ASSERT(dest->capacity > dest->length);
    SUMMA_TEST_ASSERT_EQ('\0', dest->value[dest->length]);
    summa_string_free(dest);
    summa_string_free(src);
}

void test_string_copy_shrinks_length_but_keeps_capacity() {
    SummaString dest = summa_string_make(HELLO_WORLD);
    size_t      cap  = dest->capacity;
    SummaString src  = summa_string_make("hi");
    summa_string_copy(dest, src);
    SUMMA_TEST_ASSERT_EQ_STR("hi", dest->value);
    SUMMA_TEST_ASSERT_EQ(2u, dest->length);
    /* Destination buffer already had enough room, so no realloc/shrink
     * happens; capacity is left as-is even though it now exceeds length + 1. */
    SUMMA_TEST_ASSERT_EQ(cap, dest->capacity);
    SUMMA_TEST_ASSERT_EQ('\0', dest->value[dest->length]);
    summa_string_free(dest);
    summa_string_free(src);
}

void test_string_copy_into_empty_destination() {
    SummaString dest = summa_string_make_empty();
    SummaString src  = summa_string_make(HELLO_WORLD);
    summa_string_copy(dest, src);
    SUMMA_TEST_ASSERT_EQ_STR(HELLO_WORLD, dest->value);
    SUMMA_TEST_ASSERT_EQ(strlen(HELLO_WORLD), dest->length);
    SUMMA_TEST_ASSERT(dest->capacity > dest->length);
    summa_string_free(dest);
    summa_string_free(src);
}

void test_string_copy_empty_source() {
    SummaString dest = summa_string_make(HELLO_WORLD);
    SummaString src  = summa_string_make("");
    summa_string_copy(dest, src);
    SUMMA_TEST_ASSERT_EQ_STR("", dest->value);
    SUMMA_TEST_ASSERT_EQ(0u, dest->length);
    summa_string_free(dest);
    summa_string_free(src);
}

void test_string_copy_cstr_grows_destination() {
    SummaString dest = summa_string_make("x");
    summa_string_copy_cstr(dest, HELLO_WORLD);
    SUMMA_TEST_ASSERT_EQ_STR(HELLO_WORLD, dest->value);
    SUMMA_TEST_ASSERT_EQ(strlen(HELLO_WORLD), dest->length);
    SUMMA_TEST_ASSERT(dest->capacity > dest->length);
    SUMMA_TEST_ASSERT_EQ('\0', dest->value[dest->length]);
    summa_string_free(dest);
}

void test_string_copy_cstr_shrinks_length_but_keeps_capacity() {
    SummaString dest = summa_string_make(HELLO_WORLD);
    size_t      cap  = dest->capacity;
    summa_string_copy_cstr(dest, "hi");
    SUMMA_TEST_ASSERT_EQ_STR("hi", dest->value);
    SUMMA_TEST_ASSERT_EQ(2u, dest->length);
    SUMMA_TEST_ASSERT_EQ(cap, dest->capacity);
    SUMMA_TEST_ASSERT_EQ('\0', dest->value[dest->length]);
    summa_string_free(dest);
}

void test_string_copy_cstr_into_empty_destination() {
    SummaString dest = summa_string_make_empty();
    summa_string_copy_cstr(dest, HELLO_WORLD);
    SUMMA_TEST_ASSERT_EQ_STR(HELLO_WORLD, dest->value);
    SUMMA_TEST_ASSERT_EQ(strlen(HELLO_WORLD), dest->length);
    SUMMA_TEST_ASSERT(dest->capacity > dest->length);
    summa_string_free(dest);
}

void test_string_copy_cstr_empty_string() {
    SummaString dest = summa_string_make(HELLO_WORLD);
    summa_string_copy_cstr(dest, "");
    SUMMA_TEST_ASSERT_EQ_STR("", dest->value);
    SUMMA_TEST_ASSERT_EQ(0u, dest->length);
    SUMMA_TEST_ASSERT_EQ('\0', dest->value[0]);
    summa_string_free(dest);
}

int main(void) {
    summa_test_begin("scheme.string");
    SUMMA_TEST_RUN(test_string_make);
    SUMMA_TEST_RUN(test_string_make_empty_cstr);
    SUMMA_TEST_RUN(test_string_make_empty);
    SUMMA_TEST_RUN(test_string_clear);
    SUMMA_TEST_RUN(test_string_copy_grows_destination);
    SUMMA_TEST_RUN(test_string_copy_shrinks_length_but_keeps_capacity);
    SUMMA_TEST_RUN(test_string_copy_into_empty_destination);
    SUMMA_TEST_RUN(test_string_copy_empty_source);
    SUMMA_TEST_RUN(test_string_copy_cstr_grows_destination);
    SUMMA_TEST_RUN(test_string_copy_cstr_shrinks_length_but_keeps_capacity);
    SUMMA_TEST_RUN(test_string_copy_cstr_into_empty_destination);
    SUMMA_TEST_RUN(test_string_copy_cstr_empty_string);
    return summa_test_end();
}
