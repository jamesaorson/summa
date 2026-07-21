#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_HASH_MAP_IMPLEMENTATION
#include <summa/hash_map/hash_map.h>

#define SCOPED_MAP(var, init) SUMMA_TEST_SCOPED_VALUE(SummaHashMap, var, init, summa_hash_map_free)
#define SCOPED_INT_MAP(var) SCOPED_MAP(var, summa_hash_map_make_empty(sizeof(int)))

void test_hash_map_make_empty() {
    SCOPED_INT_MAP(map) {
        SUMMA_TEST_ASSERT_NOT_NULL(map);
        SUMMA_TEST_ASSERT_EQ(sizeof(int), map->key_size);
        SUMMA_TEST_ASSERT_NOT_NULL(map->keys);
        SUMMA_TEST_ASSERT_NOT_NULL(map->values);
        SUMMA_TEST_ASSERT_EQ(0u, map->keys->length);
        SUMMA_TEST_ASSERT_EQ(0u, map->values->length);
    }
}

void test_hash_map_put_new_key() {
    SCOPED_INT_MAP(map) {
        int key = 1;
        int val = 100;
        SUMMA_TEST_ASSERT(summa_hash_map_put(map, &key, &val));
        SUMMA_TEST_ASSERT_EQ(1u, map->keys->length);
        SUMMA_TEST_ASSERT_EQ(1u, map->values->length);
    }
}

void test_hash_map_put_and_get_roundtrip() {
    SCOPED_INT_MAP(map) {
        int key = 1;
        int val = 100;
        summa_hash_map_put(map, &key, &val);
        void* out = nullptr;
        SUMMA_TEST_ASSERT(summa_hash_map_get(map, &key, &out));
        SUMMA_TEST_ASSERT_EQ(&val, out);
        SUMMA_TEST_ASSERT_EQ(100, *(int*)out);
    }
}

void test_hash_map_get_missing_key() {
    SCOPED_INT_MAP(map) {
        int key     = 1;
        int val     = 100;
        int missing = 2;
        summa_hash_map_put(map, &key, &val);
        void* out = nullptr;
        SUMMA_TEST_ASSERT(!summa_hash_map_get(map, &missing, &out));
    }
}

void test_hash_map_put_existing_key_updates_value_and_returns_false() {
    SCOPED_INT_MAP(map) {
        int key  = 1;
        int val1 = 100;
        int val2 = 200;
        SUMMA_TEST_ASSERT(summa_hash_map_put(map, &key, &val1));
        SUMMA_TEST_ASSERT(!summa_hash_map_put(map, &key, &val2));
        SUMMA_TEST_ASSERT_EQ(1u, map->keys->length);
        SUMMA_TEST_ASSERT_EQ(1u, map->values->length);
        void* out = nullptr;
        SUMMA_TEST_ASSERT(summa_hash_map_get(map, &key, &out));
        SUMMA_TEST_ASSERT_EQ(200, *(int*)out);
    }
}

void test_hash_map_distinct_keys_with_equal_values_both_stored() {
    SCOPED_INT_MAP(map) {
        int key1 = 1;
        int key2 = 2;
        int val  = 42;
        SUMMA_TEST_ASSERT(summa_hash_map_put(map, &key1, &val));
        SUMMA_TEST_ASSERT(summa_hash_map_put(map, &key2, &val));
        SUMMA_TEST_ASSERT_EQ(2u, map->keys->length);
        SUMMA_TEST_ASSERT_EQ(2u, map->values->length);
    }
}

void test_hash_map_contains_found() {
    SCOPED_INT_MAP(map) {
        int key = 1;
        int val = 100;
        summa_hash_map_put(map, &key, &val);
        SUMMA_TEST_ASSERT(summa_hash_map_contains(map, &key));
    }
}

void test_hash_map_contains_not_found() {
    SCOPED_INT_MAP(map) {
        int missing = 1;
        SUMMA_TEST_ASSERT(!summa_hash_map_contains(map, &missing));
    }
}

void test_hash_map_remove_existing_key() {
    SCOPED_INT_MAP(map) {
        int key1 = 1;
        int key2 = 2;
        int val1 = 100;
        int val2 = 200;
        summa_hash_map_put(map, &key1, &val1);
        summa_hash_map_put(map, &key2, &val2);
        SUMMA_TEST_ASSERT(summa_hash_map_remove(map, &key1));
        SUMMA_TEST_ASSERT_EQ(1u, map->keys->length);
        SUMMA_TEST_ASSERT_EQ(1u, map->values->length);
        SUMMA_TEST_ASSERT(!summa_hash_map_contains(map, &key1));
        SUMMA_TEST_ASSERT(summa_hash_map_contains(map, &key2));
    }
}

void test_hash_map_remove_missing_key_is_noop() {
    SCOPED_INT_MAP(map) {
        int key     = 1;
        int val     = 100;
        int missing = 2;
        summa_hash_map_put(map, &key, &val);
        SUMMA_TEST_ASSERT(!summa_hash_map_remove(map, &missing));
        SUMMA_TEST_ASSERT_EQ(1u, map->keys->length);
    }
}

void test_hash_map_clear() {
    SCOPED_INT_MAP(map) {
        int key = 1;
        int val = 100;
        summa_hash_map_put(map, &key, &val);
        summa_hash_map_clear(map);
        SUMMA_TEST_ASSERT_EQ(0u, map->keys->length);
        SUMMA_TEST_ASSERT_EQ(0u, map->values->length);
        SUMMA_TEST_ASSERT(!summa_hash_map_contains(map, &key));
    }
}

void test_hash_map_copy() {
    SCOPED_INT_MAP(src)
    SCOPED_INT_MAP(dest) {
        int key = 1;
        int val = 100;
        summa_hash_map_put(src, &key, &val);
        summa_hash_map_copy(dest, src);
        SUMMA_TEST_ASSERT_EQ(src->keys->length, dest->keys->length);
        SUMMA_TEST_ASSERT_EQ(src->values->length, dest->values->length);
        SUMMA_TEST_ASSERT(summa_hash_map_contains(dest, &key));
    }
}

int main(int argc, char** argv) {
    summa_test_begin("hash_map", argc, argv);
    SUMMA_TEST_RUN(test_hash_map_make_empty);
    SUMMA_TEST_RUN(test_hash_map_put_new_key);
    SUMMA_TEST_RUN(test_hash_map_put_and_get_roundtrip);
    SUMMA_TEST_RUN(test_hash_map_get_missing_key);
    SUMMA_TEST_RUN(test_hash_map_put_existing_key_updates_value_and_returns_false);
    SUMMA_TEST_RUN(test_hash_map_distinct_keys_with_equal_values_both_stored);
    SUMMA_TEST_RUN(test_hash_map_contains_found);
    SUMMA_TEST_RUN(test_hash_map_contains_not_found);
    SUMMA_TEST_RUN(test_hash_map_remove_existing_key);
    SUMMA_TEST_RUN(test_hash_map_remove_missing_key_is_noop);
    SUMMA_TEST_RUN(test_hash_map_clear);
    SUMMA_TEST_RUN(test_hash_map_copy);
    return summa_test_end();
}
