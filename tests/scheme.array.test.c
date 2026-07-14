#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#include <stdint.h>
#include <stdlib.h>

void test_array_make() {
    int        vals[3] = {1, 2, 3};
    SummaArray array   = summa_array_make(vals, 3, sizeof(int));
    SUMMA_TEST_ASSERT_NOT_NULL(array);
    SUMMA_TEST_ASSERT_EQ(3u, array->length);
    SUMMA_TEST_ASSERT_EQ(3u, array->capacity);
    SUMMA_TEST_ASSERT_EQ(sizeof(int), array->element_size);
    int* out = (int*)array->value;
    SUMMA_TEST_ASSERT_EQ(1, out[0]);
    SUMMA_TEST_ASSERT_EQ(2, out[1]);
    SUMMA_TEST_ASSERT_EQ(3, out[2]);
    summa_array_free(array);
}

void test_array_make_zero_elements() {
    int        vals[1] = {0};
    SummaArray array   = summa_array_make(vals, 0, sizeof(int));
    SUMMA_TEST_ASSERT_NOT_NULL(array);
    SUMMA_TEST_ASSERT_EQ(0u, array->length);
    SUMMA_TEST_ASSERT_EQ(0u, array->capacity);
    summa_array_free(array);
}

void test_array_make_empty() {
    SummaArray array = summa_array_make_empty(sizeof(double));
    SUMMA_TEST_ASSERT_NOT_NULL(array);
    SUMMA_TEST_ASSERT_EQ(0u, array->length);
    SUMMA_TEST_ASSERT_EQ(SUMMA_ARRAY_DEFAULT_CAPACITY, array->capacity);
    SUMMA_TEST_ASSERT_EQ(sizeof(double), array->element_size);
    /* Nothing has been allocated yet; value must not be a dangling/garbage
     * pointer, since a later copy into this array will realloc() it. */
    SUMMA_TEST_ASSERT_NOT_NULL(array->value);
    summa_array_free(array);
}

void test_array_clear() {
    int        vals[4] = {1, 2, 3, 4};
    SummaArray array   = summa_array_make(vals, 4, sizeof(int));
    size_t     cap     = array->capacity;
    summa_array_clear(array);
    SUMMA_TEST_ASSERT_EQ(0u, array->length);
    /* Clearing is a logical reset, not a dealloc: capacity is untouched so the
     * backing buffer can be reused without a fresh allocation. */
    SUMMA_TEST_ASSERT_EQ(cap, array->capacity);
    summa_array_free(array);
}

void test_array_copy_mismatched_element_size_fails() {
    int        ints[2]    = {1, 2};
    double     doubles[2] = {1.0, 2.0};
    SummaArray dest       = summa_array_make(ints, 2, sizeof(int));
    SummaArray src        = summa_array_make(doubles, 2, sizeof(double));
    bool       ok         = summa_array_copy(dest, src);
    SUMMA_TEST_ASSERT(!ok);
    summa_array_free(dest);
    summa_array_free(src);
}

void test_array_copy_grows_destination() {
    int        small[1] = {7};
    int        big[5]   = {1, 2, 3, 4, 5};
    SummaArray dest     = summa_array_make(small, 1, sizeof(int));
    SummaArray src      = summa_array_make(big, 5, sizeof(int));
    bool       ok       = summa_array_copy(dest, src);
    SUMMA_TEST_ASSERT(ok);
    SUMMA_TEST_ASSERT_EQ(5u, dest->length);
    SUMMA_TEST_ASSERT(dest->capacity >= dest->length);
    int* out = (int*)dest->value;
    for (int i = 0; i < 5; i++) {
        SUMMA_TEST_ASSERT_EQ(big[i], out[i]);
    }
    summa_array_free(dest);
    summa_array_free(src);
}

void test_array_copy_shrinks_destination_length_but_keeps_capacity() {
    int        small[1] = {7};
    int        big[5]   = {1, 2, 3, 4, 5};
    SummaArray dest     = summa_array_make(big, 5, sizeof(int));
    size_t     cap      = dest->capacity;
    SummaArray src      = summa_array_make(small, 1, sizeof(int));
    bool       ok       = summa_array_copy(dest, src);
    SUMMA_TEST_ASSERT(ok);
    SUMMA_TEST_ASSERT_EQ(1u, dest->length);
    /* Destination buffer already had enough room, so no realloc/shrink
     * happens; capacity is left as-is. */
    SUMMA_TEST_ASSERT_EQ(cap, dest->capacity);
    int* out = (int*)dest->value;
    SUMMA_TEST_ASSERT_EQ(7, out[0]);
    summa_array_free(dest);
    summa_array_free(src);
}

void test_array_copy_into_empty_destination() {
    int        vals[3] = {9, 8, 7};
    SummaArray dest    = summa_array_make_empty(sizeof(int));
    SummaArray src     = summa_array_make(vals, 3, sizeof(int));
    bool       ok      = summa_array_copy(dest, src);
    SUMMA_TEST_ASSERT(ok);
    SUMMA_TEST_ASSERT_EQ(3u, dest->length);
    SUMMA_TEST_ASSERT(dest->capacity >= 3u);
    int* out = (int*)dest->value;
    SUMMA_TEST_ASSERT_EQ(9, out[0]);
    SUMMA_TEST_ASSERT_EQ(8, out[1]);
    SUMMA_TEST_ASSERT_EQ(7, out[2]);
    summa_array_free(dest);
    summa_array_free(src);
}

void test_array_copy_raw_into_empty_destination() {
    int        vals[4] = {10, 20, 30, 40};
    SummaArray dest    = summa_array_make_empty(sizeof(int));
    bool       ok      = summa_array_copy_raw(dest, vals, 4);
    SUMMA_TEST_ASSERT(ok);
    SUMMA_TEST_ASSERT_EQ(4u, dest->length);
    SUMMA_TEST_ASSERT(dest->capacity >= 4u);
    int* out = (int*)dest->value;
    for (int i = 0; i < 4; i++) {
        SUMMA_TEST_ASSERT_EQ(vals[i], out[i]);
    }
    summa_array_free(dest);
}

void test_array_copy_raw_zero_length() {
    int        vals[2] = {1, 2};
    SummaArray dest    = summa_array_make(vals, 2, sizeof(int));
    bool       ok      = summa_array_copy_raw(dest, vals, 0);
    SUMMA_TEST_ASSERT(ok);
    SUMMA_TEST_ASSERT_EQ(0u, dest->length);
    summa_array_free(dest);
}

int main(void) {
    summa_test_begin("scheme.array");
    SUMMA_TEST_RUN(test_array_make);
    SUMMA_TEST_RUN(test_array_make_zero_elements);
    SUMMA_TEST_RUN(test_array_make_empty);
    SUMMA_TEST_RUN(test_array_clear);
    SUMMA_TEST_RUN(test_array_copy_mismatched_element_size_fails);
    SUMMA_TEST_RUN(test_array_copy_grows_destination);
    SUMMA_TEST_RUN(test_array_copy_shrinks_destination_length_but_keeps_capacity);
    SUMMA_TEST_RUN(test_array_copy_into_empty_destination);
    SUMMA_TEST_RUN(test_array_copy_raw_into_empty_destination);
    SUMMA_TEST_RUN(test_array_copy_raw_zero_length);
    return summa_test_end();
}
