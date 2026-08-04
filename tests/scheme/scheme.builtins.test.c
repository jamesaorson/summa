#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <string.h>

/* The builtin procedure table, one suite per row.
 *
 * Driven through read-evaluate-print the way the e2e suite is, because that is
 * how a builtin is actually reached: `procedure_dispatch_global` finds it by
 * the name the reader produced. Calling the static functions directly would
 * skip the registration this is half checking.
 *
 * Where the e2e suite proves the pipeline runs, this one pins what each
 * procedure computes -- the sign conventions, the arity rules, and the errors,
 * which are the parts that are easy to get subtly wrong and impossible to
 * notice from a passing euler run. */

#define SCOPED_GLOBAL_ENV(var) \
    SUMMA_TEST_SCOPED_VALUE(   \
        SummaSchemeEnvironment, var, summa_scheme_environment_make_global(), summa_scheme_environment_free)

/* Read, evaluate, discard, repeat -- the REPL loop, stopping at the first
 * error. `out` holds the last expression's value. */
static SummaSchemeError run_program(const SummaSchemeEnvironment env, const char* program, SummaSchemeValue* out) {
    SummaSchemeError err    = summa_success();
    const char*      cursor = program;

    while (*cursor) {
        SummaSchemeValue form = {};

        err = summa_scheme_read(env, cursor, &cursor, &form);
        if (err.had) {
            break;
        }

        summa_scheme_value_free(out);
        *out = (SummaSchemeValue){};
        err  = summa_scheme_evaluate(env, form, out);
        summa_scheme_value_free(&form);
        if (err.had) {
            break;
        }
    }

    return err;
}

/* Rendering goes through `print`, so a result reads back the way a REPL echoes
 * it. Any allocation the run made is released by the environment scope and the
 * explicit value free -- the result is filled in through an output parameter,
 * so it is not the scope's to destroy. */
static void assert_prints(const char* program, const char* expected) {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       result = {};
        const SummaSchemeError err    = run_program(env, program, &result);

        SUMMA_TEST_ASSERT_MSG(!err.had, err.message);
        if (!err.had) {
            SUMMA_TEST_SCOPED_FILE(f) {
                const SummaSchemeError print_err = summa_scheme_print(result, f.file);
                SUMMA_TEST_ASSERT(!print_err.had);
                SUMMA_TEST_ASSERT_FILE_EQ_STR(f, expected);
            }
        }
        summa_scheme_value_free(&result);
    }
}

/* The message is the whole product of an argument check, so it is pinned
 * rather than just checked for failing. */
static void assert_fails_with(const char* program, const char* expected_message) {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       result = {};
        const SummaSchemeError err    = run_program(env, program, &result);

        SUMMA_TEST_ASSERT(err.had);
        if (err.had) {
            SUMMA_TEST_ASSERT_EQ_STR(expected_message, err.message);
        }
        summa_scheme_value_free(&result);
    }
}

#pragma region subtraction

/* One operand negates; that is the case a left fold alone would get wrong. */
void test_scheme_builtins_subtract_negates_a_single_argument() {
    assert_prints("(- 5)", "-5");
    assert_prints("(- -5)", "5");
    assert_prints("(- 0)", "0");
}

void test_scheme_builtins_subtract_folds_left() {
    assert_prints("(- 10 3)", "7");
    assert_prints("(- 10 3 2)", "5");
    assert_prints("(- 1 2 3 4)", "-8");
}

/* One inexact operand makes the result inexact, negation included. */
void test_scheme_builtins_subtract_mixes_integers_and_floats() {
    assert_prints("(- 2.5)", "-2.500000");
    assert_prints("(- 10 2.5)", "7.500000");
}

/* No identity to hand back, so R7RS makes it an error rather than 0. */
void test_scheme_builtins_subtract_rejects_no_arguments() {
    assert_fails_with("(-)", "- - expects at least 1 argument(s), got 0");
}

void test_scheme_builtins_subtract_rejects_non_numbers() {
    assert_fails_with("(- 1 \"x\")", "- - expects numeric arguments");
    assert_fails_with("(- #t)", "- - expects numeric arguments");
}

#pragma endregion subtraction

#pragma region multiplication

void test_scheme_builtins_multiply_folds_left() {
    assert_prints("(* 6 7)", "42");
    assert_prints("(* 2 3 7)", "42");
    assert_prints("(* -2 3)", "-6");
}

/* The identity, which is what makes an empty product 1 rather than an error. */
void test_scheme_builtins_multiply_of_nothing_is_one() {
    assert_prints("(*)", "1");
    assert_prints("(* 7)", "7");
}

void test_scheme_builtins_multiply_mixes_integers_and_floats() {
    assert_prints("(* 2 1.5)", "3.000000");
}

void test_scheme_builtins_multiply_rejects_non_numbers() {
    assert_fails_with("(* 2 \"x\")", "* - expects numeric arguments");
}

#pragma endregion multiplication

#pragma region comparison

void test_scheme_builtins_numeric_equal_compares_a_pair() {
    assert_prints("(= 1 1)", "#t");
    assert_prints("(= 1 2)", "#f");
}

void test_scheme_builtins_less_and_greater_compare_a_pair() {
    assert_prints("(< 1 2)", "#t");
    assert_prints("(< 2 1)", "#f");
    assert_prints("(< 1 1)", "#f");
    assert_prints("(> 2 1)", "#t");
    assert_prints("(> 1 2)", "#f");
    assert_prints("(> 1 1)", "#f");
}

void test_scheme_builtins_less_equal_and_greater_equal_include_equality() {
    assert_prints("(<= 1 1)", "#t");
    assert_prints("(<= 1 2)", "#t");
    assert_prints("(<= 2 1)", "#f");
    assert_prints("(>= 1 1)", "#t");
    assert_prints("(>= 2 1)", "#t");
    assert_prints("(>= 1 2)", "#f");
}

/* R7RS chaining: every adjacent pair has to hold, not just the ends. */
void test_scheme_builtins_comparisons_chain() {
    assert_prints("(< 1 2 3)", "#t");
    assert_prints("(< 1 3 2)", "#f");
    assert_prints("(= 1 1 1)", "#t");
    assert_prints("(= 1 1 2)", "#f");
    assert_prints("(>= 3 3 1)", "#t");
    /* Ends ordered, middle not -- what a first-versus-last check would miss. */
    assert_prints("(> 3 1 2)", "#f");
}

void test_scheme_builtins_comparisons_mix_integers_and_floats() {
    assert_prints("(< 1 1.5)", "#t");
    assert_prints("(= 1 1.0)", "#t");
    assert_prints("(> 2.5 2)", "#t");
}

/* Integers are compared as integers, so a pair a double cannot tell apart
 * still orders. */
void test_scheme_builtins_comparisons_do_not_lose_integer_precision() {
    assert_prints("(= 9007199254740993 9007199254740992)", "#f");
    assert_prints("(< 9007199254740992 9007199254740993)", "#t");
}

/* One operand is vacuously true and almost certainly a typo. */
void test_scheme_builtins_comparisons_require_two_arguments() {
    assert_fails_with("(<)", "< - expects at least 2 argument(s), got 0");
    assert_fails_with("(= 1)", "= - expects at least 2 argument(s), got 1");
    assert_fails_with("(>= 1)", ">= - expects at least 2 argument(s), got 1");
}

void test_scheme_builtins_comparisons_reject_non_numbers() {
    assert_fails_with("(< 1 \"x\")", "< - expects numeric arguments");
    assert_fails_with("(= #t #t)", "= - expects numeric arguments");
}

#pragma endregion comparison

#pragma region integer division

void test_scheme_builtins_quotient_truncates_toward_zero() {
    assert_prints("(quotient 7 2)", "3");
    assert_prints("(quotient -7 2)", "-3");
    assert_prints("(quotient 7 -2)", "-3");
    assert_prints("(quotient -7 -2)", "3");
}

/* `remainder` truncates with the quotient, so its sign follows the dividend. */
void test_scheme_builtins_remainder_takes_the_sign_of_the_dividend() {
    assert_prints("(remainder 7 2)", "1");
    assert_prints("(remainder -7 2)", "-1");
    assert_prints("(remainder 7 -2)", "1");
    assert_prints("(remainder -7 -2)", "-1");
}

/* `modulo` takes the divisor's sign instead -- the whole difference between
 * the two, and the reason euler's `(= 0 (modulo n f))` tests work either way. */
void test_scheme_builtins_modulo_takes_the_sign_of_the_divisor() {
    assert_prints("(modulo 7 2)", "1");
    assert_prints("(modulo -7 2)", "1");
    assert_prints("(modulo 7 -2)", "-1");
    assert_prints("(modulo -7 -2)", "-1");
}

void test_scheme_builtins_modulo_and_remainder_agree_when_evenly_divided() {
    assert_prints("(modulo 6 3)", "0");
    assert_prints("(remainder 6 3)", "0");
    assert_prints("(modulo -6 3)", "0");
    assert_prints("(remainder -6 3)", "0");
}

void test_scheme_builtins_integer_division_takes_exactly_two_arguments() {
    assert_fails_with("(quotient 7)", "quotient - expects 2 argument(s), got 1");
    assert_fails_with("(remainder 7 2 1)", "remainder - expects 2 argument(s), got 3");
    assert_fails_with("(modulo)", "modulo - expects 2 argument(s), got 0");
}

/* Floats are rejected outright rather than truncated: there is no exactness
 * tracking to tell an integral float from one that rounded to look integral. */
void test_scheme_builtins_integer_division_rejects_floats() {
    assert_fails_with("(quotient 7.0 2)", "quotient - argument 1 must be integer, got floating");
    assert_fails_with("(modulo 7 2.0)", "modulo - argument 2 must be integer, got floating");
    assert_fails_with("(remainder \"x\" 2)", "remainder - argument 1 must be integer, got string");
}

void test_scheme_builtins_integer_division_by_zero_is_an_error() {
    assert_fails_with("(quotient 1 0)", "quotient - division by zero");
    assert_fails_with("(remainder 1 0)", "remainder - division by zero");
    assert_fails_with("(modulo 1 0)", "modulo - division by zero");
}

/* INT64_MIN / -1 is +2^63, which no int64_t holds -- undefined behavior in C
 * and a UBSan trap in CI, so it has to be caught before the divide. */
void test_scheme_builtins_integer_division_overflow_is_an_error() {
    assert_fails_with("(quotient -9223372036854775808 -1)", "quotient - result is not representable");
    assert_fails_with("(remainder -9223372036854775808 -1)", "remainder - result is not representable");
    assert_fails_with("(modulo -9223372036854775808 -1)", "modulo - result is not representable");
    /* The same dividend with any other divisor is ordinary. */
    assert_prints("(quotient -9223372036854775808 2)", "-4611686018427387904");
}

#pragma endregion integer division

#pragma region predicates

void test_scheme_builtins_zero_tests_a_number() {
    assert_prints("(zero? 0)", "#t");
    assert_prints("(zero? 1)", "#f");
    assert_prints("(zero? -1)", "#f");
    assert_prints("(zero? 0.0)", "#t");
    assert_prints("(zero? 0.5)", "#f");
}

void test_scheme_builtins_zero_takes_one_number() {
    assert_fails_with("(zero?)", "zero? - expects 1 argument(s), got 0");
    assert_fails_with("(zero? 1 2)", "zero? - expects 1 argument(s), got 2");
    assert_fails_with("(zero? \"x\")", "zero? - expects numeric arguments");
}

/* `#f` alone is false, so everything else -- zero and the empty string
 * included -- makes `not` give back #f. */
void test_scheme_builtins_not_inverts_truthiness() {
    assert_prints("(not #f)", "#t");
    assert_prints("(not #t)", "#f");
    assert_prints("(not 0)", "#f");
    assert_prints("(not \"\")", "#f");
    assert_prints("(not '(1 2))", "#f");
}

void test_scheme_builtins_not_takes_exactly_one_argument() {
    assert_fails_with("(not)", "not - expects 1 argument(s), got 0");
    assert_fails_with("(not #f #f)", "not - expects 1 argument(s), got 2");
}

#pragma endregion predicates

#pragma region strings and characters

void test_scheme_builtins_string_length_counts_bytes() {
    assert_prints("(string-length \"\")", "0");
    assert_prints("(string-length \"hello\")", "5");
    /* The escape is one byte by the time evaluation sees it. */
    assert_prints("(string-length \"a\\tb\")", "3");
}

void test_scheme_builtins_string_length_takes_one_string() {
    assert_fails_with("(string-length)", "string-length - expects 1 argument(s), got 0");
    assert_fails_with("(string-length 5)", "string-length - argument 1 must be string, got integer");
}

void test_scheme_builtins_string_ref_indexes_from_zero() {
    /* Printed the `write` way, so a character comes back with its `#\` prefix. */
    assert_prints("(string-ref \"hello\" 0)", "#\\h");
    assert_prints("(string-ref \"hello\" 4)", "#\\o");
}

/* The message carries both halves of the mistake: what was asked for, and what
 * the string could have answered. */
void test_scheme_builtins_string_ref_rejects_an_out_of_range_index() {
    assert_fails_with("(string-ref \"abc\" 3)", "string-ref - index 3 is out of range for a string of length 3");
    assert_fails_with("(string-ref \"abc\" -1)", "string-ref - index -1 is out of range for a string of length 3");
    assert_fails_with("(string-ref \"\" 0)", "string-ref - index 0 is out of range for a string of length 0");
}

void test_scheme_builtins_string_ref_checks_both_argument_types() {
    assert_fails_with("(string-ref \"abc\")", "string-ref - expects 2 argument(s), got 1");
    assert_fails_with("(string-ref 1 0)", "string-ref - argument 1 must be string, got integer");
    assert_fails_with("(string-ref \"abc\" \"0\")", "string-ref - argument 2 must be integer, got string");
}

void test_scheme_builtins_char_to_integer_gives_the_code_point() {
    assert_prints("(char->integer #\\a)", "97");
    assert_prints("(char->integer #\\0)", "48");
    assert_prints("(char->integer #\\space)", "32");
    assert_prints("(char->integer #\\newline)", "10");
}

void test_scheme_builtins_char_to_integer_takes_one_character() {
    assert_fails_with("(char->integer)", "char->integer - expects 1 argument(s), got 0");
    assert_fails_with("(char->integer \"a\")", "char->integer - argument 1 must be character, got string");
}

/* The two together are how a digit is read out of a string, which is the only
 * thing euler problem 8 wants them for. */
void test_scheme_builtins_string_and_char_procedures_compose() {
    assert_prints("(define (digit-at s i) (- (char->integer (string-ref s i)) 48))"
                  "(digit-at \"731\" 0)",
                  "7");
    assert_prints("(define s \"731\")"
                  "(+ (- (char->integer (string-ref s 1)) 48)"
                  "   (- (char->integer (string-ref s 2)) 48))",
                  "4");
}

#pragma endregion strings and characters

#pragma region pairs and lists

/* Prepending, which for a dynamic array means a fresh list of length n+1. */
void test_scheme_builtins_cons_prepends_to_a_list() {
    assert_prints("(cons 1 '(2 3))", "(1 2 3)");
    assert_prints("(cons 1 '())", "(1)");
    assert_prints("(cons \"a\" '(\"b\"))", "(\"a\" \"b\")");
}

/* A list head nests rather than splices -- `(cons '(1) '(2))` is a two-element
 * list whose first element is itself a list, not `(1 2)`. */
void test_scheme_builtins_cons_nests_a_list_head() {
    assert_prints("(cons '(1) '(2))", "((1) 2)");
    assert_prints("(cons '() '())", "(())");
}

/* Lists are dynamic arrays, so an improper pair has nothing to be. Saying so
 * beats quietly answering `(1 2)`, which is a different value. */
void test_scheme_builtins_cons_rejects_an_improper_pair() {
    assert_fails_with("(cons 1 2)", "cons - argument 2 must be list, got integer (there are no improper pairs)");
    assert_fails_with("(cons 1 \"x\")", "cons - argument 2 must be list, got string (there are no improper pairs)");
    /* An empty vector is not an empty list, so it is not a tail either. */
    assert_fails_with("(cons 1 #())", "cons - argument 2 must be list, got vector (there are no improper pairs)");
}

void test_scheme_builtins_cons_takes_exactly_two_arguments() {
    assert_fails_with("(cons)", "cons - expects 2 argument(s), got 0");
    assert_fails_with("(cons 1)", "cons - expects 2 argument(s), got 1");
    assert_fails_with("(cons 1 '(2) 3)", "cons - expects 2 argument(s), got 3");
}

void test_scheme_builtins_car_gives_the_first_element() {
    assert_prints("(car '(1 2 3))", "1");
    assert_prints("(car '(1))", "1");
    assert_prints("(car '((1 2) 3))", "(1 2)");
    assert_prints("(car (cons 9 '(1)))", "9");
}

/* The tail, and an exhausted one is `()` rather than an error -- that is the
 * base case every recursion over a list stops on. */
void test_scheme_builtins_cdr_gives_the_rest() {
    assert_prints("(cdr '(1 2 3))", "(2 3)");
    assert_prints("(cdr '(1 2))", "(2)");
    assert_prints("(cdr '(1))", "()");
    assert_prints("(cdr '((1 2) 3))", "(3)");
}

/* Unlike `cdr` of a one-element list, neither has an answer to give. */
void test_scheme_builtins_car_and_cdr_reject_the_empty_list() {
    assert_fails_with("(car '())", "car - expects a non-empty list");
    assert_fails_with("(cdr '())", "cdr - expects a non-empty list");
    assert_fails_with("(car (list))", "car - expects a non-empty list");
    assert_fails_with("(cdr (cdr '(1)))", "cdr - expects a non-empty list");
}

void test_scheme_builtins_car_and_cdr_take_one_list() {
    assert_fails_with("(car)", "car - expects 1 argument(s), got 0");
    assert_fails_with("(cdr '(1) '(2))", "cdr - expects 1 argument(s), got 2");
    assert_fails_with("(car 1)", "car - argument 1 must be list, got integer");
    assert_fails_with("(cdr \"ab\")", "cdr - argument 1 must be list, got string");
    /* Again the vector: a different type, not a list with elements in it. */
    assert_fails_with("(car #(1 2))", "car - argument 1 must be list, got vector");
}

/* Variadic down to zero, and every operand deep-copied on the way in. */
void test_scheme_builtins_list_collects_its_arguments() {
    assert_prints("(list)", "()");
    assert_prints("(list 1 2 3)", "(1 2 3)");
    assert_prints("(list 1 \"a\" #\\b '(2))", "(1 \"a\" #\\b (2))");
    assert_prints("(list (+ 1 1) (* 2 3))", "(2 6)");
}

/* The empty list only. `#()` is an empty *vector* -- a distinct type, and the
 * case a bare length check would get wrong. */
void test_scheme_builtins_null_tests_for_the_empty_list() {
    assert_prints("(null? '())", "#t");
    assert_prints("(null? (list))", "#t");
    assert_prints("(null? (cdr '(1)))", "#t");
    assert_prints("(null? '(1))", "#f");
    assert_prints("(null? #())", "#f");
    assert_prints("(null? 0)", "#f");
    assert_prints("(null? \"\")", "#f");
    assert_prints("(null? #f)", "#f");
}

/* The complement of `null?` over lists, and #f for every other type -- a
 * non-list is not an error to either of them. */
void test_scheme_builtins_pair_tests_for_a_non_empty_list() {
    assert_prints("(pair? '(1))", "#t");
    assert_prints("(pair? '(1 2))", "#t");
    assert_prints("(pair? '(()))", "#t");
    assert_prints("(pair? '())", "#f");
    assert_prints("(pair? #(1))", "#f");
    assert_prints("(pair? 1)", "#f");
    assert_prints("(pair? \"ab\")", "#f");
}

void test_scheme_builtins_null_and_pair_take_exactly_one_argument() {
    assert_fails_with("(null?)", "null? - expects 1 argument(s), got 0");
    assert_fails_with("(null? '() '())", "null? - expects 1 argument(s), got 2");
    assert_fails_with("(pair?)", "pair? - expects 1 argument(s), got 0");
    assert_fails_with("(pair? 1 2)", "pair? - expects 1 argument(s), got 2");
}

/* The source list is gone before the result is printed: `summa_scheme_apply`
 * frees the argument list the instant dispatch returns, and a call frame goes
 * with the call. Anything handed back that still pointed into either would be
 * freed memory here, which is what ASan catches on this suite. */
void test_scheme_builtins_pair_results_outlive_the_list_they_came_from() {
    assert_prints("(car (list (+ 1 1) 5))", "2");
    assert_prints("(cdr (list 1 (+ 2 3)))", "(5)");
    /* A nested element, so the copy has to be deep rather than one level. */
    assert_prints("(car (list (list 1 2) 3))", "(1 2)");
    assert_prints("(car (cdr (list 1 (list \"a\") 3)))", "(\"a\")");
    assert_prints("(define (head xs) (car xs))"
                  "(head (list 1 2))",
                  "1");
    assert_prints("(define (tail xs) (cdr xs))"
                  "(tail (list 1 \"b\" 3))",
                  "(\"b\" 3)");
    assert_prints("(let ((xs (list 1 2 3))) (cdr xs))", "(2 3)");
    assert_prints("(cons 0 (cdr (list 1 2)))", "(0 2)");
}

/* Nothing shares structure -- `cdr` copies -- so a name rebound after the fact
 * cannot be seen through a tail taken before it. */
void test_scheme_builtins_list_results_do_not_share_structure() {
    assert_prints("(define xs (list 1 2 3))"
                  "(define ys (cdr xs))"
                  "(set! xs (list 9))"
                  "ys",
                  "(2 3)");
}

#pragma endregion pairs and lists

#pragma region composition

/* Every builtin is reachable by name from a fresh global environment -- the
 * table registration, checked end to end rather than by reading the array. */
void test_scheme_builtins_are_all_bound_in_the_global_environment() {
    assert_prints("(if (< (* (- 10 4) 7) 43) (quotient 84 2) 0)", "42");
    assert_prints("(if (not (zero? (modulo 43 2))) (remainder 85 43) 0)", "42");
    assert_prints("(if (>= (string-length \"abc\") 3) (- (char->integer (string-ref \"abc\" 0)) 55) 0)", "42");
}

/* The base case a recursion actually needs -- comparison plus subtraction,
 * which is what the special-forms suite had to fake with a boolean flag. */
void test_scheme_builtins_give_recursion_a_numeric_base_case() {
    assert_prints("(define (count-down n acc) (if (zero? n) acc (count-down (- n 1) (+ acc n))))"
                  "(count-down 10 0)",
                  "55");
}

/* What the list procedures are for: a recursion whose base case is `null?` and
 * whose step is `cdr`, which is the shape every program over a list takes.
 * Ten elements -- evaluation is a plain C recursion, so a long list would meet
 * SUMMA_SCHEME_MAX_DEPTH rather than prove anything more. */
void test_scheme_builtins_recurse_over_a_list() {
    assert_prints("(define (sum xs) (if (null? xs) 0 (+ (car xs) (sum (cdr xs)))))"
                  "(sum (list 1 2 3 4 5 6 7 8 9 10))",
                  "55");
    assert_prints("(define (length xs) (if (null? xs) 0 (+ 1 (length (cdr xs)))))"
                  "(length '(a b c d e))",
                  "5");
    /* Built back up as well as taken apart, and `pair?` deciding the step. */
    assert_prints("(define (reverse xs acc) (if (pair? xs) (reverse (cdr xs) (cons (car xs) acc)) acc))"
                  "(reverse '(1 2 3) '())",
                  "(3 2 1)");
}

#pragma endregion composition

int main(int argc, char** argv) {
    summa_test_begin("scheme.builtins", argc, argv);

    SUMMA_TEST_RUN(test_scheme_builtins_subtract_negates_a_single_argument);
    SUMMA_TEST_RUN(test_scheme_builtins_subtract_folds_left);
    SUMMA_TEST_RUN(test_scheme_builtins_subtract_mixes_integers_and_floats);
    SUMMA_TEST_RUN(test_scheme_builtins_subtract_rejects_no_arguments);
    SUMMA_TEST_RUN(test_scheme_builtins_subtract_rejects_non_numbers);

    SUMMA_TEST_RUN(test_scheme_builtins_multiply_folds_left);
    SUMMA_TEST_RUN(test_scheme_builtins_multiply_of_nothing_is_one);
    SUMMA_TEST_RUN(test_scheme_builtins_multiply_mixes_integers_and_floats);
    SUMMA_TEST_RUN(test_scheme_builtins_multiply_rejects_non_numbers);

    SUMMA_TEST_RUN(test_scheme_builtins_numeric_equal_compares_a_pair);
    SUMMA_TEST_RUN(test_scheme_builtins_less_and_greater_compare_a_pair);
    SUMMA_TEST_RUN(test_scheme_builtins_less_equal_and_greater_equal_include_equality);
    SUMMA_TEST_RUN(test_scheme_builtins_comparisons_chain);
    SUMMA_TEST_RUN(test_scheme_builtins_comparisons_mix_integers_and_floats);
    SUMMA_TEST_RUN(test_scheme_builtins_comparisons_do_not_lose_integer_precision);
    SUMMA_TEST_RUN(test_scheme_builtins_comparisons_require_two_arguments);
    SUMMA_TEST_RUN(test_scheme_builtins_comparisons_reject_non_numbers);

    SUMMA_TEST_RUN(test_scheme_builtins_quotient_truncates_toward_zero);
    SUMMA_TEST_RUN(test_scheme_builtins_remainder_takes_the_sign_of_the_dividend);
    SUMMA_TEST_RUN(test_scheme_builtins_modulo_takes_the_sign_of_the_divisor);
    SUMMA_TEST_RUN(test_scheme_builtins_modulo_and_remainder_agree_when_evenly_divided);
    SUMMA_TEST_RUN(test_scheme_builtins_integer_division_takes_exactly_two_arguments);
    SUMMA_TEST_RUN(test_scheme_builtins_integer_division_rejects_floats);
    SUMMA_TEST_RUN(test_scheme_builtins_integer_division_by_zero_is_an_error);
    SUMMA_TEST_RUN(test_scheme_builtins_integer_division_overflow_is_an_error);

    SUMMA_TEST_RUN(test_scheme_builtins_zero_tests_a_number);
    SUMMA_TEST_RUN(test_scheme_builtins_zero_takes_one_number);
    SUMMA_TEST_RUN(test_scheme_builtins_not_inverts_truthiness);
    SUMMA_TEST_RUN(test_scheme_builtins_not_takes_exactly_one_argument);

    SUMMA_TEST_RUN(test_scheme_builtins_string_length_counts_bytes);
    SUMMA_TEST_RUN(test_scheme_builtins_string_length_takes_one_string);
    SUMMA_TEST_RUN(test_scheme_builtins_string_ref_indexes_from_zero);
    SUMMA_TEST_RUN(test_scheme_builtins_string_ref_rejects_an_out_of_range_index);
    SUMMA_TEST_RUN(test_scheme_builtins_string_ref_checks_both_argument_types);
    SUMMA_TEST_RUN(test_scheme_builtins_char_to_integer_gives_the_code_point);
    SUMMA_TEST_RUN(test_scheme_builtins_char_to_integer_takes_one_character);
    SUMMA_TEST_RUN(test_scheme_builtins_string_and_char_procedures_compose);

    SUMMA_TEST_RUN(test_scheme_builtins_cons_prepends_to_a_list);
    SUMMA_TEST_RUN(test_scheme_builtins_cons_nests_a_list_head);
    SUMMA_TEST_RUN(test_scheme_builtins_cons_rejects_an_improper_pair);
    SUMMA_TEST_RUN(test_scheme_builtins_cons_takes_exactly_two_arguments);
    SUMMA_TEST_RUN(test_scheme_builtins_car_gives_the_first_element);
    SUMMA_TEST_RUN(test_scheme_builtins_cdr_gives_the_rest);
    SUMMA_TEST_RUN(test_scheme_builtins_car_and_cdr_reject_the_empty_list);
    SUMMA_TEST_RUN(test_scheme_builtins_car_and_cdr_take_one_list);
    SUMMA_TEST_RUN(test_scheme_builtins_list_collects_its_arguments);
    SUMMA_TEST_RUN(test_scheme_builtins_null_tests_for_the_empty_list);
    SUMMA_TEST_RUN(test_scheme_builtins_pair_tests_for_a_non_empty_list);
    SUMMA_TEST_RUN(test_scheme_builtins_null_and_pair_take_exactly_one_argument);
    SUMMA_TEST_RUN(test_scheme_builtins_pair_results_outlive_the_list_they_came_from);
    SUMMA_TEST_RUN(test_scheme_builtins_list_results_do_not_share_structure);

    SUMMA_TEST_RUN(test_scheme_builtins_are_all_bound_in_the_global_environment);
    SUMMA_TEST_RUN(test_scheme_builtins_give_recursion_a_numeric_base_case);
    SUMMA_TEST_RUN(test_scheme_builtins_recurse_over_a_list);

    return summa_test_end();
}
