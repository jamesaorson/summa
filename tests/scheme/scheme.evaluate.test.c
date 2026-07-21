#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

/* The environment owns its bindings' names and values, so teardown is the
 * library's job now -- no walking the binding list by hand here. */
#define SCOPED_GLOBAL_ENV(var) \
    SUMMA_TEST_SCOPED_VALUE(   \
        SummaSchemeEnvironment, var, summa_scheme_environment_make_global(), summa_scheme_environment_free)

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

/* A form owns every value pushed into it, and summa_scheme_value_free is
 * deliberately shallow -- so nested forms are drained here rather than losing
 * the strings their head symbols carry. */
static void free_form(SummaList form) {
    for (size_t i = 0; i < form->length; i++) {
        SummaSchemeValue* value = &form->value[i];
        if (value->type == SummaSchemeListType) {
            free_form(value->value.list.value);
        } else {
            summa_scheme_value_free(value);
        }
    }
    summa_list_free(form);
}

#define SCOPED_FORM(var, init) SUMMA_TEST_SCOPED_VALUE(SummaList, var, init, free_form)

#define HELLO "hello"
#define WORLD "world"
#define HELLO_WORLD HELLO " " WORLD

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

/* A procedure value is self-evaluating: applying one needs a call site to
 * supply the arguments, which a bare value is not. */
void test_scheme_evaluate_procedure() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_STRING(body_proc_name, summa_string_make("+"))
    SCOPED_SYMBOL_LIST(body_proc_bindings, summa_symbol_list_make_empty()) {
        summa_symbol_list_push(body_proc_bindings, &(SummaSchemeSymbol){.value = summa_string_make("x")});
        summa_symbol_list_push(body_proc_bindings, &(SummaSchemeSymbol){.value = summa_string_make("y")});

        SummaSchemeValue in    = summa_make_scheme_procedure(body_proc_name, body_proc_bindings, nullptr);
        SummaSchemeValue out   = {};
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeProcedureType, out.type);
        SUMMA_TEST_ASSERT(summa_scheme_value_equals(&in, &out));
        /* The copy's bindings share the original's strings, so only what the
         * copy allocated for itself is released here. */
        SUMMA_TEST_ASSERT_NEQ(in.value.procedure.bindings, out.value.procedure.bindings);
        summa_string_free(out.value.procedure.name);
        summa_symbol_list_free(out.value.procedure.bindings);
    }
}

/* Application: the head of the form names the procedure, and everything after
 * it is evaluated into the arguments the call is dispatched with. */
void test_scheme_evaluate_application_integer() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_FORM(form, summa_list_make_empty()) {
        summa_list_push(form, &summa_make_scheme_symbol("+"));
        summa_list_push(form, &summa_make_scheme_integer(1));
        summa_list_push(form, &summa_make_scheme_integer(2));
        summa_list_push(form, &summa_make_scheme_integer(3));

        SummaSchemeValue in    = summa_make_scheme_list(form);
        SummaSchemeValue out   = {};
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeIntegerType, out.type);
        SUMMA_TEST_ASSERT_EQ(6, out.value.integer.value);
    }
}

/* One floating argument makes the whole sum floating. */
void test_scheme_evaluate_application_floating() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_FORM(form, summa_list_make_empty()) {
        summa_list_push(form, &summa_make_scheme_symbol("+"));
        summa_list_push(form, &summa_make_scheme_integer(1));
        summa_list_push(form, &summa_make_scheme_floating(2.5));

        SummaSchemeValue in    = summa_make_scheme_list(form);
        SummaSchemeValue out   = {};
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeFloatingType, out.type);
        SUMMA_TEST_ASSERT_EQ(3.5, out.value.floating.value);
    }
}

void test_scheme_evaluate_application_no_arguments() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_FORM(form, summa_list_make_empty()) {
        summa_list_push(form, &summa_make_scheme_symbol("+"));

        SummaSchemeValue in    = summa_make_scheme_list(form);
        SummaSchemeValue out   = {};
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeIntegerType, out.type);
        SUMMA_TEST_ASSERT_EQ(0, out.value.integer.value);
    }
}

/* Arguments are evaluated before the call, so a nested application is just
 * another operand by the time `+` sees it. */
void test_scheme_evaluate_application_nested() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_FORM(form, summa_list_make_empty()) {
        SummaList inner = summa_list_make_empty();
        summa_list_push(inner, &summa_make_scheme_symbol("+"));
        summa_list_push(inner, &summa_make_scheme_integer(2));
        summa_list_push(inner, &summa_make_scheme_integer(3));

        summa_list_push(form, &summa_make_scheme_symbol("+"));
        summa_list_push(form, &summa_make_scheme_integer(1));
        summa_list_push(form, &summa_make_scheme_list(inner));

        SummaSchemeValue in    = summa_make_scheme_list(form);
        SummaSchemeValue out   = {};
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeIntegerType, out.type);
        SUMMA_TEST_ASSERT_EQ(6, out.value.integer.value);
    }
}

void test_scheme_evaluate_application_non_numeric_argument() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_FORM(form, summa_list_make_empty()) {
        summa_list_push(form, &summa_make_scheme_symbol("+"));
        summa_list_push(form, &summa_make_scheme_integer(1));
        summa_list_push(form, &summa_make_scheme_string(HELLO));

        SummaSchemeValue in    = summa_make_scheme_list(form);
        SummaSchemeValue out   = {};
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(error.had);
    }
}

/* A list whose head is not a bound procedure is data, not a call. */
void test_scheme_evaluate_application_unbound_head() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_FORM(form, summa_list_make_empty()) {
        summa_list_push(form, &summa_make_scheme_symbol(WORLD));
        summa_list_push(form, &summa_make_scheme_integer(1));

        SummaSchemeValue in    = summa_make_scheme_list(form);
        SummaSchemeValue out   = {};
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeListType, out.type);
        SUMMA_TEST_ASSERT(summa_scheme_value_equals(&in, &out));

        summa_list_free(out.value.list.value);
    }
}

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
    SUMMA_TEST_RUN(test_scheme_evaluate_application_integer);
    SUMMA_TEST_RUN(test_scheme_evaluate_application_floating);
    SUMMA_TEST_RUN(test_scheme_evaluate_application_no_arguments);
    SUMMA_TEST_RUN(test_scheme_evaluate_application_nested);
    SUMMA_TEST_RUN(test_scheme_evaluate_application_non_numeric_argument);
    SUMMA_TEST_RUN(test_scheme_evaluate_application_unbound_head);
    SUMMA_TEST_RUN(test_scheme_evaluate_string);
    SUMMA_TEST_RUN(test_scheme_evaluate_symbol);
    SUMMA_TEST_RUN(test_scheme_evaluate_vector);

    return summa_test_end();
}
