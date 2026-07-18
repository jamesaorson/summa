
#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#define HELLO "hello"
#define WORLD "world"
#define HELLO_WORLD HELLO " " WORLD

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
    summa_test_random_seed();

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

    summa_list_free(left_nested);
    summa_list_free(right_nested);
    summa_list_free(left_list);
    summa_list_free(right_list);
}

void test_scheme_equals_procedure() {
    SummaSchemeValue* left;
    SummaSchemeValue* right;

    SummaString left_name_a  = summa_string_make("equal-check");
    SummaString right_name_a = summa_string_make("equal-check");
    left                     = &summa_make_scheme_procedure(left_name_a, nullptr, nullptr);
    right                    = &summa_make_scheme_procedure(right_name_a, nullptr, nullptr);

    SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));

    summa_string_free(left_name_a);
    summa_string_free(right_name_a);

    SummaString left_name_b  = summa_string_make("equal-check");
    SummaString right_name_b = summa_string_make("notequal-check");
    left                     = &summa_make_scheme_procedure(left_name_b, nullptr, nullptr);
    right                    = &summa_make_scheme_procedure(right_name_b, nullptr, nullptr);

    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));

    summa_string_free(left_name_b);
    summa_string_free(right_name_b);
}

void test_scheme_equals_string() {
    SummaSchemeValue* left;
    SummaSchemeValue* right;

    left  = &summa_make_scheme_string(HELLO_WORLD);
    right = &summa_make_scheme_string(HELLO_WORLD);
    SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));
    summa_string_free(left->value.string.value);
    summa_string_free(right->value.string.value);

    left  = &summa_make_scheme_string(HELLO_WORLD);
    right = &summa_make_scheme_string(HELLO);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
    summa_string_free(left->value.string.value);
    summa_string_free(right->value.string.value);

    left  = &summa_make_scheme_string(HELLO_WORLD);
    right = &summa_make_scheme_string(WORLD);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
    summa_string_free(left->value.string.value);
    summa_string_free(right->value.string.value);

    left  = &summa_make_scheme_string(HELLO);
    right = &summa_make_scheme_string(HELLO_WORLD);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
    summa_string_free(left->value.string.value);
    summa_string_free(right->value.string.value);

    left  = &summa_make_scheme_string(WORLD);
    right = &summa_make_scheme_string(HELLO_WORLD);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
    summa_string_free(left->value.string.value);
    summa_string_free(right->value.string.value);
}

void test_scheme_equals_symbol() {
    SummaSchemeValue* left;
    SummaSchemeValue* right;

    left  = &summa_make_scheme_symbol(HELLO_WORLD);
    right = &summa_make_scheme_symbol(HELLO_WORLD);
    SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));
    summa_string_free(left->value.symbol.value);
    summa_string_free(right->value.symbol.value);

    left  = &summa_make_scheme_symbol(HELLO_WORLD);
    right = &summa_make_scheme_symbol(HELLO);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
    summa_string_free(left->value.symbol.value);
    summa_string_free(right->value.symbol.value);

    left  = &summa_make_scheme_symbol(HELLO_WORLD);
    right = &summa_make_scheme_symbol(WORLD);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
    summa_string_free(left->value.symbol.value);
    summa_string_free(right->value.symbol.value);

    left  = &summa_make_scheme_symbol(HELLO);
    right = &summa_make_scheme_symbol(HELLO_WORLD);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
    summa_string_free(left->value.symbol.value);
    summa_string_free(right->value.symbol.value);

    left  = &summa_make_scheme_symbol(WORLD);
    right = &summa_make_scheme_symbol(HELLO_WORLD);
    SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
    summa_string_free(left->value.symbol.value);
    summa_string_free(right->value.symbol.value);
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

    summa_list_free(left_vector);
    summa_list_free(right_vector);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.equals", argc, argv);

    SUMMA_TEST_RUN(test_scheme_equals_boolean);
    SUMMA_TEST_RUN(test_scheme_equals_character);
    SUMMA_TEST_RUN(test_scheme_equals_floating);
    SUMMA_TEST_RUN(test_scheme_equals_integer);
    SUMMA_TEST_RUN(test_scheme_equals_list);
    SUMMA_TEST_RUN(test_scheme_equals_procedure);
    SUMMA_TEST_RUN(test_scheme_equals_string);
    SUMMA_TEST_RUN(test_scheme_equals_symbol);
    SUMMA_TEST_RUN(test_scheme_equals_vector);

    return summa_test_end();
}
