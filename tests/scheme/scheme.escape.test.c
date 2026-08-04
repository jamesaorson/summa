#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <stddef.h>

/* `summa_scheme_read_escape` translates one escape sequence in isolation, so
 * these go straight at it -- no string literal, no reader state. Each input
 * starts at the backslash; `rest` is checked alongside the byte, since the
 * reader relies on it to resume at the right place. */

static void assert_escapes_to(const char* input, char expected, size_t expected_consumed) {
    char                   out  = '\0';
    const char*            rest = nullptr;
    const SummaSchemeError err  = summa_scheme_read_escape(input, &rest, &out);
    SUMMA_TEST_ASSERT(!err.had);
    if (err.had) {
        return;
    }
    SUMMA_TEST_ASSERT_EQ(expected, out);
    SUMMA_TEST_ASSERT_NOT_NULL(rest);
    SUMMA_TEST_ASSERT_EQ(expected_consumed, (size_t)(rest - input));
}

static void assert_escape_fails(const char* input) {
    char                   out  = '\0';
    const char*            rest = nullptr;
    const SummaSchemeError err  = summa_scheme_read_escape(input, &rest, &out);
    SUMMA_TEST_ASSERT(err.had);
    /* `rest` is documented as untouched on error, so a caller that ignores the
     * error at least does not resume from a garbage position. */
    SUMMA_TEST_ASSERT_NULL(rest);
}

#pragma region named escapes

void test_scheme_escape_alarm() {
    assert_escapes_to("\\a", '\a', 2);
}

void test_scheme_escape_backspace() {
    assert_escapes_to("\\b", '\b', 2);
}

void test_scheme_escape_tab() {
    assert_escapes_to("\\t", '\t', 2);
}

void test_scheme_escape_newline() {
    assert_escapes_to("\\n", '\n', 2);
}

void test_scheme_escape_carriage_return() {
    assert_escapes_to("\\r", '\r', 2);
}

#pragma endregion named escapes

#pragma region literal escapes

void test_scheme_escape_double_quote() {
    assert_escapes_to("\\\"", '"', 2);
}

void test_scheme_escape_backslash() {
    assert_escapes_to("\\\\", '\\', 2);
}

void test_scheme_escape_vertical_bar() {
    assert_escapes_to("\\|", '|', 2);
}

#pragma endregion literal escapes

#pragma region hex escapes

void test_scheme_escape_hex_one_digit() {
    assert_escapes_to("\\x9;", '\t', 4);
}

void test_scheme_escape_hex_two_digits() {
    assert_escapes_to("\\x41;", 'A', 5);
}

void test_scheme_escape_hex_uppercase_digits() {
    assert_escapes_to("\\x7A;", 'z', 5);
}

void test_scheme_escape_hex_lowercase_digits() {
    assert_escapes_to("\\x7a;", 'z', 5);
}

void test_scheme_escape_hex_leading_zeros() {
    assert_escapes_to("\\x00000041;", 'A', 11);
}

void test_scheme_escape_hex_high_byte() {
    assert_escapes_to("\\xFF;", (char)0xFF, 5);
}

void test_scheme_escape_rejects_hex_without_digits() {
    assert_escape_fails("\\x;");
}

void test_scheme_escape_rejects_hex_without_terminator() {
    assert_escape_fails("\\x41");
    assert_escape_fails("\\x41z");
}

void test_scheme_escape_rejects_hex_above_a_byte() {
    assert_escape_fails("\\x100;");
    assert_escape_fails("\\x3BB;");
}

/* A NUL byte would truncate the C string the token is built on, so it is
 * rejected rather than silently swallowed. */
void test_scheme_escape_rejects_hex_null() {
    assert_escape_fails("\\x0;");
    assert_escape_fails("\\x00;");
}

#pragma endregion hex escapes

#pragma region errors

void test_scheme_escape_rejects_unknown_escape() {
    assert_escape_fails("\\q");
    assert_escape_fails("\\1");
    assert_escape_fails("\\ ");
}

void test_scheme_escape_rejects_a_dangling_backslash() {
    assert_escape_fails("\\");
}

/* Line continuation is real R7RS, but it spans a newline and yields no byte at
 * all, so it does not fit this function's contract. Rejected for now. */
void test_scheme_escape_rejects_line_continuation() {
    assert_escape_fails("\\\n   ");
}

void test_scheme_escape_rejects_input_that_is_not_an_escape() {
    assert_escape_fails("a");
    assert_escape_fails("");
}

#pragma endregion errors

#pragma region contract

/* The reader hands in a pointer into the middle of a string literal, so what
 * follows the escape has to be left alone. */
void test_scheme_escape_stops_at_the_end_of_the_sequence() {
    assert_escapes_to("\\nbc", '\n', 2);
    assert_escapes_to("\\x41;bc", 'A', 5);
}

void test_scheme_escape_rest_may_be_null() {
    char                   out = '\0';
    const SummaSchemeError err = summa_scheme_read_escape("\\t", nullptr, &out);
    SUMMA_TEST_ASSERT(!err.had);
    SUMMA_TEST_ASSERT_EQ('\t', out);
}

#pragma endregion contract

int main(int argc, char** argv) {
    summa_test_begin("scheme.escape", argc, argv);

    SUMMA_TEST_RUN(test_scheme_escape_alarm);
    SUMMA_TEST_RUN(test_scheme_escape_backspace);
    SUMMA_TEST_RUN(test_scheme_escape_tab);
    SUMMA_TEST_RUN(test_scheme_escape_newline);
    SUMMA_TEST_RUN(test_scheme_escape_carriage_return);

    SUMMA_TEST_RUN(test_scheme_escape_double_quote);
    SUMMA_TEST_RUN(test_scheme_escape_backslash);
    SUMMA_TEST_RUN(test_scheme_escape_vertical_bar);

    SUMMA_TEST_RUN(test_scheme_escape_hex_one_digit);
    SUMMA_TEST_RUN(test_scheme_escape_hex_two_digits);
    SUMMA_TEST_RUN(test_scheme_escape_hex_uppercase_digits);
    SUMMA_TEST_RUN(test_scheme_escape_hex_lowercase_digits);
    SUMMA_TEST_RUN(test_scheme_escape_hex_leading_zeros);
    SUMMA_TEST_RUN(test_scheme_escape_hex_high_byte);
    SUMMA_TEST_RUN(test_scheme_escape_rejects_hex_without_digits);
    SUMMA_TEST_RUN(test_scheme_escape_rejects_hex_without_terminator);
    SUMMA_TEST_RUN(test_scheme_escape_rejects_hex_above_a_byte);
    SUMMA_TEST_RUN(test_scheme_escape_rejects_hex_null);

    SUMMA_TEST_RUN(test_scheme_escape_rejects_unknown_escape);
    SUMMA_TEST_RUN(test_scheme_escape_rejects_a_dangling_backslash);
    SUMMA_TEST_RUN(test_scheme_escape_rejects_line_continuation);
    SUMMA_TEST_RUN(test_scheme_escape_rejects_input_that_is_not_an_escape);

    SUMMA_TEST_RUN(test_scheme_escape_stops_at_the_end_of_the_sequence);
    SUMMA_TEST_RUN(test_scheme_escape_rest_may_be_null);

    return summa_test_end();
}
