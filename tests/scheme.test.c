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
    SummaSchemeValue value;
    SummaSchemeError error = summa_scheme_read("(define x 1)", &value);
    // TODO: Make this not fail
    SUMMA_TEST_ASSERT(error.had);
    SUMMA_TEST_ASSERT_EQ_STR("summa_scheme_read - NOT IMPLEMENTED", error.message);
}

void test_scheme_evaluate() {
    SummaSchemeValue in = summa_make_scheme_boolean(true);
    SummaSchemeValue out;
    SummaSchemeError error = summa_scheme_evaluate(in, &out);
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

#define HELLO "hello"
#define WORLD "world"
#define HELLO_WORLD HELLO " " WORLD

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
    SUMMA_TEST_SCOPED_FILE(f) {
        SummaSchemeValue value = summa_make_scheme_symbol(HELLO);
        SummaSchemeError error = summa_scheme_print(value, f.file);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_FILE_EQ_STR(f, HELLO);
        summa_string_free(value.value.symbol.value);
    }
}

void test_scheme_print_vector() {
    SummaSchemeValue values[2] = {
        summa_make_scheme_integer(69),
        summa_make_scheme_integer(420),
    };
    SUMMA_TEST_SCOPED_FILE(f) {
        SummaList        vector = summa_list_make(values, sizeof(values) / sizeof(values[0]));
        SummaSchemeValue value  = summa_make_scheme_vector(vector);
        SummaSchemeError error  = summa_scheme_print(value, f.file);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_FILE_EQ_STR(f, "#(69 420)");
        summa_list_free(vector);
    }
}

void test_scheme_equals_boolean() {
    SummaSchemeValue* left;
    SummaSchemeValue* right;

    left  = &summa_make_scheme_boolean(true);
    right = &summa_make_scheme_boolean(true);
    SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));

    left  = &summa_make_scheme_boolean(false);
    right = &summa_make_scheme_boolean(false);
    SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));

    left  = &summa_make_scheme_boolean(true);
    right = &summa_make_scheme_boolean(false);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));

    left  = &summa_make_scheme_boolean(false);
    right = &summa_make_scheme_boolean(true);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
}

void test_scheme_equals_character() {
    SummaSchemeValue* left;
    SummaSchemeValue* right;

    left  = &summa_make_scheme_character('a');
    right = &summa_make_scheme_character('a');
    SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));

    left  = &summa_make_scheme_character('a');
    right = &summa_make_scheme_character('b');
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
}

void test_scheme_equals_floating() {
    SummaSchemeValue* left;
    SummaSchemeValue* right;

    left =
        &summa_make_scheme_floating(summa_test_random_double_between(-1 * 1000 * 1000 * 1000, 1 * 1000 * 1000 * 1000));
    right = &summa_make_scheme_floating(left->value.floating.value);
    SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));

    left =
        &summa_make_scheme_floating(summa_test_random_double_between(-1 * 1000 * 1000 * 1000, 1 * 1000 * 1000 * 1000));
    right = &summa_make_scheme_floating(left->value.floating.value + 1.0);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
}

void test_scheme_equals_integer() {
    SummaSchemeValue* left;
    SummaSchemeValue* right;

    left  = &summa_make_scheme_character('a');
    right = &summa_make_scheme_character('a');
    SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));

    left  = &summa_make_scheme_character('a');
    right = &summa_make_scheme_character('b');
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
}

void test_scheme_equals_list() {
    SummaSchemeValue* left;
    SummaSchemeValue* right;

    SummaList        left_nested    = summa_list_make_empty();
    SummaSchemeValue left_values[5] = {
        summa_make_scheme_boolean(true),
        summa_make_scheme_integer(420),
        summa_make_scheme_floating(3.14),
        summa_make_scheme_list(left_nested),
        summa_make_scheme_boolean(false),
    };

    SummaList        right_nested    = summa_list_make_empty();
    SummaSchemeValue right_values[5] = {
        summa_make_scheme_boolean(true),
        summa_make_scheme_integer(420),
        summa_make_scheme_floating(3.14),
        summa_make_scheme_list(right_nested),
        summa_make_scheme_boolean(false),
    };

    SummaList left_list  = summa_list_make(left_values, sizeof(left_values) / sizeof(left_values[0]));
    left                 = &summa_make_scheme_list(left_list);
    SummaList right_list = summa_list_make(right_values, sizeof(right_values) / sizeof(right_values[0]));
    right                = &summa_make_scheme_list(right_list);

    SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));

    right->value.list.value->value[4].value.boolean.value = !right->value.list.value->value[4].value.boolean.value;
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
}

void test_scheme_equals_procedure() {
    SUMMA_TEST_TODO("Procedure equality test");
}

void test_scheme_equals_string() {
    SummaSchemeValue* left;
    SummaSchemeValue* right;

    left  = &summa_make_scheme_string(HELLO_WORLD);
    right = &summa_make_scheme_string(HELLO_WORLD);
    SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));

    left  = &summa_make_scheme_string(HELLO_WORLD);
    right = &summa_make_scheme_string(HELLO);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));

    left  = &summa_make_scheme_string(HELLO_WORLD);
    right = &summa_make_scheme_string(WORLD);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));

    left  = &summa_make_scheme_string(HELLO);
    right = &summa_make_scheme_string(HELLO_WORLD);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));

    left  = &summa_make_scheme_string(WORLD);
    right = &summa_make_scheme_string(HELLO_WORLD);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
}

void test_scheme_equals_symbol() {
    SummaSchemeValue* left;
    SummaSchemeValue* right;

    left  = &summa_make_scheme_symbol(HELLO_WORLD);
    right = &summa_make_scheme_symbol(HELLO_WORLD);
    SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));

    left  = &summa_make_scheme_symbol(HELLO_WORLD);
    right = &summa_make_scheme_symbol(HELLO);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));

    left  = &summa_make_scheme_symbol(HELLO_WORLD);
    right = &summa_make_scheme_symbol(WORLD);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));

    left  = &summa_make_scheme_symbol(HELLO);
    right = &summa_make_scheme_symbol(HELLO_WORLD);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));

    left  = &summa_make_scheme_symbol(WORLD);
    right = &summa_make_scheme_symbol(HELLO_WORLD);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
}

void test_scheme_equals_vector() {
    SummaSchemeValue* left;
    SummaSchemeValue* right;

    SummaSchemeValue left_values[2] = {
        summa_make_scheme_boolean(true),
        summa_make_scheme_boolean(false),
    };
    SummaSchemeValue right_values[2] = {
        summa_make_scheme_boolean(true),
        summa_make_scheme_boolean(false),
    };

    SummaList left_vector  = summa_list_make(left_values, sizeof(left_values) / sizeof(left_values[0]));
    left                   = &summa_make_scheme_vector(left_vector);
    SummaList right_vector = summa_list_make(right_values, sizeof(right_values) / sizeof(right_values[0]));
    right                  = &summa_make_scheme_vector(right_vector);

    SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));

    right->value.vector.value->value[1].value.boolean.value = !right->value.vector.value->value[1].value.boolean.value;
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
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

    SUMMA_TEST_RUN(test_scheme_equals_boolean);
    SUMMA_TEST_RUN(test_scheme_equals_character);
    SUMMA_TEST_RUN(test_scheme_equals_floating);
    SUMMA_TEST_RUN(test_scheme_equals_integer);
    SUMMA_TEST_RUN(test_scheme_equals_list);
    // SUMMA_TEST_RUN(test_scheme_equals_procedure);
    SUMMA_TEST_RUN(test_scheme_equals_string);
    SUMMA_TEST_RUN(test_scheme_equals_symbol);
    SUMMA_TEST_RUN(test_scheme_equals_vector);
    return summa_test_end();
}
