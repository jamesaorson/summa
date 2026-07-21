
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

#define SCOPED_LIST(var, init) SUMMA_TEST_SCOPED_VALUE(SummaList, var, init, summa_list_free)
#define SCOPED_STRING(var, init) SUMMA_TEST_SCOPED_VALUE(SummaString, var, init, summa_string_free)

/* String and symbol scheme values own the SummaString the make macro builds for
 * them, so scoping the value has to reach one level in to release it. */
static void free_scheme_string(SummaSchemeValue value) {
    summa_string_free(value.value.string.value);
}

static void free_scheme_symbol(SummaSchemeValue value) {
    summa_string_free(value.value.symbol.value);
}

#define SCOPED_SCHEME_STRING(var, cstr) \
    SUMMA_TEST_SCOPED_VALUE(SummaSchemeValue, var, summa_make_scheme_string(cstr), free_scheme_string)

#define SCOPED_SCHEME_SYMBOL(var, cstr) \
    SUMMA_TEST_SCOPED_VALUE(SummaSchemeValue, var, summa_make_scheme_symbol(cstr), free_scheme_symbol)

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
    SCOPED_LIST(left_nested, summa_list_make_empty())
    SCOPED_LIST(right_nested, summa_list_make_empty()) {
        SummaSchemeValue left_values[5] = {
            summa_make_scheme_boolean(true),
            summa_make_scheme_integer(420),
            summa_make_scheme_floating(3.14),
            summa_make_scheme_list(left_nested),
            summa_make_scheme_boolean(false),
        };
        SummaSchemeValue right_values[5] = {
            summa_make_scheme_boolean(true),
            summa_make_scheme_integer(420),
            summa_make_scheme_floating(3.14),
            summa_make_scheme_list(right_nested),
            summa_make_scheme_boolean(false),
        };

        SCOPED_LIST(left_list, summa_list_make(left_values, sizeof(left_values) / sizeof(left_values[0])))
        SCOPED_LIST(right_list, summa_list_make(right_values, sizeof(right_values) / sizeof(right_values[0]))) {
            SummaSchemeValue* left  = &summa_make_scheme_list(left_list);
            SummaSchemeValue* right = &summa_make_scheme_list(right_list);

            SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));

            right->value.list.value->value[4].value.boolean.value =
                !right->value.list.value->value[4].value.boolean.value;
            SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
        }
    }
}

void test_scheme_equals_procedure() {
    SCOPED_STRING(left_name_a, summa_string_make("equal-check"))
    SCOPED_STRING(right_name_a, summa_string_make("equal-check")) {
        SummaSchemeValue* left  = &summa_make_scheme_procedure(left_name_a, nullptr, nullptr);
        SummaSchemeValue* right = &summa_make_scheme_procedure(right_name_a, nullptr, nullptr);
        SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));
    }

    SCOPED_STRING(left_name_b, summa_string_make("equal-check"))
    SCOPED_STRING(right_name_b, summa_string_make("notequal-check")) {
        SummaSchemeValue* left  = &summa_make_scheme_procedure(left_name_b, nullptr, nullptr);
        SummaSchemeValue* right = &summa_make_scheme_procedure(right_name_b, nullptr, nullptr);
        SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
    }
}

void test_scheme_equals_string() {
    SCOPED_SCHEME_STRING(left, HELLO_WORLD)
    SCOPED_SCHEME_STRING(right, HELLO_WORLD) {
        SUMMA_TEST_ASSERT(summa_scheme_value_equals(&left, &right));
    }

    SCOPED_SCHEME_STRING(left, HELLO_WORLD)
    SCOPED_SCHEME_STRING(right, HELLO) {
        SUMMA_TEST_ASSERT(!summa_scheme_value_equals(&left, &right));
    }

    SCOPED_SCHEME_STRING(left, HELLO_WORLD)
    SCOPED_SCHEME_STRING(right, WORLD) {
        SUMMA_TEST_ASSERT(!summa_scheme_value_equals(&left, &right));
    }

    SCOPED_SCHEME_STRING(left, HELLO)
    SCOPED_SCHEME_STRING(right, HELLO_WORLD) {
        SUMMA_TEST_ASSERT(!summa_scheme_value_equals(&left, &right));
    }

    SCOPED_SCHEME_STRING(left, WORLD)
    SCOPED_SCHEME_STRING(right, HELLO_WORLD) {
        SUMMA_TEST_ASSERT(!summa_scheme_value_equals(&left, &right));
    }
}

void test_scheme_equals_symbol() {
    SCOPED_SCHEME_SYMBOL(left, HELLO_WORLD)
    SCOPED_SCHEME_SYMBOL(right, HELLO_WORLD) {
        SUMMA_TEST_ASSERT(summa_scheme_value_equals(&left, &right));
    }

    SCOPED_SCHEME_SYMBOL(left, HELLO_WORLD)
    SCOPED_SCHEME_SYMBOL(right, HELLO) {
        SUMMA_TEST_ASSERT(!summa_scheme_value_equals(&left, &right));
    }

    SCOPED_SCHEME_SYMBOL(left, HELLO_WORLD)
    SCOPED_SCHEME_SYMBOL(right, WORLD) {
        SUMMA_TEST_ASSERT(!summa_scheme_value_equals(&left, &right));
    }

    SCOPED_SCHEME_SYMBOL(left, HELLO)
    SCOPED_SCHEME_SYMBOL(right, HELLO_WORLD) {
        SUMMA_TEST_ASSERT(!summa_scheme_value_equals(&left, &right));
    }

    SCOPED_SCHEME_SYMBOL(left, WORLD)
    SCOPED_SCHEME_SYMBOL(right, HELLO_WORLD) {
        SUMMA_TEST_ASSERT(!summa_scheme_value_equals(&left, &right));
    }
}

void test_scheme_equals_vector() {
    SummaSchemeValue left_values[2] = {
        summa_make_scheme_boolean(true),
        summa_make_scheme_boolean(false),
    };
    SummaSchemeValue right_values[2] = {
        summa_make_scheme_boolean(true),
        summa_make_scheme_boolean(false),
    };

    SCOPED_LIST(left_vector, summa_list_make(left_values, sizeof(left_values) / sizeof(left_values[0])))
    SCOPED_LIST(right_vector, summa_list_make(right_values, sizeof(right_values) / sizeof(right_values[0]))) {
        SummaSchemeValue* left  = &summa_make_scheme_vector(left_vector);
        SummaSchemeValue* right = &summa_make_scheme_vector(right_vector);

        SUMMA_TEST_ASSERT(summa_scheme_value_equals(left, right));

        right->value.vector.value->value[1].value.boolean.value =
            !right->value.vector.value->value[1].value.boolean.value;
        SUMMA_TEST_ASSERT(!summa_scheme_value_equals(left, right));
    }
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
