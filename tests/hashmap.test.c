#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_HASHMAP_IMPLEMENTATION
#include <summa/hashmap/hashmap.h>

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

void test_hash_map_make_empty() {
    SummaHashMap map = summa_hash_map_make_empty(sizeof(int));
    SUMMA_TEST_ASSERT_NOT_NULL(map);
    SUMMA_TEST_ASSERT_EQ(sizeof(int), map->element_size);
    SUMMA_TEST_ASSERT_NOT_NULL(map->indices);
    SUMMA_TEST_ASSERT_NOT_NULL(map->values);
    SUMMA_TEST_ASSERT_EQ(0u, map->indices->length);
    SUMMA_TEST_ASSERT_EQ(0u, map->values->length);
    summa_hash_map_free(map);
}

void test_hash_map_make_inserts_values() {
    int          a         = 1;
    int          b         = 2;
    void*        values[2] = {&a, &b};
    SummaHashMap map       = summa_hash_map_make(values, 2, sizeof(int));
    SUMMA_TEST_ASSERT_NOT_NULL(map);
    SUMMA_TEST_ASSERT_EQ(2u, map->indices->length);
    SUMMA_TEST_ASSERT_EQ(2u, map->values->length);
    summa_hash_map_free(map);
}

void test_hash_map_make_dedupes_equal_values() {
    int          a         = 7;
    int          b         = 7;
    void*        values[2] = {&a, &b};
    SummaHashMap map       = summa_hash_map_make(values, 2, sizeof(int));
    SUMMA_TEST_ASSERT_NOT_NULL(map);
    SUMMA_TEST_ASSERT_EQ(1u, map->indices->length);
    SUMMA_TEST_ASSERT_EQ(1u, map->values->length);
    summa_hash_map_free(map);
}

void test_hash_map_clear() {
    int          a         = 1;
    void*        values[1] = {&a};
    SummaHashMap map       = summa_hash_map_make(values, 1, sizeof(int));
    summa_hash_map_clear(map);
    SUMMA_TEST_ASSERT_EQ(0u, map->indices->length);
    summa_hash_map_free(map);
}

void test_hash_map_copy() {
    int          a         = 1;
    int          b         = 2;
    void*        values[2] = {&a, &b};
    SummaHashMap src       = summa_hash_map_make(values, 2, sizeof(int));
    SummaHashMap dest      = summa_hash_map_make_empty(sizeof(int));
    summa_hash_map_copy(dest, src);
    SUMMA_TEST_ASSERT_EQ(src->indices->length, dest->indices->length);
    SUMMA_TEST_ASSERT_EQ(src->values->length, dest->values->length);
    summa_hash_map_free(src);
    summa_hash_map_free(dest);
}

int main(void) {
    summa_test_begin("hashmap");
    SUMMA_TEST_RUN(test_hash_is_deterministic);
    SUMMA_TEST_RUN(test_hash_differs_for_different_values);
    SUMMA_TEST_RUN(test_hash_map_make_empty);
    SUMMA_TEST_RUN(test_hash_map_make_inserts_values);
    SUMMA_TEST_RUN(test_hash_map_make_dedupes_equal_values);
    SUMMA_TEST_RUN(test_hash_map_clear);
    SUMMA_TEST_RUN(test_hash_map_copy);
    return summa_test_end();
}
