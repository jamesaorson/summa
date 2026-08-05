#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <stdint.h>
#include <stdlib.h>

#define HELLO_WORLD "hello world"

#define SCOPED_STRING(var, init) SUMMA_TEST_SCOPED_VALUE(SummaString, var, init, summa_string_free)

void test_string_make() {
    SCOPED_STRING(str, summa_string_make(HELLO_WORLD)) {
        SUMMA_TEST_ASSERT_NOT_NULL(str);
        SUMMA_TEST_ASSERT_EQ_STR(HELLO_WORLD, str->value);
        SUMMA_TEST_ASSERT_EQ(strlen(HELLO_WORLD), str->length);
        /* Capacity reserves room for the trailing null byte, but length does not
         * count it: capacity == length + 1 for a freshly-made string. */
        SUMMA_TEST_ASSERT_EQ(str->length + 1, str->capacity);
        SUMMA_TEST_ASSERT_EQ('\0', str->value[str->length]);
    }
}

/* `init` is `make` writing into a header the caller supplies -- here a C local,
 * which is what lets a string be a field of something bigger rather than a
 * second allocation beside it. The invariant has to come out the same. */
void test_string_init_fills_a_header_the_caller_owns() {
    SummaString_t str = {};
    summa_string_init(&str, HELLO_WORLD);
    SUMMA_TEST_ASSERT_EQ_STR(HELLO_WORLD, str.value);
    SUMMA_TEST_ASSERT_EQ(strlen(HELLO_WORLD), str.length);
    SUMMA_TEST_ASSERT_EQ(str.length + 1, str.capacity);
    SUMMA_TEST_ASSERT_EQ('\0', str.value[str.length]);
    /* Not scoped: the header is a local and only the characters were
     * allocated, so `dispose` is the whole of the teardown. */
    summa_string_dispose(&str);
    SUMMA_TEST_ASSERT_EQ(0u, str.length);
    SUMMA_TEST_ASSERT_EQ(0u, str.capacity);
    SUMMA_TEST_ASSERT_NULL(str.value);
}

/* The empty string through `init`: one byte of capacity for the terminator and
 * nothing else. */
void test_string_init_empty_cstr() {
    SummaString_t str = {};
    summa_string_init(&str, "");
    SUMMA_TEST_ASSERT_EQ(0u, str.length);
    SUMMA_TEST_ASSERT_EQ(1u, str.capacity);
    SUMMA_TEST_ASSERT_EQ('\0', str.value[0]);
    summa_string_dispose(&str);
}

/* An initialised string is an ordinary one: it grows through the same push,
 * and `elements` was always its own heap block. */
void test_string_init_grows_like_any_other() {
    SummaString_t str = {};
    summa_string_init(&str, "a");
    for (int i = 0; i < 32; i++) {
        summa_string_push(&str, 'b');
    }
    SUMMA_TEST_ASSERT_EQ(33u, str.length);
    SUMMA_TEST_ASSERT_EQ('\0', str.value[str.length]);
    SUMMA_TEST_ASSERT_EQ('a', str.value[0]);
    SUMMA_TEST_ASSERT_EQ('b', str.value[32]);
    summa_string_dispose(&str);
}

void test_string_make_empty_cstr() {
    SCOPED_STRING(str, summa_string_make("")) {
        SUMMA_TEST_ASSERT_NOT_NULL(str);
        SUMMA_TEST_ASSERT_EQ(0u, str->length);
        SUMMA_TEST_ASSERT_EQ(1u, str->capacity);
        SUMMA_TEST_ASSERT_EQ('\0', str->value[0]);
    }
}

void test_string_make_empty() {
    SCOPED_STRING(str, summa_string_make_empty()) {
        SUMMA_TEST_ASSERT_NOT_NULL(str);
        SUMMA_TEST_ASSERT_EQ(0u, str->length);
        SUMMA_TEST_ASSERT_EQ(SUMMA_ARRAY_DEFAULT_CAPACITY, str->capacity);
        /* summa_array_make_empty eagerly allocates the default capacity, so
         * value must not be a dangling/garbage pointer. */
        SUMMA_TEST_ASSERT_NOT_NULL(str->value);
        /* And the allocation is terminated, not merely present -- reading it as
         * a C string has to be safe before anything is pushed. */
        SUMMA_TEST_ASSERT_EQ('\0', str->value[0]);
        SUMMA_TEST_ASSERT_EQ_STR("", str->value);
    }
}

void test_string_clear() {
    SCOPED_STRING(str, summa_string_make(HELLO_WORLD)) {
        size_t cap = str->capacity;
        summa_string_clear(str);
        SUMMA_TEST_ASSERT_EQ(0u, str->length);
        /* Clearing is a logical reset, not a dealloc: capacity (and the room it
         * reserves for a null byte) is untouched. */
        SUMMA_TEST_ASSERT_EQ(cap, str->capacity);
        /* A logical reset still has to terminate: without this the old contents
         * read straight back through value. */
        SUMMA_TEST_ASSERT_EQ_STR("", str->value);
    }
}

void test_string_copy_grows_destination() {
    SCOPED_STRING(dest, summa_string_make("x"))
    SCOPED_STRING(src, summa_string_make(HELLO_WORLD)) {
        summa_string_copy(dest, src);
        SUMMA_TEST_ASSERT_EQ_STR(HELLO_WORLD, dest->value);
        SUMMA_TEST_ASSERT_EQ(strlen(HELLO_WORLD), dest->length);
        /* A growing copy reallocates, so capacity only has to leave room for the
         * null byte -- it isn't required to be the tightest possible fit. */
        SUMMA_TEST_ASSERT(dest->capacity > dest->length);
        SUMMA_TEST_ASSERT_EQ('\0', dest->value[dest->length]);
    }
}

void test_string_copy_shrinks_length_but_keeps_capacity() {
    SCOPED_STRING(dest, summa_string_make(HELLO_WORLD))
    SCOPED_STRING(src, summa_string_make("hi")) {
        size_t cap = dest->capacity;
        summa_string_copy(dest, src);
        SUMMA_TEST_ASSERT_EQ_STR("hi", dest->value);
        SUMMA_TEST_ASSERT_EQ(2u, dest->length);
        /* Destination buffer already had enough room, so no realloc/shrink
         * happens; capacity is left as-is even though it now exceeds length + 1. */
        SUMMA_TEST_ASSERT_EQ(cap, dest->capacity);
        SUMMA_TEST_ASSERT_EQ('\0', dest->value[dest->length]);
    }
}

void test_string_copy_into_empty_destination() {
    SCOPED_STRING(dest, summa_string_make_empty())
    SCOPED_STRING(src, summa_string_make(HELLO_WORLD)) {
        summa_string_copy(dest, src);
        SUMMA_TEST_ASSERT_EQ_STR(HELLO_WORLD, dest->value);
        SUMMA_TEST_ASSERT_EQ(strlen(HELLO_WORLD), dest->length);
        SUMMA_TEST_ASSERT(dest->capacity > dest->length);
    }
}

void test_string_copy_empty_source() {
    SCOPED_STRING(dest, summa_string_make(HELLO_WORLD))
    SCOPED_STRING(src, summa_string_make("")) {
        summa_string_copy(dest, src);
        SUMMA_TEST_ASSERT_EQ_STR("", dest->value);
        SUMMA_TEST_ASSERT_EQ(0u, dest->length);
    }
}

void test_string_copy_cstr_grows_destination() {
    SCOPED_STRING(dest, summa_string_make("x")) {
        summa_string_copy_cstr(dest, HELLO_WORLD);
        SUMMA_TEST_ASSERT_EQ_STR(HELLO_WORLD, dest->value);
        SUMMA_TEST_ASSERT_EQ(strlen(HELLO_WORLD), dest->length);
        SUMMA_TEST_ASSERT(dest->capacity > dest->length);
        SUMMA_TEST_ASSERT_EQ('\0', dest->value[dest->length]);
    }
}

void test_string_copy_cstr_shrinks_length_but_keeps_capacity() {
    SCOPED_STRING(dest, summa_string_make(HELLO_WORLD)) {
        size_t cap = dest->capacity;
        summa_string_copy_cstr(dest, "hi");
        SUMMA_TEST_ASSERT_EQ_STR("hi", dest->value);
        SUMMA_TEST_ASSERT_EQ(2u, dest->length);
        SUMMA_TEST_ASSERT_EQ(cap, dest->capacity);
        SUMMA_TEST_ASSERT_EQ('\0', dest->value[dest->length]);
    }
}

void test_string_copy_cstr_into_empty_destination() {
    SCOPED_STRING(dest, summa_string_make_empty()) {
        summa_string_copy_cstr(dest, HELLO_WORLD);
        SUMMA_TEST_ASSERT_EQ_STR(HELLO_WORLD, dest->value);
        SUMMA_TEST_ASSERT_EQ(strlen(HELLO_WORLD), dest->length);
        SUMMA_TEST_ASSERT(dest->capacity > dest->length);
    }
}

void test_string_copy_cstr_empty_string() {
    SCOPED_STRING(dest, summa_string_make(HELLO_WORLD)) {
        summa_string_copy_cstr(dest, "");
        SUMMA_TEST_ASSERT_EQ_STR("", dest->value);
        SUMMA_TEST_ASSERT_EQ(0u, dest->length);
        SUMMA_TEST_ASSERT_EQ('\0', dest->value[0]);
    }
}

#pragma region push

void test_string_push_appends() {
    SCOPED_STRING(str, summa_string_make_empty()) {
        summa_string_push(str, 'a');
        summa_string_push(str, 'b');
        summa_string_push(str, 'c');

        SUMMA_TEST_ASSERT_EQ(3u, str->length);
        SUMMA_TEST_ASSERT_EQ_STR("abc", str->value);
        SUMMA_TEST_ASSERT_EQ('\0', str->value[str->length]);
    }
}

void test_string_push_onto_existing_contents() {
    SCOPED_STRING(str, summa_string_make("hell")) {
        summa_string_push(str, 'o');

        SUMMA_TEST_ASSERT_EQ(5u, str->length);
        SUMMA_TEST_ASSERT_EQ_STR("hello", str->value);
    }
}

/* A freshly made string has capacity == length + 1, so the very first push has
 * no slack and must grow. This is the case a plain summa_array_push would get
 * wrong, writing the terminator one byte past the allocation. */
void test_string_push_grows_a_full_string() {
    SCOPED_STRING(str, summa_string_make("x")) {
        SUMMA_TEST_ASSERT_EQ(str->length + 1, str->capacity);

        summa_string_push(str, 'y');

        SUMMA_TEST_ASSERT_EQ_STR("xy", str->value);
        SUMMA_TEST_ASSERT(str->capacity >= str->length + 1);
        SUMMA_TEST_ASSERT_EQ('\0', str->value[str->length]);
    }
}

/* Enough pushes to force several reallocations. */
void test_string_push_many() {
    SCOPED_STRING(str, summa_string_make_empty()) {
        for (size_t i = 0; i < 1000; i++) {
            summa_string_push(str, 'z');
        }

        SUMMA_TEST_ASSERT_EQ(1000u, str->length);
        SUMMA_TEST_ASSERT_EQ('\0', str->value[str->length]);
        SUMMA_TEST_ASSERT_EQ(1000u, strlen(str->value));
    }
}

void test_string_push_after_clear() {
    SCOPED_STRING(str, summa_string_make(HELLO_WORLD)) {
        summa_string_clear(str);
        summa_string_push(str, 'h');
        summa_string_push(str, 'i');

        SUMMA_TEST_ASSERT_EQ_STR("hi", str->value);
    }
}

#pragma endregion push

#pragma region contains and index_of

void test_string_contains() {
    SCOPED_STRING(str, summa_string_make(HELLO_WORLD)) {
        SUMMA_TEST_ASSERT(summa_string_contains(str, 'h'));
        SUMMA_TEST_ASSERT(summa_string_contains(str, ' '));
        SUMMA_TEST_ASSERT(summa_string_contains(str, 'd'));
        SUMMA_TEST_ASSERT(!summa_string_contains(str, 'z'));
    }
}

/* The terminator is past `length`, so it is not part of the contents. */
void test_string_does_not_contain_its_terminator() {
    SCOPED_STRING(str, summa_string_make(HELLO_WORLD)) {
        SUMMA_TEST_ASSERT(!summa_string_contains(str, '\0'));
    }
}

void test_string_contains_nothing_when_empty() {
    SCOPED_STRING(str, summa_string_make_empty()) {
        SUMMA_TEST_ASSERT(!summa_string_contains(str, 'a'));
    }
}

void test_string_index_of_finds_the_first_match() {
    SCOPED_STRING(str, summa_string_make(HELLO_WORLD)) {
        size_t index = 99;
        SUMMA_TEST_ASSERT(summa_string_index_of(str, 'l', &index));
        SUMMA_TEST_ASSERT_EQ(2u, index);

        SUMMA_TEST_ASSERT(summa_string_index_of(str, ' ', &index));
        SUMMA_TEST_ASSERT_EQ(5u, index);
    }
}

void test_string_index_of_reports_a_miss() {
    SCOPED_STRING(str, summa_string_make(HELLO_WORLD)) {
        size_t index = 99;
        SUMMA_TEST_ASSERT(!summa_string_index_of(str, 'z', &index));
        /* Left alone on a miss, as the array version does. */
        SUMMA_TEST_ASSERT_EQ(99u, index);
    }
}

void test_string_index_of_accepts_a_null_out_index() {
    SCOPED_STRING(str, summa_string_make(HELLO_WORLD)) {
        SUMMA_TEST_ASSERT(summa_string_index_of(str, 'w', nullptr));
    }
}

#pragma endregion contains and index_of

#pragma region remove_at and set_at

void test_string_remove_at() {
    SCOPED_STRING(str, summa_string_make("abcd")) {
        summa_string_remove_at(str, 1);

        SUMMA_TEST_ASSERT_EQ(3u, str->length);
        SUMMA_TEST_ASSERT_EQ_STR("acd", str->value);
        SUMMA_TEST_ASSERT_EQ('\0', str->value[str->length]);
    }
}

void test_string_remove_at_first_and_last() {
    SCOPED_STRING(str, summa_string_make("abc")) {
        summa_string_remove_at(str, 0);
        SUMMA_TEST_ASSERT_EQ_STR("bc", str->value);

        summa_string_remove_at(str, 1);
        SUMMA_TEST_ASSERT_EQ_STR("b", str->value);

        summa_string_remove_at(str, 0);
        SUMMA_TEST_ASSERT_EQ_STR("", str->value);
        SUMMA_TEST_ASSERT_EQ(0u, str->length);
    }
}

void test_string_remove_at_ignores_an_out_of_range_index() {
    SCOPED_STRING(str, summa_string_make("abc")) {
        summa_string_remove_at(str, 3);
        summa_string_remove_at(str, 100);

        SUMMA_TEST_ASSERT_EQ(3u, str->length);
        SUMMA_TEST_ASSERT_EQ_STR("abc", str->value);
    }
}

void test_string_set_at() {
    SCOPED_STRING(str, summa_string_make("cat")) {
        summa_string_set_at(str, 0, 'b');

        SUMMA_TEST_ASSERT_EQ_STR("bat", str->value);
        SUMMA_TEST_ASSERT_EQ(3u, str->length);
    }
}

void test_string_set_at_ignores_an_out_of_range_index() {
    SCOPED_STRING(str, summa_string_make("cat")) {
        summa_string_set_at(str, 3, 'x');
        summa_string_set_at(str, 100, 'x');

        SUMMA_TEST_ASSERT_EQ_STR("cat", str->value);
        SUMMA_TEST_ASSERT_EQ(3u, str->length);
    }
}

#pragma endregion remove_at and set_at

/* Building a token one character at a time, then reading it back as a C string
 * -- what a reader does, and the reason push exists. */
void test_string_accumulate_then_compare() {
    SCOPED_STRING(built, summa_string_make_empty())
    SCOPED_STRING(expected, summa_string_make("define")) {
        const char* source = "define";
        for (const char* c = source; *c; c++) {
            summa_string_push(built, *c);
        }

        SUMMA_TEST_ASSERT_EQ(0, summa_string_cmp(built, expected));
        SUMMA_TEST_ASSERT_EQ(expected->length, built->length);
    }
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.string", argc, argv);
    SUMMA_TEST_RUN(test_string_make);
    SUMMA_TEST_RUN(test_string_init_fills_a_header_the_caller_owns);
    SUMMA_TEST_RUN(test_string_init_empty_cstr);
    SUMMA_TEST_RUN(test_string_init_grows_like_any_other);
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

    SUMMA_TEST_RUN(test_string_push_appends);
    SUMMA_TEST_RUN(test_string_push_onto_existing_contents);
    SUMMA_TEST_RUN(test_string_push_grows_a_full_string);
    SUMMA_TEST_RUN(test_string_push_many);
    SUMMA_TEST_RUN(test_string_push_after_clear);

    SUMMA_TEST_RUN(test_string_contains);
    SUMMA_TEST_RUN(test_string_does_not_contain_its_terminator);
    SUMMA_TEST_RUN(test_string_contains_nothing_when_empty);
    SUMMA_TEST_RUN(test_string_index_of_finds_the_first_match);
    SUMMA_TEST_RUN(test_string_index_of_reports_a_miss);
    SUMMA_TEST_RUN(test_string_index_of_accepts_a_null_out_index);

    SUMMA_TEST_RUN(test_string_remove_at);
    SUMMA_TEST_RUN(test_string_remove_at_first_and_last);
    SUMMA_TEST_RUN(test_string_remove_at_ignores_an_out_of_range_index);
    SUMMA_TEST_RUN(test_string_set_at);
    SUMMA_TEST_RUN(test_string_set_at_ignores_an_out_of_range_index);

    SUMMA_TEST_RUN(test_string_accumulate_then_compare);
    return summa_test_end();
}
