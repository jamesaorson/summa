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

/* A list payload is counted, so releasing the last handle is what frees it --
 * and the release reaches every value inside. A form and a list of data want
 * the same scope now; they used to want two, because a bare `summa_list_free`
 * could not reach elements. */
#define SCOPED_LIST(var, init) SUMMA_TEST_SCOPED_VALUE(SummaList, var, init, summa_scheme_list_release)

/* A symbol names an interned record the table owns, so the list is all there is
 * to release -- where this used to need a destructor that drained a string out
 * of every element first. */
#define SCOPED_SYMBOL_LIST(var, init) SUMMA_TEST_SCOPED_VALUE(SummaSchemeSymbolList, var, init, summa_symbol_list_free)

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

/* A list in evaluated position is a combination, so one whose head is not a
 * procedure fails to apply rather than standing for itself. Quoting is what
 * yields the list as data. */
void test_scheme_evaluate_list_of_data_is_not_applicable() {
    SCOPED_GLOBAL_ENV(env) {
        /* The nested list is handed to the outer one and is not scoped
         * separately: a payload has one owner at a time, and `summa_scheme_list_make`
         * moves rather than retains. */
        SummaSchemeValue values[5] = {
            summa_make_scheme_boolean(true),
            summa_make_scheme_integer(420),
            summa_make_scheme_floating(3.14),
            summa_make_scheme_list(summa_scheme_list_make_empty()),
            summa_make_scheme_boolean(false),
        };
        SCOPED_LIST(list, summa_scheme_list_make(values, sizeof(values) / sizeof(values[0]))) {
            SummaSchemeValue in    = summa_make_scheme_list(list);
            SummaSchemeValue out   = {};
            SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

            SUMMA_TEST_ASSERT(error.had);
            SUMMA_TEST_ASSERT_EQ_STR("Wrong type to apply: #t", error.message);

            summa_scheme_value_free(&out);
        }
    }
}

/* Only the operator is reached before the failure -- the operands are never
 * evaluated, so a combination cannot fail halfway. */
void test_scheme_evaluate_empty_list_is_not_a_combination() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_LIST(empty, summa_scheme_list_make_empty()) {
        SummaSchemeValue in    = summa_make_scheme_list(empty);
        SummaSchemeValue out   = {};
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(error.had);
        summa_scheme_value_free(&out);
    }
}

/* Quoting is the way to get the list itself. */
void test_scheme_evaluate_quoted_list_is_data() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_LIST(form, summa_scheme_list_make_empty()) {
        SummaList quoted = summa_scheme_list_make_empty();
        summa_list_push(quoted, &summa_make_scheme_integer(1));
        summa_list_push(quoted, &summa_make_scheme_integer(2));

        summa_list_push(form, &summa_make_scheme_symbol("quote"));
        summa_list_push(form, &summa_make_scheme_list(quoted));

        SummaSchemeValue in    = summa_make_scheme_list(form);
        SummaSchemeValue out   = {};
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeListType, out.type);
        SUMMA_TEST_ASSERT_EQ(2, out.value.list.value->length);

        summa_scheme_value_free(&out);
    }
}

/* A procedure value is self-evaluating: applying one needs a call site to
 * supply the arguments, which a bare value is not. */
void test_scheme_evaluate_procedure() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_SYMBOL_LIST(body_proc_bindings, summa_symbol_list_make_empty()) {
        summa_symbol_list_push(body_proc_bindings, &(SummaSchemeSymbol){.value = summa_scheme_symbol_intern("x")});
        summa_symbol_list_push(body_proc_bindings, &(SummaSchemeSymbol){.value = summa_scheme_symbol_intern("y")});

        SummaSchemeValue in =
            summa_make_scheme_procedure(summa_scheme_symbol_intern("+"), body_proc_bindings, nullptr);
        SummaSchemeValue out   = {};
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeProcedureType, out.type);
        SUMMA_TEST_ASSERT(summa_scheme_value_equals(&in, &out));
        /* The copy owns its own binding list -- the names inside it are the
         * intern table's and are shared -- so releasing it cannot disturb the
         * original. */
        SUMMA_TEST_ASSERT_NEQ(in.value.procedure.bindings, out.value.procedure.bindings);
        summa_scheme_value_free(&out);
    }
}

/* Application: the head of the form names the procedure, and everything after
 * it is evaluated into the arguments the call is dispatched with. */
void test_scheme_evaluate_application_integer() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_LIST(form, summa_scheme_list_make_empty()) {
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
    SCOPED_LIST(form, summa_scheme_list_make_empty()) {
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
    SCOPED_LIST(form, summa_scheme_list_make_empty()) {
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
    SCOPED_LIST(form, summa_scheme_list_make_empty()) {
        SummaList inner = summa_scheme_list_make_empty();
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
    SCOPED_LIST(form, summa_scheme_list_make_empty()) {
        summa_list_push(form, &summa_make_scheme_symbol("+"));
        summa_list_push(form, &summa_make_scheme_integer(1));
        summa_list_push(form, &summa_make_scheme_string(HELLO));

        SummaSchemeValue in    = summa_make_scheme_list(form);
        SummaSchemeValue out   = {};
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(error.had);
    }
}

/* An operator that names nothing is an unbound variable, reported as such
 * rather than leaving the form to stand for itself. */
void test_scheme_evaluate_application_unbound_head() {
    SCOPED_GLOBAL_ENV(env)
    SCOPED_LIST(form, summa_scheme_list_make_empty()) {
        summa_list_push(form, &summa_make_scheme_symbol(WORLD));
        summa_list_push(form, &summa_make_scheme_integer(1));

        SummaSchemeValue in    = summa_make_scheme_list(form);
        SummaSchemeValue out   = {};
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(error.had);
        SUMMA_TEST_ASSERT_EQ_STR("Unbound variable: " WORLD, error.message);

        summa_scheme_value_free(&out);
    }
}

void test_scheme_evaluate_string() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue in = summa_make_scheme_string(HELLO_WORLD);
        SummaSchemeValue out;
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT(summa_scheme_value_equals(&in, &out));

        /* Both are handles on the *same* payload -- evaluating a string value
         * retains it rather than duplicating the characters -- so this is two
         * releases of one allocation, not two frees. */
        SUMMA_TEST_ASSERT_EQ(in.value.string.value, out.value.string.value);
        SUMMA_TEST_ASSERT_EQ(2, summa_scheme_string_ref_count(in.value.string.value));
        summa_scheme_value_free(&in);
        summa_scheme_value_free(&out);
    }
}

/* A symbol in evaluated position is a variable reference, so an unbound one is
 * an error. Quoting is what yields the symbol itself. */
void test_scheme_evaluate_unbound_symbol_is_an_error() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue in    = summa_make_scheme_symbol(HELLO);
        SummaSchemeValue out   = {};
        SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

        SUMMA_TEST_ASSERT(error.had);
        SUMMA_TEST_ASSERT_EQ_STR("Unbound variable: " HELLO, error.message);

        /* `in` names an interned record, so there is nothing of it to free. */
        summa_scheme_value_free(&out);
    }
}

void test_scheme_evaluate_vector() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue values[2] = {
            summa_make_scheme_floating(3.14),
            summa_make_scheme_floating(420.69),
        };
        SCOPED_LIST(vector, summa_scheme_list_make(values, sizeof(values) / sizeof(values[0]))) {
            SummaSchemeValue in    = summa_make_scheme_vector(vector);
            SummaSchemeValue out   = {};
            SummaSchemeError error = summa_scheme_evaluate(env, in, &out);

            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT(summa_scheme_value_equals(&in, &out));

            summa_scheme_value_free(&out);
        }
    }
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.evaluate", argc, argv);

    SUMMA_TEST_RUN(test_scheme_evaluate_boolean);
    SUMMA_TEST_RUN(test_scheme_evaluate_character);
    SUMMA_TEST_RUN(test_scheme_evaluate_floating);
    SUMMA_TEST_RUN(test_scheme_evaluate_integer);
    SUMMA_TEST_RUN(test_scheme_evaluate_list_of_data_is_not_applicable);
    SUMMA_TEST_RUN(test_scheme_evaluate_empty_list_is_not_a_combination);
    SUMMA_TEST_RUN(test_scheme_evaluate_quoted_list_is_data);
    SUMMA_TEST_RUN(test_scheme_evaluate_procedure);
    SUMMA_TEST_RUN(test_scheme_evaluate_application_integer);
    SUMMA_TEST_RUN(test_scheme_evaluate_application_floating);
    SUMMA_TEST_RUN(test_scheme_evaluate_application_no_arguments);
    SUMMA_TEST_RUN(test_scheme_evaluate_application_nested);
    SUMMA_TEST_RUN(test_scheme_evaluate_application_non_numeric_argument);
    SUMMA_TEST_RUN(test_scheme_evaluate_application_unbound_head);
    SUMMA_TEST_RUN(test_scheme_evaluate_string);
    SUMMA_TEST_RUN(test_scheme_evaluate_unbound_symbol_is_an_error);
    SUMMA_TEST_RUN(test_scheme_evaluate_vector);

    return summa_test_end();
}
