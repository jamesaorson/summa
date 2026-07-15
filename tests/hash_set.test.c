#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_HASHMAP_IMPLEMENTATION
#include <summa/hash_set/hash_set.h>

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
    SummaHashSet set = summa_hash_set_make_empty(sizeof(int));
    SUMMA_TEST_ASSERT_NOT_NULL(set);
    SUMMA_TEST_ASSERT_EQ(sizeof(int), set->element_size);
    SUMMA_TEST_ASSERT_NOT_NULL(set->keys);
    SUMMA_TEST_ASSERT_NOT_NULL(set->values);
    SUMMA_TEST_ASSERT_EQ(0u, set->keys->length);
    SUMMA_TEST_ASSERT_EQ(0u, set->values->length);
    summa_hash_set_free(set);
}

void test_hash_set_make_inserts_values() {
    int          a         = 1;
    int          b         = 2;
    void*        values[2] = {&a, &b};
    SummaHashSet set       = summa_hash_set_make(values, 2, sizeof(int));
    SUMMA_TEST_ASSERT_NOT_NULL(set);
    SUMMA_TEST_ASSERT_EQ(2u, set->keys->length);
    SUMMA_TEST_ASSERT_EQ(2u, set->values->length);
    summa_hash_set_free(set);
}

void test_hash_set_make_dedupes_equal_values() {
    int          a         = 7;
    int          b         = 7;
    void*        values[2] = {&a, &b};
    SummaHashSet set       = summa_hash_set_make(values, 2, sizeof(int));
    SUMMA_TEST_ASSERT_NOT_NULL(set);
    SUMMA_TEST_ASSERT_EQ(1u, set->keys->length);
    SUMMA_TEST_ASSERT_EQ(1u, set->values->length);
    summa_hash_set_free(set);
}

void test_hash_set_clear() {
    int          a         = 1;
    void*        values[1] = {&a};
    SummaHashSet set       = summa_hash_set_make(values, 1, sizeof(int));
    summa_hash_set_clear(set);
    SUMMA_TEST_ASSERT_EQ(0u, set->keys->length);
    summa_hash_set_free(set);
}

void test_hash_set_copy() {
    int          a         = 1;
    int          b         = 2;
    void*        values[2] = {&a, &b};
    SummaHashSet src       = summa_hash_set_make(values, 2, sizeof(int));
    SummaHashSet dest      = summa_hash_set_make_empty(sizeof(int));
    summa_hash_set_copy(dest, src);
    SUMMA_TEST_ASSERT_EQ(src->keys->length, dest->keys->length);
    SUMMA_TEST_ASSERT_EQ(src->values->length, dest->values->length);
    summa_hash_set_free(src);
    summa_hash_set_free(dest);
}

int main(void) {
    summa_test_begin("hash_set");
    SUMMA_TEST_RUN(test_hash_is_deterministic);
    SUMMA_TEST_RUN(test_hash_differs_for_different_values);
    SUMMA_TEST_RUN(test_hash_set_make_empty);
    SUMMA_TEST_RUN(test_hash_set_make_inserts_values);
    SUMMA_TEST_RUN(test_hash_set_make_dedupes_equal_values);
    SUMMA_TEST_RUN(test_hash_set_clear);
    SUMMA_TEST_RUN(test_hash_set_copy);
    return summa_test_end();
}
