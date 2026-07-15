#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

void test_scheme_read() {
    SummaSchemeExpression expression;
    SummaSchemeError      error = summa_scheme_read("(define x 1)", &expression);
    // TODO: Make this not fail
    SUMMA_TEST_ASSERT(error.had);
    SUMMA_TEST_ASSERT_EQ_STR("summa_scheme_read - NOT IMPLEMENTED", error.message);
}

void test_scheme_evaluate() {
    SummaSchemeExpression expression = (SummaSchemeExpression){};
    SummaSchemeValue      value;
    SummaSchemeError      error = summa_scheme_evaluate(expression, &value);
    // TODO: Make this not fail
    SUMMA_TEST_ASSERT(error.had);
    SUMMA_TEST_ASSERT_EQ_STR("summa_scheme_evaluate - NOT IMPLEMENTED", error.message);
}

void test_scheme_print_boolean() {
    SUMMA_TEST_SCOPED_FILE(f) {
        SummaSchemeValue value = summa_make_scheme_boolean(true);
        SummaSchemeError error = summa_scheme_print(value, f.file);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_FILE_EQ_STR(f, SUMMA_SCHEME_TRUE);

        value = summa_make_scheme_boolean(false);
        error = summa_scheme_print(value, f.file);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_FILE_EQ_STR(f, SUMMA_SCHEME_FALSE);
    }
}

void test_scheme_print_character() {
    for (unsigned int i = 0; i < 128; i++) {
        const char c = (char)i;
        SUMMA_TEST_SCOPED_FILE(f) {
            SummaSchemeValue value = summa_make_scheme_character(c);
            SummaSchemeError error = summa_scheme_print(value, f.file);
            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT_FILE_EQ_CHAR(f, c);
        }
    }
}

#define NUM_RANDOM_CASES 1024
#define NUM_STR_LEN 32

void test_scheme_print_floating() {
    summa_test_random_seed();
    char str[NUM_STR_LEN] = "\0";
    for (int i = 0; i < NUM_RANDOM_CASES; i++) {
        SUMMA_TEST_SCOPED_FILE(f) {
            SummaSchemeValue value = summa_make_scheme_floating(
                summa_test_random_double_between(-1 * 1000 * 1000 * 1000, 1 * 1000 * 1000 * 1000));
            snprintf(str, sizeof(str), "%f", value.value.floating.value);
            SummaSchemeError error = summa_scheme_print(value, f.file);
            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT_FILE_EQ_STR(f, str);
        }
    }
}

void test_scheme_print_integer() {
    summa_test_random_seed();
    char str[NUM_STR_LEN] = "\0";
    for (int i = 0; i < NUM_RANDOM_CASES; i++) {
        SUMMA_TEST_SCOPED_FILE(f) {
            SummaSchemeValue value =
                summa_make_scheme_integer(summa_test_random_integer_between(INT64_MIN, INT64_MAX));
            snprintf(str, sizeof(str), "%" PRId64, value.value.integer.value);
            SummaSchemeError error = summa_scheme_print(value, f.file);
            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT_FILE_EQ_STR(f, str);
        }
    }
}

void test_scheme_print_list() {
    SummaList        nested    = summa_list_make_empty();
    SummaSchemeValue values[5] = {
        summa_make_scheme_boolean(true),
        summa_make_scheme_integer(420),
        summa_make_scheme_floating(3.14),
        summa_make_scheme_list(nested),
        summa_make_scheme_boolean(false),
    };
    SUMMA_TEST_SCOPED_FILE(f) {
        SummaList        list  = summa_list_make(values, sizeof(values) / sizeof(values[0]));
        SummaSchemeValue value = summa_make_scheme_list(list);
        SummaSchemeError error = summa_scheme_print(value, f.file);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_FILE_EQ_STR(f, "(#t 420 3.140000 () #f)");
        summa_list_free(list);
    }
    summa_list_free(nested);
}

void test_scheme_print_procedure() {
    // TODO: Finish
    SUMMA_TEST_SCOPED_FILE(f) {
        SummaSchemeValue value = summa_make_scheme_procedure((void*)nullptr);
        SummaSchemeError error = summa_scheme_print(value, f.file);
        SUMMA_TEST_ASSERT(error.had);
    }
}

#define HELLO_WORLD "hello world"

void test_scheme_print_string() {
    SUMMA_TEST_SCOPED_FILE(f) {
        SummaSchemeValue value = summa_make_scheme_string(HELLO_WORLD);
        SummaSchemeError error = summa_scheme_print(value, f.file);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_FILE_EQ_STR(f, "\"" HELLO_WORLD "\"");
        summa_string_free(value.value.string.value);
    }
}

void test_scheme_print_symbol() {
    // TODO: Finish, will be very hard
    SUMMA_TEST_SCOPED_FILE(f) {
        SummaSchemeValue value = summa_make_scheme_symbol((void*)nullptr);
        SummaSchemeError error = summa_scheme_print(value, f.file);
        SUMMA_TEST_ASSERT(error.had);
    }
}

void test_scheme_print_vector() {
    // TODO: Finish
    SUMMA_TEST_SCOPED_FILE(f) {
        SummaSchemeValue value = summa_make_scheme_vector((void*)nullptr);
        SummaSchemeError error = summa_scheme_print(value, f.file);
        SUMMA_TEST_ASSERT(error.had);
    }
}

int main(void) {
    summa_test_begin("scheme");
    SUMMA_TEST_RUN(test_scheme_read);
    SUMMA_TEST_RUN(test_scheme_evaluate);
    SUMMA_TEST_RUN(test_scheme_print_boolean);
    SUMMA_TEST_RUN(test_scheme_print_character);
    SUMMA_TEST_RUN(test_scheme_print_floating);
    SUMMA_TEST_RUN(test_scheme_print_integer);
    SUMMA_TEST_RUN(test_scheme_print_list);
    SUMMA_TEST_RUN(test_scheme_print_procedure);
    SUMMA_TEST_RUN(test_scheme_print_string);
    SUMMA_TEST_RUN(test_scheme_print_symbol);
    SUMMA_TEST_RUN(test_scheme_print_vector);
    return summa_test_end();
}
