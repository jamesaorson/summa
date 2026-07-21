#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

static void free_global_env(SummaSchemeEnvironment env) {
    for (size_t i = 0; i < env->bindings->length; i++) {
        SummaSchemeBinding binding = env->bindings->value[i];
        if (binding.value.type == SummaSchemeProcedureType) {
            summa_symbol_list_free(binding.value.value.procedure.bindings);
            summa_list_free(binding.value.value.procedure.body);
        }
        summa_string_free(binding.name);
    }
    summa_binding_list_free(env->bindings);
}

/* summa_scheme_environment_make_global is an assignment macro that evaluates to
 * void, so it can't be a SCOPED_VALUE initializer. This does the same two steps
 * as an expression. Note the environment itself is a compound literal with
 * automatic storage duration, so it must be built in the scope that uses it --
 * which is exactly where the macro expansion puts it -- and never returned from
 * a helper. */
static SummaSchemeEnvironment init_global_env(SummaSchemeEnvironment env) {
    summa_scheme_environment_init_global(env);
    return env;
}

#define SCOPED_GLOBAL_ENV(var) \
    SUMMA_TEST_SCOPED_VALUE(   \
        SummaSchemeEnvironment, var, init_global_env(summa_scheme_environment_make_empty()), free_global_env)

#define SCOPED_LIST(var, init) SUMMA_TEST_SCOPED_VALUE(SummaList, var, init, summa_list_free)
#define SCOPED_STRING(var, init) SUMMA_TEST_SCOPED_VALUE(SummaString, var, init, summa_string_free)

/* A symbol list owns the string inside each symbol, which summa_symbol_list_free
 * doesn't know about -- so the destructor drains them first. */
static void free_symbol_list(SummaSchemeSymbolList symbols) {
    for (size_t i = 0; i < symbols->length; i++) {
        summa_string_free(symbols->value[i].value);
    }
    summa_symbol_list_free(symbols);
}

#define SCOPED_SYMBOL_LIST(var, init) SUMMA_TEST_SCOPED_VALUE(SummaSchemeSymbolList, var, init, free_symbol_list)

void test_scheme_evaluate_boolean() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue in = summa_make_scheme_boolean(true);
        SummaSchemeValue out;
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(out.value.boolean.value, true);
    }
}

void test_scheme_evaluate_character() {
    SummaSchemeValue in;
    SummaSchemeValue out;
    SummaSchemeError error;
    for (int c = 0; c < 256; c++) {
        SCOPED_GLOBAL_ENV(env) {
            in    = summa_make_scheme_character((char)c);
            error = summa_scheme_evaluate(env, in, &out);
            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT_EQ(out.value.character.value, (char)c);
        }
    }
}

#define NUM_RANDOM_CASES 1024

void test_scheme_evaluate_floating() {
    summa_test_random_seed();
    SummaSchemeValue in;
    SummaSchemeValue out;
    SummaSchemeError error;

    for (int i = 0; i < NUM_RANDOM_CASES; i++) {
        SCOPED_GLOBAL_ENV(env) {
            double value = summa_test_random_double_between(-1 * 1000 * 1000 * 1000, 1 * 1000 * 1000 * 1000);
            in           = summa_make_scheme_floating(value);
            error        = summa_scheme_evaluate(env, in, &out);

            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT_EQ(out.value.floating.value, value);
            SUMMA_TEST_ASSERT_NEQ(out.value.floating.value, value = 0.0001);
        }
    }
}

void test_scheme_evaluate_integer() {
    summa_test_random_seed();
    SummaSchemeValue in;
    SummaSchemeValue out;
    SummaSchemeError error;

    for (int i = 0; i < NUM_RANDOM_CASES; i++) {
        SCOPED_GLOBAL_ENV(env) {
            int value = summa_test_random_integer_between(INT64_MIN, INT64_MAX);
            in        = summa_make_scheme_integer(value);
            error     = summa_scheme_evaluate(env, in, &out);

            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT_EQ(out.value.integer.value, value);
            SUMMA_TEST_ASSERT_NEQ(out.value.integer.value, value + 1);
        }
    }
}

void test_scheme_evaluate_list() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_LIST(nested, summa_list_make_empty()) {
        SummaSchemeValue values[5] = {
            summa_make_scheme_boolean(true),
            summa_make_scheme_integer(420),
            summa_make_scheme_floating(3.14),
            summa_make_scheme_list(nested),
            summa_make_scheme_boolean(false),
        };
        SCOPED_LIST(list, summa_list_make(values, sizeof(values) / sizeof(values[0]))) {
            SummaSchemeValue in    = summa_make_scheme_list(list);
            SummaSchemeValue out   = {};
            SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT(summa_scheme_value_equals(&in, &out));

            /* `out` is an output parameter rather than something this scope
             * made, so its inner list is still freed by hand. */
            summa_list_free(out.value.list.value);
        }
    }
}

void test_scheme_evaluate_procedure() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_STRING(body_proc_name, summa_string_make("+"))
    SCOPED_SYMBOL_LIST(body_proc_bindings, summa_symbol_list_make_empty()) {
        summa_symbol_list_push(body_proc_bindings, &(SummaSchemeSymbol){.value = summa_string_make("x")});
        summa_symbol_list_push(body_proc_bindings, &(SummaSchemeSymbol){.value = summa_string_make("y")});

        SummaSchemeValue in = summa_make_scheme_procedure(body_proc_name, body_proc_bindings, nullptr);
        SummaSchemeValue out;
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        // TODO: Make this not fail
        SUMMA_TEST_ASSERT(error.had);
    }
}

#define HELLO "hello"
#define WORLD "world"
#define HELLO_WORLD HELLO " " WORLD

void test_scheme_evaluate_string() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue in = summa_make_scheme_string(HELLO_WORLD);
        SummaSchemeValue out;
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT(summa_scheme_value_equals(&in, &out));

        summa_string_free(in.value.string.value);
        summa_string_free(out.value.string.value);
    }
}

void test_scheme_evaluate_symbol() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue in = summa_make_scheme_symbol(HELLO);
        SummaSchemeValue out;
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT(summa_scheme_value_equals(&in, &out));

        summa_string_free(in.value.symbol.value);
        summa_string_free(out.value.symbol.value);
    }
}

void test_scheme_evaluate_vector() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue values[2] = {
            summa_make_scheme_floating(3.14),
            summa_make_scheme_floating(420.69),
        };
        SCOPED_LIST(vector, summa_list_make(values, sizeof(values) / sizeof(values[0]))) {
            SummaSchemeValue in    = summa_make_scheme_vector(vector);
            SummaSchemeValue out   = {};
            SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT(summa_scheme_value_equals(&in, &out));

            summa_list_free(out.value.vector.value);
        }
    }
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
