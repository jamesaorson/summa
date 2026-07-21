#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_HASH_SET_IMPLEMENTATION
#include <summa/hash_set/hash_set.h>

#define SCOPED_SET(var, init) SUMMA_TEST_SCOPED_VALUE(SummaHashSet, var, init, summa_hash_set_free)
#define SCOPED_INT_SET(var) SCOPED_SET(var, summa_hash_set_make_empty(sizeof(int)))

void test_hash_is_deterministic() {
    int           a  = 42;
    SummaHashCode h1 = summa_hash(&a, sizeof(a));
    SummaHashCode h2 = summa_hash(&a, sizeof(a));
    SUMMA_TEST_ASSERT_EQ(h1, h2);
}

void test_hash_differs_for_different_values() {
    int           a  = 42;
    int           b  = 43;
    SummaHashCode h1 = summa_hash(&a, sizeof(a));
    SummaHashCode h2 = summa_hash(&b, sizeof(b));
    SUMMA_TEST_ASSERT_NEQ(h1, h2);
}

void test_hash_set_make_empty() {
    SCOPED_INT_SET(set) {
        SUMMA_TEST_ASSERT_NOT_NULL(set);
        SUMMA_TEST_ASSERT_EQ(sizeof(int), set->element_size);
        SUMMA_TEST_ASSERT_NOT_NULL(set->keys);
        SUMMA_TEST_ASSERT_NOT_NULL(set->values);
        SUMMA_TEST_ASSERT_EQ(0u, set->keys->length);
        SUMMA_TEST_ASSERT_EQ(0u, set->values->length);
    }
}

void test_hash_set_make_inserts_values() {
    int   a         = 1;
    int   b         = 2;
    void* values[2] = {&a, &b};
    SCOPED_SET(set, summa_hash_set_make(values, 2, sizeof(int))) {
        SUMMA_TEST_ASSERT_NOT_NULL(set);
        SUMMA_TEST_ASSERT_EQ(2u, set->keys->length);
        SUMMA_TEST_ASSERT_EQ(2u, set->values->length);
    }
}

void test_hash_set_make_dedupes_equal_values() {
    int   a         = 7;
    int   b         = 7;
    void* values[2] = {&a, &b};
    SCOPED_SET(set, summa_hash_set_make(values, 2, sizeof(int))) {
        SUMMA_TEST_ASSERT_NOT_NULL(set);
        SUMMA_TEST_ASSERT_EQ(1u, set->keys->length);
        SUMMA_TEST_ASSERT_EQ(1u, set->values->length);
    }
}

void test_hash_set_clear() {
    int   a         = 1;
    void* values[1] = {&a};
    SCOPED_SET(set, summa_hash_set_make(values, 1, sizeof(int))) {
        summa_hash_set_clear(set);
        SUMMA_TEST_ASSERT_EQ(0u, set->keys->length);
    }
}

void test_hash_set_contains_found() {
    int   a         = 1;
    int   b         = 2;
    void* values[2] = {&a, &b};
    SCOPED_SET(set, summa_hash_set_make(values, 2, sizeof(int))) {
        SUMMA_TEST_ASSERT(summa_hash_set_contains(set, &a));
        SUMMA_TEST_ASSERT(summa_hash_set_contains(set, &b));
    }
}

void test_hash_set_contains_not_found() {
    int   a         = 1;
    int   absent    = 99;
    void* values[1] = {&a};
    SCOPED_SET(set, summa_hash_set_make(values, 1, sizeof(int))) {
        SUMMA_TEST_ASSERT(!summa_hash_set_contains(set, &absent));
    }
}

void test_hash_set_add_new_element() {
    SCOPED_INT_SET(set) {
        int a = 5;
        SUMMA_TEST_ASSERT(summa_hash_set_add(set, &a));
        SUMMA_TEST_ASSERT_EQ(1u, set->keys->length);
        SUMMA_TEST_ASSERT_EQ(1u, set->values->length);
        SUMMA_TEST_ASSERT(summa_hash_set_contains(set, &a));
    }
}

void test_hash_set_add_duplicate_is_noop() {
    SCOPED_INT_SET(set) {
        int a = 5;
        SUMMA_TEST_ASSERT(summa_hash_set_add(set, &a));
        SUMMA_TEST_ASSERT(!summa_hash_set_add(set, &a));
        SUMMA_TEST_ASSERT_EQ(1u, set->keys->length);
        SUMMA_TEST_ASSERT_EQ(1u, set->values->length);
    }
}

void test_hash_set_remove_existing_element() {
    int   a         = 1;
    int   b         = 2;
    void* values[2] = {&a, &b};
    SCOPED_SET(set, summa_hash_set_make(values, 2, sizeof(int))) {
        SUMMA_TEST_ASSERT(summa_hash_set_remove(set, &a));
        SUMMA_TEST_ASSERT_EQ(1u, set->keys->length);
        SUMMA_TEST_ASSERT_EQ(1u, set->values->length);
        SUMMA_TEST_ASSERT(!summa_hash_set_contains(set, &a));
        SUMMA_TEST_ASSERT(summa_hash_set_contains(set, &b));
    }
}

void test_hash_set_remove_missing_element_is_noop() {
    int   a         = 1;
    int   absent    = 99;
    void* values[1] = {&a};
    SCOPED_SET(set, summa_hash_set_make(values, 1, sizeof(int))) {
        SUMMA_TEST_ASSERT(!summa_hash_set_remove(set, &absent));
        SUMMA_TEST_ASSERT_EQ(1u, set->keys->length);
    }
}

void test_hash_set_copy() {
    int   a         = 1;
    int   b         = 2;
    void* values[2] = {&a, &b};
    SCOPED_SET(src, summa_hash_set_make(values, 2, sizeof(int)))
    SCOPED_INT_SET(dest) {
        summa_hash_set_copy(dest, src);
        SUMMA_TEST_ASSERT_EQ(src->keys->length, dest->keys->length);
        SUMMA_TEST_ASSERT_EQ(src->values->length, dest->values->length);
    }
}

int main(int argc, char** argv) {
    summa_test_begin("hash_set", argc, argv);
    SUMMA_TEST_RUN(test_hash_is_deterministic);
    SUMMA_TEST_RUN(test_hash_differs_for_different_values);
    SUMMA_TEST_RUN(test_hash_set_make_empty);
    SUMMA_TEST_RUN(test_hash_set_make_inserts_values);
    SUMMA_TEST_RUN(test_hash_set_make_dedupes_equal_values);
    SUMMA_TEST_RUN(test_hash_set_contains_found);
    SUMMA_TEST_RUN(test_hash_set_contains_not_found);
    SUMMA_TEST_RUN(test_hash_set_add_new_element);
    SUMMA_TEST_RUN(test_hash_set_add_duplicate_is_noop);
    SUMMA_TEST_RUN(test_hash_set_remove_existing_element);
    SUMMA_TEST_RUN(test_hash_set_remove_missing_element_is_noop);
    SUMMA_TEST_RUN(test_hash_set_clear);
    SUMMA_TEST_RUN(test_hash_set_copy);
    return summa_test_end();
}
