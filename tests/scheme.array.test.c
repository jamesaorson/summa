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
    int* out = (int*)array->elements;
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
    SUMMA_TEST_ASSERT_NOT_NULL(array->elements);
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
    int* out = (int*)dest->elements;
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
    int* out = (int*)dest->elements;
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
    int* out = (int*)dest->elements;
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
    int* out = (int*)dest->elements;
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

void test_array_push_onto_empty() {
    SummaArray array = summa_array_make_empty(sizeof(int));
    int        val   = 42;
    summa_array_push(array, (void**)(&val));
    SUMMA_TEST_ASSERT_EQ(1u, array->length);
    SUMMA_TEST_ASSERT(array->capacity >= array->length);
    int* out = (int*)array->elements;
    SUMMA_TEST_ASSERT_EQ(42, out[0]);
    summa_array_free(array);
}

void test_array_push_onto_zero_capacity() {
    /* summa_array_make with 0 elements yields capacity == 0, a distinct
     * zero-capacity path from summa_array_make_empty(). */
    int        vals[1] = {0};
    SummaArray array   = summa_array_make(vals, 0, sizeof(int));
    int        val     = 7;
    summa_array_push(array, &val);
    SUMMA_TEST_ASSERT_EQ(1u, array->length);
    SUMMA_TEST_ASSERT(array->capacity >= array->length);
    int* out = (int*)array->elements;
    SUMMA_TEST_ASSERT_EQ(7, out[0]);
    summa_array_free(array);
}

void test_array_push_appends_in_order() {
    SummaArray array = summa_array_make_empty(sizeof(int));
    for (int i = 0; i < 5; i++) {
        summa_array_push(array, &i);
    }
    SUMMA_TEST_ASSERT_EQ(5u, array->length);
    int* out = (int*)array->elements;
    for (int i = 0; i < 5; i++) {
        SUMMA_TEST_ASSERT_EQ(i, out[i]);
    }
    summa_array_free(array);
}

void test_array_push_grows_past_default_capacity() {
    SummaArray array       = summa_array_make_empty(sizeof(int));
    size_t     initial_cap = array->capacity;
    int        num_to_push = 20;
    for (int i = 0; i < num_to_push; i++) {
        summa_array_push(array, &i);
    }
    SUMMA_TEST_ASSERT_EQ((size_t)num_to_push, array->length);
    /* Capacity must have actually grown to fit everything pushed, and stay
     * in sync with the buffer that was really allocated. */
    SUMMA_TEST_ASSERT(array->capacity >= array->length);
    SUMMA_TEST_ASSERT(array->capacity > initial_cap);
    int* out = (int*)array->elements;
    for (int i = 0; i < num_to_push; i++) {
        SUMMA_TEST_ASSERT_EQ(i, out[i]);
    }
    summa_array_free(array);
}

void test_array_push_preserves_existing_elements_on_growth() {
    int        vals[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    SummaArray array   = summa_array_make(vals, 8, sizeof(int));
    /* length == capacity here, so the next push must trigger a growth
     * realloc without losing the elements already in the buffer. */
    SUMMA_TEST_ASSERT_EQ(array->capacity, array->length);
    int val = 9;
    summa_array_push(array, &val);
    SUMMA_TEST_ASSERT_EQ(9u, array->length);
    SUMMA_TEST_ASSERT(array->capacity >= 9u);
    int* out = (int*)array->elements;
    for (int i = 0; i < 9; i++) {
        SUMMA_TEST_ASSERT_EQ(i + 1, out[i]);
    }
    summa_array_free(array);
}

void test_array_push_copies_by_value() {
    SummaArray array = summa_array_make_empty(sizeof(int));
    int        val   = 100;
    summa_array_push(array, &val);
    /* Mutating the source after push must not affect the stored element;
     * push copies element_size bytes rather than storing the pointer. */
    val      = 999;
    int* out = (int*)array->elements;
    SUMMA_TEST_ASSERT_EQ(100, out[0]);
    summa_array_free(array);
}

typedef struct {
    int a;
    int b;
    int c;
    int d;
} test_push_wide_t;

void test_array_push_element_wider_than_pointer() {
    /* element_size (16 bytes) is larger than sizeof(void*), which would
     * misbehave under pointer-slot-per-element storage. */
    SummaArray       array  = summa_array_make_empty(sizeof(test_push_wide_t));
    test_push_wide_t first  = {1, 2, 3, 4};
    test_push_wide_t second = {5, 6, 7, 8};
    summa_array_push(array, &first);
    summa_array_push(array, &second);
    SUMMA_TEST_ASSERT_EQ(2u, array->length);
    test_push_wide_t* out = (test_push_wide_t*)array->elements;
    SUMMA_TEST_ASSERT_EQ(1, out[0].a);
    SUMMA_TEST_ASSERT_EQ(4, out[0].d);
    SUMMA_TEST_ASSERT_EQ(5, out[1].a);
    SUMMA_TEST_ASSERT_EQ(8, out[1].d);
    summa_array_free(array);
}

void test_array_contains_found() {
    int        vals[4] = {1, 2, 3, 4};
    SummaArray array   = summa_array_make(vals, 4, sizeof(int));
    int        needle  = 3;
    SUMMA_TEST_ASSERT(summa_array_contains(array, &needle));
    summa_array_free(array);
}

void test_array_contains_not_found() {
    int        vals[4] = {1, 2, 3, 4};
    SummaArray array   = summa_array_make(vals, 4, sizeof(int));
    int        needle  = 5;
    SUMMA_TEST_ASSERT(!summa_array_contains(array, &needle));
    summa_array_free(array);
}

void test_array_contains_empty_array() {
    SummaArray array  = summa_array_make_empty(sizeof(int));
    int        needle = 1;
    SUMMA_TEST_ASSERT(!summa_array_contains(array, &needle));
    summa_array_free(array);
}

void test_array_contains_checks_first_and_last_elements() {
    int        vals[3] = {10, 20, 30};
    SummaArray array   = summa_array_make(vals, 3, sizeof(int));
    int        first   = 10;
    int        last    = 30;
    SUMMA_TEST_ASSERT(summa_array_contains(array, &first));
    SUMMA_TEST_ASSERT(summa_array_contains(array, &last));
    summa_array_free(array);
}

void test_array_contains_ignores_elements_past_length() {
    /* length == 1 but capacity holds a second slot with a value that was
     * never actually pushed/copied in; contains must only scan [0, length). */
    SummaArray array  = summa_array_make_empty(sizeof(int));
    int        pushed = 1;
    int        never  = 2;
    summa_array_push(array, &pushed);
    summa_array_push(array, &never);
    summa_array_clear(array);
    summa_array_push(array, &pushed);
    SUMMA_TEST_ASSERT_EQ(1u, array->length);
    SUMMA_TEST_ASSERT(!summa_array_contains(array, &never));
    summa_array_free(array);
}

void test_array_contains_element_wider_than_pointer() {
    SummaArray       array   = summa_array_make_empty(sizeof(test_push_wide_t));
    test_push_wide_t first   = {1, 2, 3, 4};
    test_push_wide_t second  = {5, 6, 7, 8};
    test_push_wide_t missing = {9, 9, 9, 9};
    summa_array_push(array, &first);
    summa_array_push(array, &second);
    SUMMA_TEST_ASSERT(summa_array_contains(array, &first));
    SUMMA_TEST_ASSERT(summa_array_contains(array, &second));
    SUMMA_TEST_ASSERT(!summa_array_contains(array, &missing));
    summa_array_free(array);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.array", argc, argv);
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
    SUMMA_TEST_RUN(test_array_push_onto_empty);
    SUMMA_TEST_RUN(test_array_push_onto_zero_capacity);
    SUMMA_TEST_RUN(test_array_push_appends_in_order);
    SUMMA_TEST_RUN(test_array_push_grows_past_default_capacity);
    SUMMA_TEST_RUN(test_array_push_preserves_existing_elements_on_growth);
    SUMMA_TEST_RUN(test_array_push_copies_by_value);
    SUMMA_TEST_RUN(test_array_push_element_wider_than_pointer);
    SUMMA_TEST_RUN(test_array_contains_found);
    SUMMA_TEST_RUN(test_array_contains_not_found);
    SUMMA_TEST_RUN(test_array_contains_empty_array);
    SUMMA_TEST_RUN(test_array_contains_checks_first_and_last_elements);
    SUMMA_TEST_RUN(test_array_contains_ignores_elements_past_length);
    SUMMA_TEST_RUN(test_array_contains_element_wider_than_pointer);
    return summa_test_end();
}
