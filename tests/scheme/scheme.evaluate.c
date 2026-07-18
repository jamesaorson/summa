#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

void test_scheme_evaluate_boolean() {
    SummaSchemeEnvironment env;
    summa_scheme_environment_make_global(env);
    SummaSchemeValue in = summa_make_scheme_boolean(true);
    SummaSchemeValue out;
    SummaSchemeError error = summa_scheme_evaluate(env, in, &out);
    SUMMA_TEST_ASSERT(!error.had);
    SUMMA_TEST_ASSERT_EQ(out.value.boolean.value, true);
}

void test_scheme_evaluate_character() {
    SummaSchemeValue in;
    SummaSchemeValue out;
    SummaSchemeError error;
    for (int c = 0; c < 256; c++) {
        SummaSchemeEnvironment env;
        summa_scheme_environment_make_global(env);
        in    = summa_make_scheme_character((char)c);
        error = summa_scheme_evaluate(env, in, &out);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(out.value.character.value, (char)c);
    }
}

#define NUM_RANDOM_CASES 1024

void test_scheme_evaluate_floating() {
    summa_test_random_seed();
    SummaSchemeValue in;
    SummaSchemeValue out;
    SummaSchemeError error;

    for (int i = 0; i < NUM_RANDOM_CASES; i++) {
        SummaSchemeEnvironment env;
        summa_scheme_environment_make_global(env);
        double value = summa_test_random_double_between(-1 * 1000 * 1000 * 1000, 1 * 1000 * 1000 * 1000);
        in           = summa_make_scheme_floating(value);
        error        = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(out.value.floating.value, value);
        SUMMA_TEST_ASSERT_NEQ(out.value.floating.value, value = 0.0001);
    }
}

void test_scheme_evaluate_integer() {
    summa_test_random_seed();
    SummaSchemeValue in;
    SummaSchemeValue out;
    SummaSchemeError error;

    for (int i = 0; i < NUM_RANDOM_CASES; i++) {
        SummaSchemeEnvironment env;
        summa_scheme_environment_make_global(env);
        int value = summa_test_random_integer_between(INT64_MIN, INT64_MAX);
        in        = summa_make_scheme_integer(value);
        error     = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(out.value.integer.value, value);
        SUMMA_TEST_ASSERT_NEQ(out.value.integer.value, value + 1);
    }
}

void test_scheme_evaluate_list() {
    SummaSchemeEnvironment env;
    summa_scheme_environment_make_global(env);
    SummaSchemeValue in;
    SummaSchemeValue out;

    SummaList        nested    = summa_list_make_empty();
    SummaSchemeValue values[5] = {
        summa_make_scheme_boolean(true),
        summa_make_scheme_integer(420),
        summa_make_scheme_floating(3.14),
        summa_make_scheme_list(nested),
        summa_make_scheme_boolean(false),
    };
    SummaList list         = summa_list_make(values, sizeof(values) / sizeof(values[0]));
    in                     = summa_make_scheme_list(list);
    SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

    SUMMA_TEST_ASSERT(!error.had);
    SUMMA_TEST_ASSERT(summa_scheme_value_equals(&in, &out));
}

void test_scheme_evaluate_procedure() {
    SummaSchemeEnvironment env;
    summa_scheme_environment_make_global(env);
    SummaString           body_proc_name     = summa_string_make("+");
    SummaSchemeSymbolList body_proc_bindings = summa_symbol_list_make_empty();
    summa_symbol_list_push(body_proc_bindings, &(SummaSchemeSymbol){.value = summa_string_make("x")});
    summa_symbol_list_push(body_proc_bindings, &(SummaSchemeSymbol){.value = summa_string_make("y")});

    SummaSchemeValue in = summa_make_scheme_procedure(body_proc_name, body_proc_bindings, nullptr);
    SummaSchemeValue out;
    SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

    // TODO: Make this not fail
    SUMMA_TEST_ASSERT(error.had);
}

#define HELLO "hello"
#define WORLD "world"
#define HELLO_WORLD HELLO " " WORLD

void test_scheme_evaluate_string() {
    SummaSchemeEnvironment env;
    summa_scheme_environment_make_global(env);
    SummaSchemeValue in = summa_make_scheme_string(HELLO_WORLD);
    SummaSchemeValue out;
    SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

    SUMMA_TEST_ASSERT(!error.had);
    SUMMA_TEST_ASSERT(summa_scheme_value_equals(&in, &out));
}

void test_scheme_evaluate_symbol() {
    SummaSchemeEnvironment env;
    summa_scheme_environment_make_global(env);
    SummaSchemeValue in = summa_make_scheme_symbol(HELLO);
    SummaSchemeValue out;
    SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

    SUMMA_TEST_ASSERT(!error.had);
    SUMMA_TEST_ASSERT(summa_scheme_value_equals(&in, &out));
}

void test_scheme_evaluate_vector() {
    SummaSchemeEnvironment env;
    summa_scheme_environment_make_global(env);
    SummaSchemeValue in;
    SummaSchemeValue out;

    SummaSchemeValue values[2] = {
        summa_make_scheme_floating(3.14),
        summa_make_scheme_floating(420.69),
    };
    SummaList vector       = summa_list_make(values, sizeof(values) / sizeof(values[0]));
    in                     = summa_make_scheme_vector(vector);
    SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

    SUMMA_TEST_ASSERT(!error.had);
    SUMMA_TEST_ASSERT(summa_scheme_value_equals(&in, &out));
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.evaluate", argc, argv);

    SUMMA_TEST_RUN(test_scheme_evaluate_boolean);
    SUMMA_TEST_RUN(test_scheme_evaluate_character);
    SUMMA_TEST_RUN(test_scheme_evaluate_floating);
    SUMMA_TEST_RUN(test_scheme_evaluate_integer);
    SUMMA_TEST_RUN(test_scheme_evaluate_list);
    SUMMA_TEST_RUN(test_scheme_evaluate_procedure);
    SUMMA_TEST_RUN(test_scheme_evaluate_string);
    SUMMA_TEST_RUN(test_scheme_evaluate_symbol);
    SUMMA_TEST_RUN(test_scheme_evaluate_vector);

    return summa_test_end();
}
