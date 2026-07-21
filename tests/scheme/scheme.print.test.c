#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

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
    SCOPED_LIST(nested, summa_list_make_empty()) {
        SummaSchemeValue values[5] = {
            summa_make_scheme_boolean(true),
            summa_make_scheme_integer(420),
            summa_make_scheme_floating(3.14),
            summa_make_scheme_list(nested),
            summa_make_scheme_boolean(false),
        };
        SUMMA_TEST_SCOPED_FILE(f)
        SCOPED_LIST(list, summa_list_make(values, sizeof(values) / sizeof(values[0]))) {
            SummaSchemeValue value = summa_make_scheme_list(list);
            SummaSchemeError error = summa_scheme_print(value, f.file);
            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT_FILE_EQ_STR(f, "(#t 420 3.140000 () #f)");
        }
    }
}

void test_scheme_print_procedure() {
    // TODO: Finish
    SCOPED_STRING(def_name, summa_string_make("add2"))
    SCOPED_SYMBOL_LIST(def_bindings, summa_symbol_list_make_empty())
    SCOPED_LIST(def_body, summa_list_make_empty())
    SCOPED_STRING(body_proc_name, summa_string_make("+"))
    SCOPED_SYMBOL_LIST(body_proc_bindings, summa_symbol_list_make_empty()) {
        SummaSchemeValue def = summa_make_scheme_procedure(def_name, def_bindings, def_body);

        summa_symbol_list_push(def_bindings, &(SummaSchemeSymbol){.value = summa_string_make("x")});
        summa_symbol_list_push(def_bindings, &(SummaSchemeSymbol){.value = summa_string_make("y")});

        summa_symbol_list_push(body_proc_bindings, &(SummaSchemeSymbol){.value = summa_string_make("x")});
        summa_symbol_list_push(body_proc_bindings, &(SummaSchemeSymbol){.value = summa_string_make("y")});
        summa_list_push(def_body, &summa_make_scheme_procedure(body_proc_name, body_proc_bindings, nullptr));

        SUMMA_TEST_SCOPED_FILE(f) {
            SummaSchemeError error = summa_scheme_print(def, f.file);
            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT_FILE_EQ_STR(f, "#<procedure add2 (x y)>");
        }
    }
}

#define HELLO "hello"
#define WORLD "world"
#define HELLO_WORLD HELLO " " WORLD

void test_scheme_print_string() {
    SUMMA_TEST_SCOPED_FILE(f)
    SCOPED_SCHEME_STRING(value, HELLO_WORLD) {
        SummaSchemeError error = summa_scheme_print(value, f.file);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_FILE_EQ_STR(f, "\"" HELLO_WORLD "\"");
    }
}

void test_scheme_print_symbol() {
    SUMMA_TEST_SCOPED_FILE(f)
    SCOPED_SCHEME_SYMBOL(value, HELLO) {
        SummaSchemeError error = summa_scheme_print(value, f.file);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_FILE_EQ_STR(f, HELLO);
    }
}

void test_scheme_print_vector() {
    SummaSchemeValue values[2] = {
        summa_make_scheme_integer(69),
        summa_make_scheme_integer(420),
    };
    SUMMA_TEST_SCOPED_FILE(f)
    SCOPED_LIST(vector, summa_list_make(values, sizeof(values) / sizeof(values[0]))) {
        SummaSchemeValue value = summa_make_scheme_vector(vector);
        SummaSchemeError error = summa_scheme_print(value, f.file);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_FILE_EQ_STR(f, "#(69 420)");
    }
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.print", argc, argv);

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
