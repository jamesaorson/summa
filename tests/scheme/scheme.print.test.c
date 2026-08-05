#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#define SCOPED_LIST(var, init) SUMMA_TEST_SCOPED_VALUE(SummaList, var, init, summa_scheme_list_release)

/* A procedure's body is the one list inside a value that is not counted, so it
 * is built and released the plain way -- and shallowly, because the values in
 * this one are borrowed from scopes of their own. */
#define SCOPED_BODY(var, init) SUMMA_TEST_SCOPED_VALUE(SummaList, var, init, summa_list_free)

/* A symbol names an interned record the table owns, so the list is all there is
 * to release -- where this used to need a destructor that drained a string out
 * of every element first. */
#define SCOPED_SYMBOL_LIST(var, init) SUMMA_TEST_SCOPED_VALUE(SummaSchemeSymbolList, var, init, summa_symbol_list_free)

/* One destructor for every value type now. A string value used to need its own,
 * reaching a level in to free the SummaString the make macro built; the string
 * is a counted payload and summa_scheme_value_free releases it like any other.
 * A *symbol* value stays a no-op through the same call: its name is interned. */
static void free_scheme_value(SummaSchemeValue value) {
    summa_scheme_value_free(&value);
}

#define SCOPED_SCHEME_STRING(var, cstr) \
    SUMMA_TEST_SCOPED_VALUE(SummaSchemeValue, var, summa_make_scheme_string(cstr), free_scheme_value)

#define SCOPED_SCHEME_SYMBOL(var, cstr) \
    SUMMA_TEST_SCOPED_VALUE(SummaSchemeValue, var, summa_make_scheme_symbol(cstr), free_scheme_value)

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

/* `write` output has to read back as source, so a character carries its `#\`
 * prefix -- the bare glyph is `display`. */
void test_scheme_print_character() {
    for (unsigned int i = 0; i < 128; i++) {
        const char c = (char)i;
        /* The three the reader spells by name are the three whose glyph would
         * not survive the round trip. */
        if (c == ' ' || c == '\n' || c == '\t') {
            continue;
        }
        char expected[4] = {'#', '\\', c, '\0'};
        SUMMA_TEST_SCOPED_FILE(f) {
            const SummaSchemeValue value = summa_make_scheme_character(c);
            const SummaSchemeError error = summa_scheme_print(value, f.file);
            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT_FILE_EQ_STR(f, expected);
        }
    }
}

void test_scheme_print_character_names_the_three_the_reader_knows() {
    const struct {
        char        character;
        const char* written;
    } cases[] = {
        {' ', "#\\space"},
        {'\n', "#\\newline"},
        {'\t', "#\\tab"},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        SUMMA_TEST_SCOPED_FILE(f) {
            const SummaSchemeValue value = summa_make_scheme_character(cases[i].character);
            const SummaSchemeError error = summa_scheme_print(value, f.file);
            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT_FILE_EQ_STR(f, cases[i].written);
        }
    }
}

/* `display` is the other half of the split: the character itself in the
 * stream, so displaying a newline breaks the line rather than spelling it. */
void test_scheme_display_character_is_the_glyph_itself() {
    for (unsigned int i = 0; i < 128; i++) {
        const char c = (char)i;
        SUMMA_TEST_SCOPED_FILE(f) {
            const SummaSchemeValue value = summa_make_scheme_character(c);
            const SummaSchemeError error = summa_scheme_display(value, f.file);
            SUMMA_TEST_ASSERT(!error.had);
            SUMMA_TEST_ASSERT_FILE_EQ_CHAR(f, c);
        }
    }
}

/* A character nested in a container follows the same rule as a string does --
 * the style reaches all the way down rather than stopping at the top level. */
void test_scheme_print_character_inside_a_list() {
    SummaSchemeValue values[2] = {
        summa_make_scheme_character('a'),
        summa_make_scheme_character(' '),
    };
    SUMMA_TEST_SCOPED_FILE(written)
    SUMMA_TEST_SCOPED_FILE(displayed)
    SCOPED_LIST(list, summa_scheme_list_make(values, sizeof(values) / sizeof(values[0]))) {
        const SummaSchemeValue value = summa_make_scheme_list(list);

        SUMMA_TEST_ASSERT(!summa_scheme_print(value, written.file).had);
        SUMMA_TEST_ASSERT_FILE_EQ_STR(written, "(#\\a #\\space)");

        SUMMA_TEST_ASSERT(!summa_scheme_display(value, displayed.file).had);
        SUMMA_TEST_ASSERT_FILE_EQ_STR(displayed, "(a  )");
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
    /* The nested list belongs to the outer one from the moment it is handed
     * over, so it gets no scope of its own -- one owner per payload. */
    SummaSchemeValue values[5] = {
        summa_make_scheme_boolean(true),
        summa_make_scheme_integer(420),
        summa_make_scheme_floating(3.14),
        summa_make_scheme_list(summa_scheme_list_make_empty()),
        summa_make_scheme_boolean(false),
    };
    SUMMA_TEST_SCOPED_FILE(f)
    SCOPED_LIST(list, summa_scheme_list_make(values, sizeof(values) / sizeof(values[0]))) {
        SummaSchemeValue value = summa_make_scheme_list(list);
        SummaSchemeError error = summa_scheme_print(value, f.file);
        SUMMA_TEST_ASSERT(!error.had);
        SUMMA_TEST_ASSERT_FILE_EQ_STR(f, "(#t 420 3.140000 () #f)");
    }
}

void test_scheme_print_procedure() {
    // TODO: Finish
    SCOPED_SYMBOL_LIST(def_bindings, summa_symbol_list_make_empty())
    SCOPED_BODY(def_body, summa_list_make_empty())
    SCOPED_SYMBOL_LIST(body_proc_bindings, summa_symbol_list_make_empty()) {
        SummaSchemeValue def = summa_make_scheme_procedure(summa_scheme_symbol_intern("add2"), def_bindings, def_body);

        summa_symbol_list_push(def_bindings, &(SummaSchemeSymbol){.value = summa_scheme_symbol_intern("x")});
        summa_symbol_list_push(def_bindings, &(SummaSchemeSymbol){.value = summa_scheme_symbol_intern("y")});

        summa_symbol_list_push(body_proc_bindings, &(SummaSchemeSymbol){.value = summa_scheme_symbol_intern("x")});
        summa_symbol_list_push(body_proc_bindings, &(SummaSchemeSymbol){.value = summa_scheme_symbol_intern("y")});
        summa_list_push(def_body,
                        &summa_make_scheme_procedure(summa_scheme_symbol_intern("+"), body_proc_bindings, nullptr));

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
    SCOPED_LIST(vector, summa_scheme_list_make(values, sizeof(values) / sizeof(values[0]))) {
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
    SUMMA_TEST_RUN(test_scheme_print_character_names_the_three_the_reader_knows);
    SUMMA_TEST_RUN(test_scheme_display_character_is_the_glyph_itself);
    SUMMA_TEST_RUN(test_scheme_print_character_inside_a_list);
    SUMMA_TEST_RUN(test_scheme_print_floating);
    SUMMA_TEST_RUN(test_scheme_print_integer);
    SUMMA_TEST_RUN(test_scheme_print_list);
    SUMMA_TEST_RUN(test_scheme_print_procedure);
    SUMMA_TEST_RUN(test_scheme_print_string);
    SUMMA_TEST_RUN(test_scheme_print_symbol);
    SUMMA_TEST_RUN(test_scheme_print_vector);

    return summa_test_end();
}
