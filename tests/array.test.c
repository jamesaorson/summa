#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_ARRAY_IMPLEMENTATION
#include <summa/array/array.h>

#include <stdint.h>
#include <stdlib.h>

#define SCOPED_ARRAY(var, init) SUMMA_TEST_SCOPED_VALUE(SummaArray, var, init, summa_array_free)

void test_array_make() {
    int vals[3] = {1, 2, 3};
    SCOPED_ARRAY(array, summa_array_make(vals, 3, sizeof(int))) {
        SUMMA_TEST_ASSERT_NOT_NULL(array);
        SUMMA_TEST_ASSERT_EQ(3u, array->length);
        SUMMA_TEST_ASSERT_EQ(3u, array->capacity);
        SUMMA_TEST_ASSERT_EQ(sizeof(int), array->element_size);
        int* out = (int*)array->elements;
        SUMMA_TEST_ASSERT_EQ(1, out[0]);
        SUMMA_TEST_ASSERT_EQ(2, out[1]);
        SUMMA_TEST_ASSERT_EQ(3, out[2]);
    }
}

void test_array_make_zero_elements() {
    int vals[1] = {0};
    SCOPED_ARRAY(array, summa_array_make(vals, 0, sizeof(int))) {
        SUMMA_TEST_ASSERT_NOT_NULL(array);
        SUMMA_TEST_ASSERT_EQ(0u, array->length);
        SUMMA_TEST_ASSERT_EQ(0u, array->capacity);
    }
}

void test_array_make_empty() {
    SCOPED_ARRAY(array, summa_array_make_empty(sizeof(double))) {
        SUMMA_TEST_ASSERT_NOT_NULL(array);
        SUMMA_TEST_ASSERT_EQ(0u, array->length);
        SUMMA_TEST_ASSERT_EQ(SUMMA_ARRAY_DEFAULT_CAPACITY, array->capacity);
        SUMMA_TEST_ASSERT_EQ(sizeof(double), array->element_size);
        /* Nothing has been allocated yet; value must not be a dangling/garbage
         * pointer, since a later copy into this array will realloc() it. */
        SUMMA_TEST_ASSERT_NOT_NULL(array->elements);
    }
}

void test_array_make_with_capacity() {
    SCOPED_ARRAY(array, summa_array_make_with_capacity(sizeof(int), 3)) {
        SUMMA_TEST_ASSERT_NOT_NULL(array);
        SUMMA_TEST_ASSERT_EQ(0u, array->length);
        SUMMA_TEST_ASSERT_EQ(3u, array->capacity);
        SUMMA_TEST_ASSERT_EQ(sizeof(int), array->element_size);
        SUMMA_TEST_ASSERT_NOT_NULL(array->elements);
    }
}

void test_array_make_with_zero_capacity_allocates_no_elements() {
    /* The point of the entry point: a caller that knows it wants nothing pays
     * for nothing. Freeing this must still work, and free(nullptr) is why. */
    SCOPED_ARRAY(array, summa_array_make_with_capacity(sizeof(int), 0)) {
        SUMMA_TEST_ASSERT_NOT_NULL(array);
        SUMMA_TEST_ASSERT_EQ(0u, array->length);
        SUMMA_TEST_ASSERT_EQ(0u, array->capacity);
        SUMMA_TEST_ASSERT_NULL(array->elements);
    }
}

void test_array_make_with_zero_capacity_still_grows() {
    SCOPED_ARRAY(array, summa_array_make_with_capacity(sizeof(int), 0)) {
        for (int i = 0; i < 20; i++) {
            summa_array_push(array, &i);
        }
        SUMMA_TEST_ASSERT_EQ(20u, array->length);
        SUMMA_TEST_ASSERT(array->capacity >= 20u);
        const int* out = (const int*)array->elements;
        for (int i = 0; i < 20; i++) {
            SUMMA_TEST_ASSERT_EQ(i, out[i]);
        }
    }
}

void test_array_make_with_exact_capacity_still_grows_past_it() {
    /* An array sized to exactly what its caller asked for is full after that
     * many pushes. The next one has to grow, and must not lose anything. */
    SCOPED_ARRAY(array, summa_array_make_with_capacity(sizeof(int), 3)) {
        for (int i = 0; i < 3; i++) {
            summa_array_push(array, &i);
        }
        SUMMA_TEST_ASSERT_EQ(3u, array->capacity);
        int extra = 99;
        summa_array_push(array, &extra);
        SUMMA_TEST_ASSERT_EQ(4u, array->length);
        SUMMA_TEST_ASSERT(array->capacity >= 4u);
        const int* out = (const int*)array->elements;
        SUMMA_TEST_ASSERT_EQ(0, out[0]);
        SUMMA_TEST_ASSERT_EQ(1, out[1]);
        SUMMA_TEST_ASSERT_EQ(2, out[2]);
        SUMMA_TEST_ASSERT_EQ(99, out[3]);
    }
}

/* `init`/`dispose` are `make`/`free` for a header the caller already has, so
 * these cases deliberately use a stack header and never call `summa_array_free`
 * on it -- which is the whole distinction being pinned. */
void test_array_init_with_capacity_uses_the_callers_header() {
    SummaArray_t array;
    summa_array_init_with_capacity(&array, sizeof(int), 3);
    SUMMA_TEST_ASSERT_EQ(0u, array.length);
    SUMMA_TEST_ASSERT_EQ(3u, array.capacity);
    SUMMA_TEST_ASSERT_EQ(sizeof(int), array.element_size);
    SUMMA_TEST_ASSERT_NOT_NULL(array.elements);
    summa_array_dispose(&array);
}

void test_array_init_with_zero_capacity_allocates_nothing() {
    SummaArray_t array;
    summa_array_init_with_capacity(&array, sizeof(int), 0);
    SUMMA_TEST_ASSERT_NULL(array.elements);
    summa_array_dispose(&array);
}

void test_array_init_then_grow_behaves_like_any_other_array() {
    /* The point: nothing downstream can tell where the header came from.
     * `push` reallocs the element storage exactly as it always did. */
    SummaArray_t array;
    summa_array_init_with_capacity(&array, sizeof(int), 2);
    for (int i = 0; i < 20; i++) {
        summa_array_push(&array, &i);
    }
    SUMMA_TEST_ASSERT_EQ(20u, array.length);
    SUMMA_TEST_ASSERT(array.capacity >= 20u);
    const int* out = (const int*)array.elements;
    for (int i = 0; i < 20; i++) {
        SUMMA_TEST_ASSERT_EQ(i, out[i]);
    }
    summa_array_dispose(&array);
}

void test_array_dispose_leaves_an_empty_array_behind() {
    /* Emptied rather than merely freed, so a header outliving its contents
     * reads as a zero-length array instead of a dangling pointer. An owner
     * being torn down around it can go on enumerating it safely. */
    int          vals[3] = {1, 2, 3};
    SummaArray_t array;
    summa_array_init_with_capacity(&array, sizeof(int), 3);
    for (int i = 0; i < 3; i++) {
        summa_array_push(&array, &vals[i]);
    }
    summa_array_dispose(&array);
    SUMMA_TEST_ASSERT_NULL(array.elements);
    SUMMA_TEST_ASSERT_EQ(0u, array.length);
    SUMMA_TEST_ASSERT_EQ(0u, array.capacity);
    /* And still usable: capacity 0 is where push already starts. */
    summa_array_push(&array, &vals[0]);
    SUMMA_TEST_ASSERT_EQ(1u, array.length);
    summa_array_dispose(&array);
}

void test_array_reserve_grows_from_zero_capacity() {
    SCOPED_ARRAY(array, summa_array_make_with_capacity(sizeof(int), 0)) {
        summa_array_reserve(array, 5);
        SUMMA_TEST_ASSERT_EQ(5u, array->capacity);
        SUMMA_TEST_ASSERT_EQ(0u, array->length);
        SUMMA_TEST_ASSERT_NOT_NULL(array->elements);
    }
}

void test_array_reserve_keeps_existing_elements() {
    int vals[3] = {4, 5, 6};
    SCOPED_ARRAY(array, summa_array_make(vals, 3, sizeof(int))) {
        summa_array_reserve(array, 32);
        SUMMA_TEST_ASSERT_EQ(32u, array->capacity);
        SUMMA_TEST_ASSERT_EQ(3u, array->length);
        const int* out = (const int*)array->elements;
        SUMMA_TEST_ASSERT_EQ(4, out[0]);
        SUMMA_TEST_ASSERT_EQ(5, out[1]);
        SUMMA_TEST_ASSERT_EQ(6, out[2]);
    }
}

void test_array_reserve_never_shrinks() {
    SCOPED_ARRAY(array, summa_array_make_empty(sizeof(int))) {
        const size_t before = array->capacity;
        summa_array_reserve(array, 1);
        SUMMA_TEST_ASSERT_EQ(before, array->capacity);
        summa_array_reserve(array, 0);
        SUMMA_TEST_ASSERT_EQ(before, array->capacity);
    }
}

void test_array_copy_into_zero_capacity_destination() {
    /* dest->elements is null here, which is the one thing a copy must not
     * hand to memcpy unguarded. */
    int vals[2] = {11, 12};
    SCOPED_ARRAY(dest, summa_array_make_with_capacity(sizeof(int), 0))
    SCOPED_ARRAY(src, summa_array_make(vals, 2, sizeof(int))) {
        SUMMA_TEST_ASSERT(summa_array_copy(dest, src));
        SUMMA_TEST_ASSERT_EQ(2u, dest->length);
        const int* out = (const int*)dest->elements;
        SUMMA_TEST_ASSERT_EQ(11, out[0]);
        SUMMA_TEST_ASSERT_EQ(12, out[1]);
    }
}

void test_array_copy_empty_into_zero_capacity_destination() {
    /* Both sides empty: nothing is copied, and nothing is dereferenced. */
    int vals[1] = {0};
    SCOPED_ARRAY(dest, summa_array_make_with_capacity(sizeof(int), 0))
    SCOPED_ARRAY(src, summa_array_make(vals, 0, sizeof(int))) {
        SUMMA_TEST_ASSERT(summa_array_copy(dest, src));
        SUMMA_TEST_ASSERT_EQ(0u, dest->length);
        SUMMA_TEST_ASSERT_EQ(0u, dest->capacity);
        SUMMA_TEST_ASSERT_NULL(dest->elements);
    }
}

void test_array_clear() {
    int vals[4] = {1, 2, 3, 4};
    SCOPED_ARRAY(array, summa_array_make(vals, 4, sizeof(int))) {
        size_t cap = array->capacity;
        summa_array_clear(array);
        SUMMA_TEST_ASSERT_EQ(0u, array->length);
        /* Clearing is a logical reset, not a dealloc: capacity is untouched so the
         * backing buffer can be reused without a fresh allocation. */
        SUMMA_TEST_ASSERT_EQ(cap, array->capacity);
    }
}

void test_array_copy_mismatched_element_size_fails() {
    int    ints[2]    = {1, 2};
    double doubles[2] = {1.0, 2.0};
    SCOPED_ARRAY(dest, summa_array_make(ints, 2, sizeof(int)))
    SCOPED_ARRAY(src, summa_array_make(doubles, 2, sizeof(double))) {
        bool ok = summa_array_copy(dest, src);
        SUMMA_TEST_ASSERT(!ok);
    }
}

void test_array_copy_grows_destination() {
    int small[1] = {7};
    int big[5]   = {1, 2, 3, 4, 5};
    SCOPED_ARRAY(dest, summa_array_make(small, 1, sizeof(int)))
    SCOPED_ARRAY(src, summa_array_make(big, 5, sizeof(int))) {
        bool ok = summa_array_copy(dest, src);
        SUMMA_TEST_ASSERT(ok);
        SUMMA_TEST_ASSERT_EQ(5u, dest->length);
        SUMMA_TEST_ASSERT(dest->capacity >= dest->length);
        int* out = (int*)dest->elements;
        for (int i = 0; i < 5; i++) {
            SUMMA_TEST_ASSERT_EQ(big[i], out[i]);
        }
    }
}

void test_array_copy_shrinks_destination_length_but_keeps_capacity() {
    int small[1] = {7};
    int big[5]   = {1, 2, 3, 4, 5};
    SCOPED_ARRAY(dest, summa_array_make(big, 5, sizeof(int)))
    SCOPED_ARRAY(src, summa_array_make(small, 1, sizeof(int))) {
        size_t cap = dest->capacity;
        bool   ok  = summa_array_copy(dest, src);
        SUMMA_TEST_ASSERT(ok);
        SUMMA_TEST_ASSERT_EQ(1u, dest->length);
        /* Destination buffer already had enough room, so no realloc/shrink
         * happens; capacity is left as-is. */
        SUMMA_TEST_ASSERT_EQ(cap, dest->capacity);
        int* out = (int*)dest->elements;
        SUMMA_TEST_ASSERT_EQ(7, out[0]);
    }
}

void test_array_copy_into_empty_destination() {
    int vals[3] = {9, 8, 7};
    SCOPED_ARRAY(dest, summa_array_make_empty(sizeof(int)))
    SCOPED_ARRAY(src, summa_array_make(vals, 3, sizeof(int))) {
        bool ok = summa_array_copy(dest, src);
        SUMMA_TEST_ASSERT(ok);
        SUMMA_TEST_ASSERT_EQ(3u, dest->length);
        SUMMA_TEST_ASSERT(dest->capacity >= 3u);
        int* out = (int*)dest->elements;
        SUMMA_TEST_ASSERT_EQ(9, out[0]);
        SUMMA_TEST_ASSERT_EQ(8, out[1]);
        SUMMA_TEST_ASSERT_EQ(7, out[2]);
    }
}

void test_array_copy_raw_into_empty_destination() {
    int vals[4] = {10, 20, 30, 40};
    SCOPED_ARRAY(dest, summa_array_make_empty(sizeof(int))) {
        bool ok = summa_array_copy_raw(dest, vals, 4);
        SUMMA_TEST_ASSERT(ok);
        SUMMA_TEST_ASSERT_EQ(4u, dest->length);
        SUMMA_TEST_ASSERT(dest->capacity >= 4u);
        int* out = (int*)dest->elements;
        for (int i = 0; i < 4; i++) {
            SUMMA_TEST_ASSERT_EQ(vals[i], out[i]);
        }
    }
}

void test_array_copy_raw_zero_length() {
    int vals[2] = {1, 2};
    SCOPED_ARRAY(dest, summa_array_make(vals, 2, sizeof(int))) {
        bool ok = summa_array_copy_raw(dest, vals, 0);
        SUMMA_TEST_ASSERT(ok);
        SUMMA_TEST_ASSERT_EQ(0u, dest->length);
    }
}

void test_array_push_onto_empty() {
    SCOPED_ARRAY(array, summa_array_make_empty(sizeof(int))) {
        int val = 42;
        summa_array_push(array, (void**)(&val));
        SUMMA_TEST_ASSERT_EQ(1u, array->length);
        SUMMA_TEST_ASSERT(array->capacity >= array->length);
        int* out = (int*)array->elements;
        SUMMA_TEST_ASSERT_EQ(42, out[0]);
    }
}

void test_array_push_onto_zero_capacity() {
    /* summa_array_make with 0 elements yields capacity == 0, a distinct
     * zero-capacity path from summa_array_make_empty(). */
    int vals[1] = {0};
    SCOPED_ARRAY(array, summa_array_make(vals, 0, sizeof(int))) {
        int val = 7;
        summa_array_push(array, &val);
        SUMMA_TEST_ASSERT_EQ(1u, array->length);
        SUMMA_TEST_ASSERT(array->capacity >= array->length);
        int* out = (int*)array->elements;
        SUMMA_TEST_ASSERT_EQ(7, out[0]);
    }
}

void test_array_push_appends_in_order() {
    SCOPED_ARRAY(array, summa_array_make_empty(sizeof(int))) {
        for (int i = 0; i < 5; i++) {
            summa_array_push(array, &i);
        }
        SUMMA_TEST_ASSERT_EQ(5u, array->length);
        int* out = (int*)array->elements;
        for (int i = 0; i < 5; i++) {
            SUMMA_TEST_ASSERT_EQ(i, out[i]);
        }
    }
}

void test_array_push_grows_past_default_capacity() {
    SCOPED_ARRAY(array, summa_array_make_empty(sizeof(int))) {
        size_t initial_cap = array->capacity;
        int    num_to_push = 20;
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
    }
}

void test_array_push_preserves_existing_elements_on_growth() {
    int vals[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    SCOPED_ARRAY(array, summa_array_make(vals, 8, sizeof(int))) {
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
    }
}

void test_array_push_copies_by_value() {
    SCOPED_ARRAY(array, summa_array_make_empty(sizeof(int))) {
        int val = 100;
        summa_array_push(array, &val);
        /* Mutating the source after push must not affect the stored element;
         * push copies element_size bytes rather than storing the pointer. */
        val      = 999;
        int* out = (int*)array->elements;
        SUMMA_TEST_ASSERT_EQ(100, out[0]);
    }
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
    SCOPED_ARRAY(array, summa_array_make_empty(sizeof(test_push_wide_t))) {
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
    }
}

void test_array_contains_found() {
    int vals[4] = {1, 2, 3, 4};
    SCOPED_ARRAY(array, summa_array_make(vals, 4, sizeof(int))) {
        int needle = 3;
        SUMMA_TEST_ASSERT(summa_array_contains(array, &needle));
    }
}

void test_array_contains_not_found() {
    int vals[4] = {1, 2, 3, 4};
    SCOPED_ARRAY(array, summa_array_make(vals, 4, sizeof(int))) {
        int needle = 5;
        SUMMA_TEST_ASSERT(!summa_array_contains(array, &needle));
    }
}

void test_array_contains_empty_array() {
    SCOPED_ARRAY(array, summa_array_make_empty(sizeof(int))) {
        int needle = 1;
        SUMMA_TEST_ASSERT(!summa_array_contains(array, &needle));
    }
}

void test_array_contains_checks_first_and_last_elements() {
    int vals[3] = {10, 20, 30};
    SCOPED_ARRAY(array, summa_array_make(vals, 3, sizeof(int))) {
        int first = 10;
        int last  = 30;
        SUMMA_TEST_ASSERT(summa_array_contains(array, &first));
        SUMMA_TEST_ASSERT(summa_array_contains(array, &last));
    }
}

void test_array_contains_ignores_elements_past_length() {
    /* length == 1 but capacity holds a second slot with a value that was
     * never actually pushed/copied in; contains must only scan [0, length). */
    SCOPED_ARRAY(array, summa_array_make_empty(sizeof(int))) {
        int pushed = 1;
        int never  = 2;
        summa_array_push(array, &pushed);
        summa_array_push(array, &never);
        summa_array_clear(array);
        summa_array_push(array, &pushed);
        SUMMA_TEST_ASSERT_EQ(1u, array->length);
        SUMMA_TEST_ASSERT(!summa_array_contains(array, &never));
    }
}

void test_array_contains_element_wider_than_pointer() {
    SCOPED_ARRAY(array, summa_array_make_empty(sizeof(test_push_wide_t))) {
        test_push_wide_t first   = {1, 2, 3, 4};
        test_push_wide_t second  = {5, 6, 7, 8};
        test_push_wide_t missing = {9, 9, 9, 9};
        summa_array_push(array, &first);
        summa_array_push(array, &second);
        SUMMA_TEST_ASSERT(summa_array_contains(array, &first));
        SUMMA_TEST_ASSERT(summa_array_contains(array, &second));
        SUMMA_TEST_ASSERT(!summa_array_contains(array, &missing));
    }
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.array", argc, argv);
    SUMMA_TEST_RUN(test_array_make);
    SUMMA_TEST_RUN(test_array_make_zero_elements);
    SUMMA_TEST_RUN(test_array_make_empty);
    SUMMA_TEST_RUN(test_array_make_with_capacity);
    SUMMA_TEST_RUN(test_array_make_with_zero_capacity_allocates_no_elements);
    SUMMA_TEST_RUN(test_array_make_with_zero_capacity_still_grows);
    SUMMA_TEST_RUN(test_array_make_with_exact_capacity_still_grows_past_it);
    SUMMA_TEST_RUN(test_array_init_with_capacity_uses_the_callers_header);
    SUMMA_TEST_RUN(test_array_init_with_zero_capacity_allocates_nothing);
    SUMMA_TEST_RUN(test_array_init_then_grow_behaves_like_any_other_array);
    SUMMA_TEST_RUN(test_array_dispose_leaves_an_empty_array_behind);
    SUMMA_TEST_RUN(test_array_reserve_grows_from_zero_capacity);
    SUMMA_TEST_RUN(test_array_reserve_keeps_existing_elements);
    SUMMA_TEST_RUN(test_array_reserve_never_shrinks);
    SUMMA_TEST_RUN(test_array_copy_into_zero_capacity_destination);
    SUMMA_TEST_RUN(test_array_copy_empty_into_zero_capacity_destination);
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
