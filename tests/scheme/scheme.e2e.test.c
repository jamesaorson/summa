#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <string.h>

/* Read, evaluate, print -- the three pieces wired together and driven over a
 * whole program rather than a single datum. The other scheme suites each test
 * one stage in isolation; what only shows up here is the seam between them,
 * and the environment persisting from one expression to the next. */

#define SCOPED_GLOBAL_ENV(var) \
    SUMMA_TEST_SCOPED_VALUE(   \
        SummaSchemeEnvironment, var, summa_scheme_environment_make_global(), summa_scheme_environment_free)

/* The loop a REPL actually runs. `out` ends up holding the last expression's
 * value, since that is what a program evaluates to; each turn releases the
 * previous value and the form it came from, so a long program does not
 * accumulate either. Stops at the first error, read or evaluate. */
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

/* Runs `program` in a fresh global environment and asserts the last value
 * renders as `expected`. Rendering goes through `print` rather than `display`,
 * so a string result comes back quoted the way a REPL echoes it. How any one
 * value is formatted is the print suite's business -- what is checked here is
 * that the pipeline produced that value at all. */
static void assert_program_prints(const char* program, const char* expected) {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       result = {};
        const SummaSchemeError err    = run_program(env, program, &result);

        SUMMA_TEST_ASSERT(!err.had);
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

static void assert_program_fails(const char* program) {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       result = {};
        const SummaSchemeError err    = run_program(env, program, &result);

        SUMMA_TEST_ASSERT(err.had);
        summa_scheme_value_free(&result);
    }
}

/* The message is what a REPL user is left with, so the ones they will hit most
 * are pinned rather than just checked for failing. */
static void assert_program_fails_with(const char* program, const char* expected_message) {
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

#pragma region literals

/* A bare literal is a program: it reads, evaluates to itself, and prints. */
void test_scheme_e2e_evaluates_an_integer_literal() {
    assert_program_prints("42", "42");
}

void test_scheme_e2e_evaluates_a_boolean_literal() {
    assert_program_prints("#t", "#t");
    assert_program_prints("#f", "#f");
}

/* `print` is `write`, so the quotes survive the round trip. */
void test_scheme_e2e_evaluates_a_string_literal() {
    assert_program_prints("\"hi\"", "\"hi\"");
}

/* The escape is translated on the way in and rendered back out as the byte it
 * denotes -- the reader's work showing up at the far end of the pipeline. */
void test_scheme_e2e_evaluates_a_string_with_escapes() {
    assert_program_prints("\"a\\tb\"", "\"a\tb\"");
}

#pragma endregion literals

#pragma region arithmetic

void test_scheme_e2e_adds_two_numbers() {
    assert_program_prints("(+ 1 2)", "3");
}

void test_scheme_e2e_adds_a_variable_number_of_arguments() {
    assert_program_prints("(+)", "0");
    assert_program_prints("(+ 7)", "7");
    assert_program_prints("(+ 1 2 3 4)", "10");
}

void test_scheme_e2e_adds_nested_results() {
    assert_program_prints("(+ (+ 1 2) (+ 3 4))", "10");
}

/* An integer and a floating operand give a floating result. The `%f` spelling
 * is print's convention, pinned by the print suite. */
void test_scheme_e2e_mixes_integers_and_floats() {
    assert_program_prints("(+ 1 2.5)", "3.500000");
}

#pragma endregion arithmetic

#pragma region definitions

void test_scheme_e2e_defines_and_reads_back_a_variable() {
    assert_program_prints("(define x 2) (+ x 40)", "42");
}

/* The point of the loop: a binding made by one expression is still there for
 * the next one. */
void test_scheme_e2e_carries_bindings_across_expressions() {
    assert_program_prints("(define a 1) (define b 2) (define c 3) (+ a b c)", "6");
}

void test_scheme_e2e_rebinds_with_set() {
    assert_program_prints("(define x 1) (set! x 41) (+ x 1)", "42");
}

#pragma endregion definitions

#pragma region procedures

void test_scheme_e2e_applies_a_lambda_immediately() {
    assert_program_prints("((lambda (n) (+ n 1)) 41)", "42");
}

void test_scheme_e2e_defines_a_procedure_with_the_shorthand() {
    assert_program_prints("(define (inc n) (+ n 1)) (inc 41)", "42");
}

void test_scheme_e2e_defines_a_procedure_as_a_lambda_binding() {
    assert_program_prints("(define inc (lambda (n) (+ n 1))) (inc 41)", "42");
}

void test_scheme_e2e_applies_a_procedure_of_several_arguments() {
    assert_program_prints("(define (add2 a b) (+ a b)) (define x 40) (add2 x 2)", "42");
}

#pragma endregion procedures

#pragma region conditionals

void test_scheme_e2e_takes_the_true_branch() {
    assert_program_prints("(if #t 1 2)", "1");
}

void test_scheme_e2e_takes_the_false_branch() {
    assert_program_prints("(if #f 1 2)", "2");
}

/* Everything but `#f` is true, which is what makes `(if n ...)` a usable test. */
void test_scheme_e2e_treats_every_non_false_value_as_true() {
    assert_program_prints("(if 0 1 2)", "1");
    assert_program_prints("(if \"\" 1 2)", "1");
}

void test_scheme_e2e_evaluates_a_cond() {
    assert_program_prints("(cond (#f 1) (#t 2))", "2");
}

void test_scheme_e2e_evaluates_when_and_unless() {
    assert_program_prints("(when #t 42)", "42");
    assert_program_prints("(unless #f 42)", "42");
}

/* `and` and `or` give back the operand that settled them rather than a
 * boolean, and stop as soon as it is settled. */
void test_scheme_e2e_short_circuits_and_or() {
    assert_program_prints("(and 1 2)", "2");
    assert_program_prints("(and #f 2)", "#f");
    assert_program_prints("(or #f 42)", "42");
    assert_program_prints("(or 42 #f)", "42");
}

#pragma endregion conditionals

#pragma region binding forms

void test_scheme_e2e_evaluates_a_let() {
    assert_program_prints("(let ((a 1) (b 2)) (+ a b))", "3");
}

/* `let*` sees the bindings to its left; plain `let` would not. */
void test_scheme_e2e_evaluates_a_let_star() {
    assert_program_prints("(let* ((a 1) (b (+ a 1))) (+ a b))", "3");
}

/* `letrec` binds the name before the lambda is built, so the body can call
 * itself -- recursion without a top-level `define`. */
void test_scheme_e2e_evaluates_a_recursive_letrec() {
    assert_program_prints("(letrec ((f (lambda (n) (if n (f #f) 42)))) (f #t))", "42");
}

void test_scheme_e2e_sequences_with_begin() {
    assert_program_prints("(begin 1 2 42)", "42");
}

#pragma endregion binding forms

#pragma region quoting

void test_scheme_e2e_quotes_a_symbol() {
    assert_program_prints("(quote x)", "x");
    assert_program_prints("'x", "x");
}

/* Quoting is what stops `(1 2 3)` from being applied. */
void test_scheme_e2e_quotes_a_list() {
    assert_program_prints("'(1 2 3)", "(1 2 3)");
    assert_program_prints("'(a (b c))", "(a (b c))");
}

#pragma endregion quoting

#pragma region whole programs

/* Atmosphere between and around expressions is the reader's problem, but a
 * program written the way a person writes one is what proves it. */
void test_scheme_e2e_runs_a_program_spanning_lines_and_comments() {
    assert_program_prints("; double it\n"
                          "(define (double n) (+ n n))\n"
                          "\n"
                          "(define seed 21) ; the answer, halved\n"
                          "(double seed)\n",
                          "42");
}

/* Several expressions, each building on the last. */
void test_scheme_e2e_runs_a_multi_step_program() {
    assert_program_prints("(define x 0)"
                          "(define (bump n) (+ n 1))"
                          "(set! x (bump x))"
                          "(set! x (bump x))"
                          "(+ x 40)",
                          "42");
}

/* The value of a program is the value of its last expression, whatever the
 * ones before it did. */
void test_scheme_e2e_yields_the_last_expression() {
    assert_program_prints("1 2 3", "3");
    assert_program_prints("(define x 1) 99", "99");
}

#pragma endregion whole programs

#pragma region errors

void test_scheme_e2e_rejects_an_unbound_variable() {
    assert_program_fails_with("nope", "Unbound variable: nope");
}

/* Reading and evaluating fail independently: `(1 2 3)` is a fine datum and a
 * bad expression. */
void test_scheme_e2e_rejects_applying_a_non_procedure() {
    assert_program_fails_with("(1 2 3)", "Wrong type to apply: 1");
}

void test_scheme_e2e_rejects_bad_arguments() {
    assert_program_fails("(+ 1 \"x\")");
}

/* A read error stops the program where an evaluate error would. */
void test_scheme_e2e_stops_on_a_read_error() {
    assert_program_fails("(define x 1) (+ x");
    assert_program_fails("(define x 1) \"unterminated");
}

/* The failure has to come from the expression that is wrong, not from the ones
 * before it -- so the earlier ones must have run cleanly. */
void test_scheme_e2e_runs_up_to_the_failing_expression() {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       result = {};
        const SummaSchemeError err    = run_program(env, "(define x 40) (+ x nope)", &result);

        SUMMA_TEST_ASSERT(err.had);
        if (err.had) {
            SUMMA_TEST_ASSERT_EQ_STR("Unbound variable: nope", err.message);
        }
        summa_scheme_value_free(&result);

        /* `x` survived, so the failure was the second expression's alone. */
        SummaSchemeValue       again    = {};
        const SummaSchemeError next_err = run_program(env, "(+ x 2)", &again);
        SUMMA_TEST_ASSERT(!next_err.had);
        if (!next_err.had) {
            SUMMA_TEST_ASSERT_EQ(SummaSchemeIntegerType, again.type);
            SUMMA_TEST_ASSERT_EQ(42, again.value.integer.value);
        }
        summa_scheme_value_free(&again);
    }
}

#pragma endregion errors

int main(int argc, char** argv) {
    summa_test_begin("scheme.e2e", argc, argv);

    SUMMA_TEST_RUN(test_scheme_e2e_evaluates_an_integer_literal);
    SUMMA_TEST_RUN(test_scheme_e2e_evaluates_a_boolean_literal);
    SUMMA_TEST_RUN(test_scheme_e2e_evaluates_a_string_literal);
    SUMMA_TEST_RUN(test_scheme_e2e_evaluates_a_string_with_escapes);

    SUMMA_TEST_RUN(test_scheme_e2e_adds_two_numbers);
    SUMMA_TEST_RUN(test_scheme_e2e_adds_a_variable_number_of_arguments);
    SUMMA_TEST_RUN(test_scheme_e2e_adds_nested_results);
    SUMMA_TEST_RUN(test_scheme_e2e_mixes_integers_and_floats);

    SUMMA_TEST_RUN(test_scheme_e2e_defines_and_reads_back_a_variable);
    SUMMA_TEST_RUN(test_scheme_e2e_carries_bindings_across_expressions);
    SUMMA_TEST_RUN(test_scheme_e2e_rebinds_with_set);

    SUMMA_TEST_RUN(test_scheme_e2e_applies_a_lambda_immediately);
    SUMMA_TEST_RUN(test_scheme_e2e_defines_a_procedure_with_the_shorthand);
    SUMMA_TEST_RUN(test_scheme_e2e_defines_a_procedure_as_a_lambda_binding);
    SUMMA_TEST_RUN(test_scheme_e2e_applies_a_procedure_of_several_arguments);

    SUMMA_TEST_RUN(test_scheme_e2e_takes_the_true_branch);
    SUMMA_TEST_RUN(test_scheme_e2e_takes_the_false_branch);
    SUMMA_TEST_RUN(test_scheme_e2e_treats_every_non_false_value_as_true);
    SUMMA_TEST_RUN(test_scheme_e2e_evaluates_a_cond);
    SUMMA_TEST_RUN(test_scheme_e2e_evaluates_when_and_unless);
    SUMMA_TEST_RUN(test_scheme_e2e_short_circuits_and_or);

    SUMMA_TEST_RUN(test_scheme_e2e_evaluates_a_let);
    SUMMA_TEST_RUN(test_scheme_e2e_evaluates_a_let_star);
    SUMMA_TEST_RUN(test_scheme_e2e_evaluates_a_recursive_letrec);
    SUMMA_TEST_RUN(test_scheme_e2e_sequences_with_begin);

    SUMMA_TEST_RUN(test_scheme_e2e_quotes_a_symbol);
    SUMMA_TEST_RUN(test_scheme_e2e_quotes_a_list);

    SUMMA_TEST_RUN(test_scheme_e2e_runs_a_program_spanning_lines_and_comments);
    SUMMA_TEST_RUN(test_scheme_e2e_runs_a_multi_step_program);
    SUMMA_TEST_RUN(test_scheme_e2e_yields_the_last_expression);

    SUMMA_TEST_RUN(test_scheme_e2e_rejects_an_unbound_variable);
    SUMMA_TEST_RUN(test_scheme_e2e_rejects_applying_a_non_procedure);
    SUMMA_TEST_RUN(test_scheme_e2e_rejects_bad_arguments);
    SUMMA_TEST_RUN(test_scheme_e2e_stops_on_a_read_error);
    SUMMA_TEST_RUN(test_scheme_e2e_runs_up_to_the_failing_expression);

    return summa_test_end();
}
