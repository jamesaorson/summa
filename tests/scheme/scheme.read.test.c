#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <stdint.h>
#include <string.h>

#define SCOPED_ENV(var)      \
    SUMMA_TEST_SCOPED_VALUE( \
        SummaSchemeEnvironment, var, summa_scheme_environment_make_empty(), summa_scheme_environment_free)

#define SCOPED_GLOBAL_ENV(var) \
    SUMMA_TEST_SCOPED_VALUE(   \
        SummaSchemeEnvironment, var, summa_scheme_environment_make_global(), summa_scheme_environment_free)

/* Reading needs no bindings, so most cases run against an empty environment and
 * `rest` is not interesting. The value comes back through an output parameter,
 * so these release it by hand rather than through a scope. */
static SummaSchemeError read_one(const char* input, SummaSchemeValue* out) {
    SummaSchemeError err = summa_success();
    SCOPED_ENV(env) {
        err = summa_scheme_read(env, input, nullptr, out);
    }
    return err;
}

static void assert_reads_integer(const char* input, int64_t expected) {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = read_one(input, &out);
    SUMMA_TEST_ASSERT(!err.had);
    SUMMA_TEST_ASSERT_EQ(SummaSchemeIntegerType, out.type);
    SUMMA_TEST_ASSERT_EQ(expected, out.value.integer.value);
    summa_scheme_value_free(&out);
}

static void assert_reads_floating(const char* input, double expected) {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = read_one(input, &out);
    SUMMA_TEST_ASSERT(!err.had);
    SUMMA_TEST_ASSERT_EQ(SummaSchemeFloatingType, out.type);
    SUMMA_TEST_ASSERT_EQ(expected, out.value.floating.value);
    summa_scheme_value_free(&out);
}

static void assert_reads_boolean(const char* input, bool expected) {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = read_one(input, &out);
    SUMMA_TEST_ASSERT(!err.had);
    SUMMA_TEST_ASSERT_EQ(SummaSchemeBooleanType, out.type);
    SUMMA_TEST_ASSERT_EQ(expected, out.value.boolean.value);
    summa_scheme_value_free(&out);
}

static void assert_reads_character(const char* input, char expected) {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = read_one(input, &out);
    SUMMA_TEST_ASSERT(!err.had);
    SUMMA_TEST_ASSERT_EQ(SummaSchemeCharacterType, out.type);
    SUMMA_TEST_ASSERT_EQ(expected, out.value.character.value);
    summa_scheme_value_free(&out);
}

static void assert_reads_string(const char* input, const char* expected) {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = read_one(input, &out);
    SUMMA_TEST_ASSERT(!err.had);
    if (err.had) {
        return;
    }
    SUMMA_TEST_ASSERT_EQ(SummaSchemeStringType, out.type);
    if (out.type == SummaSchemeStringType) {
        SUMMA_TEST_ASSERT_EQ_STR(expected, out.value.string.value->value);
    }
    summa_scheme_value_free(&out);
}

static void assert_reads_symbol(const char* input, const char* expected) {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = read_one(input, &out);
    SUMMA_TEST_ASSERT(!err.had);
    if (err.had) {
        return;
    }
    SUMMA_TEST_ASSERT_EQ(SummaSchemeSymbolType, out.type);
    if (out.type == SummaSchemeSymbolType) {
        SUMMA_TEST_ASSERT_EQ_STR(expected, out.value.symbol.value->value);
    }
    summa_scheme_value_free(&out);
}

static void assert_read_fails(const char* input) {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = read_one(input, &out);
    SUMMA_TEST_ASSERT(err.had);
    if (!err.had) {
        summa_scheme_value_free(&out);
    }
}

/* Reads `input` and asserts print renders it back as `expected`. Covers the
 * compound cases, where asserting on shape element by element says less than
 * the round trip does. */
static void assert_reads_and_prints_as(const char* input, const char* expected) {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = read_one(input, &out);
    SUMMA_TEST_ASSERT(!err.had);
    if (err.had) {
        return;
    }
    SUMMA_TEST_SCOPED_FILE(f) {
        const SummaSchemeError print_err = summa_scheme_print(out, f.file);
        SUMMA_TEST_ASSERT(!print_err.had);
        SUMMA_TEST_ASSERT_FILE_EQ_STR(f, expected);
    }
    summa_scheme_value_free(&out);
}

#pragma region atoms

void test_scheme_read_integer() {
    assert_reads_integer("42", 42);
    assert_reads_integer("0", 0);
    assert_reads_integer("-7", -7);
    assert_reads_integer("+3", 3);
}

void test_scheme_read_floating() {
    assert_reads_floating("3.5", 3.5);
    assert_reads_floating("-0.5", -0.5);
    assert_reads_floating("0.0", 0.0);
}

/* The decimal point is what decides the type; a bare digit run stays exact. */
void test_scheme_read_distinguishes_integer_from_floating() {
    assert_reads_integer("42", 42);
    assert_reads_floating("42.0", 42.0);
}

void test_scheme_read_boolean() {
    assert_reads_boolean(SUMMA_SCHEME_TRUE, true);
    assert_reads_boolean(SUMMA_SCHEME_FALSE, false);
}

void test_scheme_read_character() {
    assert_reads_character("#\\a", 'a');
    assert_reads_character("#\\Z", 'Z');
    assert_reads_character("#\\0", '0');
}

/* The named characters, which are the ones that cannot be written literally. */
void test_scheme_read_named_character() {
    assert_reads_character("#\\space", ' ');
    assert_reads_character("#\\newline", '\n');
    assert_reads_character("#\\tab", '\t');
}

void test_scheme_read_string() {
    assert_reads_string("\"hello\"", "hello");
    assert_reads_string("\"\"", "");
    assert_reads_string("\"hello world\"", "hello world");
}

void test_scheme_read_string_escapes() {
    assert_reads_string("\"a\\\"b\"", "a\"b");
    assert_reads_string("\"a\\\\b\"", "a\\b");
    assert_reads_string("\"a\\nb\"", "a\nb");
    assert_reads_string("\"a\\tb\"", "a\tb");
}

void test_scheme_read_symbol() {
    assert_reads_symbol("foo", "foo");
    assert_reads_symbol("hello-world", "hello-world");
    assert_reads_symbol("set!", "set!");
    assert_reads_symbol("let*", "let*");
    assert_reads_symbol("string->symbol", "string->symbol");
}

/* The operators are symbols like any other -- nothing about `+` is special to
 * the reader, only to the environment it is later looked up in. */
void test_scheme_read_operator_symbol() {
    assert_reads_symbol("+", "+");
    assert_reads_symbol("-", "-");
    assert_reads_symbol("<=", "<=");
}

/* A lone sign is a symbol; a sign followed by digits is a number. */
void test_scheme_read_sign_alone_is_a_symbol() {
    assert_reads_symbol("-", "-");
    assert_reads_integer("-1", -1);
}

#pragma endregion atoms

#pragma region compound

void test_scheme_read_empty_list() {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = read_one("()", &out);
    SUMMA_TEST_ASSERT(!err.had);
    SUMMA_TEST_ASSERT_EQ(SummaSchemeListType, out.type);
    if (out.type == SummaSchemeListType) {
        SUMMA_TEST_ASSERT_EQ(0, out.value.list.value->length);
    }
    summa_scheme_value_free(&out);
}

void test_scheme_read_flat_list() {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = read_one("(1 2 3)", &out);
    SUMMA_TEST_ASSERT(!err.had);
    SUMMA_TEST_ASSERT_EQ(SummaSchemeListType, out.type);
    if (out.type == SummaSchemeListType && out.value.list.value->length == 3) {
        SUMMA_TEST_ASSERT_EQ(SummaSchemeIntegerType, out.value.list.value->value[0].type);
        SUMMA_TEST_ASSERT_EQ(1, out.value.list.value->value[0].value.integer.value);
        SUMMA_TEST_ASSERT_EQ(3, out.value.list.value->value[2].value.integer.value);
    } else {
        SUMMA_TEST_ASSERT(false);
    }
    summa_scheme_value_free(&out);
}

void test_scheme_read_nested_list() {
    assert_reads_and_prints_as("(1 (2 3) 4)", "(1 (2 3) 4)");
    assert_reads_and_prints_as("((1))", "((1))");
    assert_reads_and_prints_as("(())", "(())");
}

void test_scheme_read_list_of_mixed_types() {
    assert_reads_and_prints_as("(foo \"bar\" 1 #t)", "(foo \"bar\" 1 #t)");
}

void test_scheme_read_vector() {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = read_one("#(1 2 3)", &out);
    SUMMA_TEST_ASSERT(!err.had);
    SUMMA_TEST_ASSERT_EQ(SummaSchemeVectorType, out.type);
    if (out.type == SummaSchemeVectorType) {
        SUMMA_TEST_ASSERT_EQ(3, out.value.vector.value->length);
    }
    summa_scheme_value_free(&out);
}

void test_scheme_read_empty_vector() {
    SummaSchemeValue       out = {};
    const SummaSchemeError err = read_one("#()", &out);
    SUMMA_TEST_ASSERT(!err.had);
    SUMMA_TEST_ASSERT_EQ(SummaSchemeVectorType, out.type);
    if (out.type == SummaSchemeVectorType) {
        SUMMA_TEST_ASSERT_EQ(0, out.value.vector.value->length);
    }
    summa_scheme_value_free(&out);
}

/* `'x` is read as `(quote x)` -- the shorthand is expanded here rather than
 * being a value type of its own, so the evaluator only ever sees the form. */
void test_scheme_read_quote_shorthand() {
    assert_reads_and_prints_as("'x", "(quote x)");
    assert_reads_and_prints_as("'(1 2)", "(quote (1 2))");
    assert_reads_and_prints_as("''x", "(quote (quote x))");
}

#pragma endregion compound

#pragma region whitespace and comments

void test_scheme_read_skips_leading_whitespace() {
    assert_reads_integer("   42", 42);
    assert_reads_integer("\t42", 42);
    assert_reads_integer("\n\n42", 42);
}

void test_scheme_read_allows_whitespace_inside_a_list() {
    assert_reads_and_prints_as("(  1   2  )", "(1 2)");
    assert_reads_and_prints_as("(1\n 2\n 3)", "(1 2 3)");
}

void test_scheme_read_skips_line_comments() {
    assert_reads_integer("; a comment\n42", 42);
    assert_reads_integer("   ; indented\n   42", 42);
}

void test_scheme_read_skips_comments_inside_a_list() {
    assert_reads_and_prints_as("(1 ; two is missing\n 3)", "(1 3)");
}

#pragma endregion whitespace and comments

#pragma region rest

/* The reason `rest` exists: more than one datum on a line. */
void test_scheme_read_rest_points_at_the_next_datum() {
    SCOPED_ENV(env) {
        const char*      input = "(+ 1 2) (+ 3 4)";
        const char*      rest  = nullptr;
        SummaSchemeValue out   = {};

        const SummaSchemeError err = summa_scheme_read(env, input, &rest, &out);
        SUMMA_TEST_ASSERT(!err.had);
        SUMMA_TEST_ASSERT_NOT_NULL(rest);
        if (rest) {
            SUMMA_TEST_ASSERT_EQ_STR("(+ 3 4)", rest);
        }
        summa_scheme_value_free(&out);
    }
}

/* Trailing whitespace is consumed along with the datum, so `rest` lands on the
 * NUL and `while (*cursor)` terminates without a second call. */
void test_scheme_read_rest_lands_on_the_terminator() {
    SCOPED_ENV(env) {
        const char*      rest = nullptr;
        SummaSchemeValue out  = {};

        const SummaSchemeError err = summa_scheme_read(env, "42", &rest, &out);
        SUMMA_TEST_ASSERT(!err.had);
        SUMMA_TEST_ASSERT_NOT_NULL(rest);
        if (rest) {
            SUMMA_TEST_ASSERT_EQ('\0', *rest);
        }
        summa_scheme_value_free(&out);
    }
}

void test_scheme_read_rest_consumes_trailing_whitespace() {
    SCOPED_ENV(env) {
        const char*      rest = nullptr;
        SummaSchemeValue out  = {};

        const SummaSchemeError err = summa_scheme_read(env, "42   \n  ", &rest, &out);
        SUMMA_TEST_ASSERT(!err.had);
        SUMMA_TEST_ASSERT_NOT_NULL(rest);
        if (rest) {
            SUMMA_TEST_ASSERT_EQ('\0', *rest);
        }
        summa_scheme_value_free(&out);
    }
}

void test_scheme_read_rest_consumes_a_trailing_comment() {
    SCOPED_ENV(env) {
        const char*      rest = nullptr;
        SummaSchemeValue out  = {};

        const SummaSchemeError err = summa_scheme_read(env, "42 ; trailing", &rest, &out);
        SUMMA_TEST_ASSERT(!err.had);
        SUMMA_TEST_ASSERT_NOT_NULL(rest);
        if (rest) {
            SUMMA_TEST_ASSERT_EQ('\0', *rest);
        }
        summa_scheme_value_free(&out);
    }
}

/* The driver loop a REPL will actually run. */
void test_scheme_read_drives_a_loop_over_every_datum() {
    SCOPED_ENV(env) {
        const char* cursor = "1 2 3";
        int64_t     sum    = 0;
        size_t      count  = 0;

        while (*cursor && count < 8) {
            SummaSchemeValue       out = {};
            const SummaSchemeError err = summa_scheme_read(env, cursor, &cursor, &out);
            SUMMA_TEST_ASSERT(!err.had);
            if (err.had) {
                break;
            }
            SUMMA_TEST_ASSERT_EQ(SummaSchemeIntegerType, out.type);
            if (out.type == SummaSchemeIntegerType) {
                sum += out.value.integer.value;
            }
            count++;
            summa_scheme_value_free(&out);
        }

        SUMMA_TEST_ASSERT_EQ(3, count);
        SUMMA_TEST_ASSERT_EQ(6, sum);
    }
}

void test_scheme_read_rest_may_be_null() {
    SCOPED_ENV(env) {
        SummaSchemeValue       out = {};
        const SummaSchemeError err = summa_scheme_read(env, "(1 2) (3 4)", nullptr, &out);
        SUMMA_TEST_ASSERT(!err.had);
        SUMMA_TEST_ASSERT_EQ(SummaSchemeListType, out.type);
        summa_scheme_value_free(&out);
    }
}

/* An error leaves `rest` alone, so a caller cannot mistake a failed read for
 * progress and loop forever. */
void test_scheme_read_rest_is_untouched_on_error() {
    SCOPED_ENV(env) {
        const char* const sentinel = "unchanged";
        const char*       rest     = sentinel;
        SummaSchemeValue  out      = {};

        const SummaSchemeError err = summa_scheme_read(env, "(1 2", &rest, &out);
        SUMMA_TEST_ASSERT(err.had);
        SUMMA_TEST_ASSERT_EQ(sentinel, rest);
    }
}

#pragma endregion rest

#pragma region errors

void test_scheme_read_rejects_empty_input() {
    assert_read_fails("");
}

void test_scheme_read_rejects_whitespace_only_input() {
    assert_read_fails("   ");
    assert_read_fails("\n\t ");
}

void test_scheme_read_rejects_comment_only_input() {
    assert_read_fails("; nothing here");
}

void test_scheme_read_rejects_unbalanced_open_paren() {
    assert_read_fails("(1 2");
    assert_read_fails("(1 (2)");
    assert_read_fails("(");
}

void test_scheme_read_rejects_unbalanced_close_paren() {
    assert_read_fails(")");
    assert_read_fails("(1))");
}

void test_scheme_read_rejects_unterminated_string() {
    assert_read_fails("\"abc");
    assert_read_fails("\"");
}

void test_scheme_read_rejects_unterminated_vector() {
    assert_read_fails("#(1 2");
}

void test_scheme_read_rejects_a_dangling_quote() {
    assert_read_fails("'");
}

#pragma endregion errors

#pragma region round trips

/* What the REPL is: read, evaluate, print. */
void test_scheme_read_result_evaluates() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       form = {};
        const SummaSchemeError err  = summa_scheme_read(env, "(+ 1 2)", nullptr, &form);
        SUMMA_TEST_ASSERT(!err.had);

        /* Guarded rather than returned early: a return here would leave the
         * scope without freeing the environment. */
        if (!err.had) {
            SummaSchemeValue       result   = {};
            const SummaSchemeError eval_err = summa_scheme_evaluate(env, form, &result);
            SUMMA_TEST_ASSERT(!eval_err.had);
            SUMMA_TEST_ASSERT_EQ(SummaSchemeIntegerType, result.type);
            SUMMA_TEST_ASSERT_EQ(3, result.value.integer.value);

            summa_scheme_value_free(&result);
            summa_scheme_value_free(&form);
        }
    }
}

void test_scheme_read_result_evaluates_a_definition_and_a_call() {
    SCOPED_GLOBAL_ENV(env) {
        const char* cursor = "(define (inc n) (+ n 1)) (inc 41)";
        int64_t     last   = 0;

        while (*cursor) {
            SummaSchemeValue       form = {};
            const SummaSchemeError err  = summa_scheme_read(env, cursor, &cursor, &form);
            SUMMA_TEST_ASSERT(!err.had);
            if (err.had) {
                break;
            }

            SummaSchemeValue       result   = {};
            const SummaSchemeError eval_err = summa_scheme_evaluate(env, form, &result);
            SUMMA_TEST_ASSERT(!eval_err.had);
            if (!eval_err.had && result.type == SummaSchemeIntegerType) {
                last = result.value.integer.value;
            }

            summa_scheme_value_free(&result);
            summa_scheme_value_free(&form);
        }

        SUMMA_TEST_ASSERT_EQ(42, last);
    }
}

/* Reading and evaluating fail independently. `(1 2 3)` is a perfectly good
 * datum and a bad expression, so the reader must accept it and leave the
 * complaint to evaluation:
 *
 *     scheme@(guile-user)> (1 2 3)
 *     Wrong type to apply: 1
 */
void test_scheme_read_accepts_what_evaluation_rejects() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       form = {};
        const SummaSchemeError err  = summa_scheme_read(env, "(1 2 3)", nullptr, &form);
        SUMMA_TEST_ASSERT(!err.had);

        if (!err.had) {
            SUMMA_TEST_ASSERT_EQ(SummaSchemeListType, form.type);

            SummaSchemeValue       result   = {};
            const SummaSchemeError eval_err = summa_scheme_evaluate(env, form, &result);
            SUMMA_TEST_ASSERT(eval_err.had);
            SUMMA_TEST_ASSERT_EQ_STR("Wrong type to apply: 1", eval_err.message);

            summa_scheme_value_free(&result);
            summa_scheme_value_free(&form);
        }
    }
}

/* Quoting is what turns the same text into data. */
void test_scheme_read_quoted_list_evaluates_to_itself() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       form = {};
        const SummaSchemeError err  = summa_scheme_read(env, "'(1 2 3)", nullptr, &form);
        SUMMA_TEST_ASSERT(!err.had);

        if (!err.had) {
            SummaSchemeValue       result   = {};
            const SummaSchemeError eval_err = summa_scheme_evaluate(env, form, &result);
            SUMMA_TEST_ASSERT(!eval_err.had);
            SUMMA_TEST_ASSERT_EQ(SummaSchemeListType, result.type);
            if (result.type == SummaSchemeListType) {
                SUMMA_TEST_ASSERT_EQ(3, result.value.list.value->length);
            }

            summa_scheme_value_free(&result);
            summa_scheme_value_free(&form);
        }
    }
}

/* Printing what was read gives back the source, which is the property that
 * catches a reader quietly dropping or reordering structure. */
void test_scheme_read_and_print_round_trip() {
    assert_reads_and_prints_as("(define (add2 x y) (+ x y))", "(define (add2 x y) (+ x y))");
    assert_reads_and_prints_as("(if #t 1 2)", "(if #t 1 2)");
    assert_reads_and_prints_as("#(1 (2 3))", "#(1 (2 3))");
}

#pragma endregion round trips

int main(int argc, char** argv) {
    summa_test_begin("scheme.read", argc, argv);

    SUMMA_TEST_RUN(test_scheme_read_integer);
    SUMMA_TEST_RUN(test_scheme_read_floating);
    SUMMA_TEST_RUN(test_scheme_read_distinguishes_integer_from_floating);
    SUMMA_TEST_RUN(test_scheme_read_boolean);
    SUMMA_TEST_RUN(test_scheme_read_character);
    SUMMA_TEST_RUN(test_scheme_read_named_character);
    SUMMA_TEST_RUN(test_scheme_read_string);
    SUMMA_TEST_RUN(test_scheme_read_string_escapes);
    SUMMA_TEST_RUN(test_scheme_read_symbol);
    SUMMA_TEST_RUN(test_scheme_read_operator_symbol);
    SUMMA_TEST_RUN(test_scheme_read_sign_alone_is_a_symbol);

    SUMMA_TEST_RUN(test_scheme_read_empty_list);
    SUMMA_TEST_RUN(test_scheme_read_flat_list);
    SUMMA_TEST_RUN(test_scheme_read_nested_list);
    SUMMA_TEST_RUN(test_scheme_read_list_of_mixed_types);
    SUMMA_TEST_RUN(test_scheme_read_vector);
    SUMMA_TEST_RUN(test_scheme_read_empty_vector);
    SUMMA_TEST_RUN(test_scheme_read_quote_shorthand);

    SUMMA_TEST_RUN(test_scheme_read_skips_leading_whitespace);
    SUMMA_TEST_RUN(test_scheme_read_allows_whitespace_inside_a_list);
    SUMMA_TEST_RUN(test_scheme_read_skips_line_comments);
    SUMMA_TEST_RUN(test_scheme_read_skips_comments_inside_a_list);

    SUMMA_TEST_RUN(test_scheme_read_rest_points_at_the_next_datum);
    SUMMA_TEST_RUN(test_scheme_read_rest_lands_on_the_terminator);
    SUMMA_TEST_RUN(test_scheme_read_rest_consumes_trailing_whitespace);
    SUMMA_TEST_RUN(test_scheme_read_rest_consumes_a_trailing_comment);
    SUMMA_TEST_RUN(test_scheme_read_drives_a_loop_over_every_datum);
    SUMMA_TEST_RUN(test_scheme_read_rest_may_be_null);
    SUMMA_TEST_RUN(test_scheme_read_rest_is_untouched_on_error);

    SUMMA_TEST_RUN(test_scheme_read_rejects_empty_input);
    SUMMA_TEST_RUN(test_scheme_read_rejects_whitespace_only_input);
    SUMMA_TEST_RUN(test_scheme_read_rejects_comment_only_input);
    SUMMA_TEST_RUN(test_scheme_read_rejects_unbalanced_open_paren);
    SUMMA_TEST_RUN(test_scheme_read_rejects_unbalanced_close_paren);
    SUMMA_TEST_RUN(test_scheme_read_rejects_unterminated_string);
    SUMMA_TEST_RUN(test_scheme_read_rejects_unterminated_vector);
    SUMMA_TEST_RUN(test_scheme_read_rejects_a_dangling_quote);

    SUMMA_TEST_RUN(test_scheme_read_result_evaluates);
    SUMMA_TEST_RUN(test_scheme_read_result_evaluates_a_definition_and_a_call);
    SUMMA_TEST_RUN(test_scheme_read_accepts_what_evaluation_rejects);
    SUMMA_TEST_RUN(test_scheme_read_quoted_list_evaluates_to_itself);
    SUMMA_TEST_RUN(test_scheme_read_and_print_round_trip);

    return summa_test_end();
}
