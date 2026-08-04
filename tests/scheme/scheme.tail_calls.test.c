#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include <stdio.h>
#include <string.h>

/* Proper tail calls -- R7RS 3.5, and the difference between a language that can
 * iterate and one that can only pretend to.
 *
 * `summa_scheme_evaluate_inner` is a trampoline: an expression in tail position
 * is handed back rather than recursed into, so the C stack stays where it is
 * and the frame the call left is released. Two things have to hold for that to
 * be an optimization rather than a bug, and both are tested here:
 *
 * - *Depth.* Every case below drives its form past SUMMA_SCHEME_MAX_DEPTH, so a
 *   form that quietly stopped handing its tail expression back would fail on
 *   the recursion limit rather than pass slowly.
 * - *Memory.* A trampoline that loops forever while retaining every frame has
 *   traded a stack overflow for a leak. `live_count_is_flat` is the guard.
 *
 * What is deliberately *not* optimized is just as load-bearing. Operands are
 * not tail positions -- `(+ n (sum (- n 1)))` has work left to do after the
 * call returns -- so they keep a C frame per level, and the depth guard is what
 * makes running out of them an error instead of a segfault. The last two cases
 * pin that.
 *
 * See BUILTINS.md, "Tail calls". */

#define SCOPED_GLOBAL_ENV(var) \
    SUMMA_TEST_SCOPED_VALUE(   \
        SummaSchemeEnvironment, var, summa_scheme_environment_make_global(), summa_scheme_environment_free)

/* Deeper than SUMMA_SCHEME_MAX_DEPTH by enough that no per-call frame budget
 * could absorb it, and small enough that a case still runs in milliseconds. A
 * form whose tail position regressed fails these on the recursion limit. */
#define TAIL_DEPTH "5000"

/* Read, evaluate, discard, repeat. `out` holds the last expression's value. */
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

/* The shape every tail-position case takes: a recursion that only terminates
 * because the form under test handed its last expression back. The failure
 * carries the interpreter's message, so a regression reads as "recursion limit
 * exceeded" rather than as an unexplained red. */
static void assert_program_yields_integer(const char* program, int64_t expected) {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       result = {};
        const SummaSchemeError err    = run_program(env, program, &result);

        SUMMA_TEST_ASSERT_MSG(!err.had, err.message);
        if (!err.had) {
            SUMMA_TEST_ASSERT_EQ(SummaSchemeIntegerType, result.type);
            if (result.type == SummaSchemeIntegerType) {
                SUMMA_TEST_ASSERT_EQ_INT(expected, result.value.integer.value);
            }
        }
        summa_scheme_value_free(&result);
    }
}

/* Fails, and fails for the stated reason. A case that errored on a typo in the
 * program instead of on the depth guard would otherwise look identical. */
static void assert_program_fails_with(const char* program, const char* fragment) {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       result = {};
        const SummaSchemeError err    = run_program(env, program, &result);

        SUMMA_TEST_ASSERT(err.had);
        if (err.had) {
            SUMMA_TEST_ASSERT_MSG(strstr(err.message, fragment) != nullptr, err.message);
        }
        summa_scheme_value_free(&result);
    }
}

#pragma region Tail positions

/* Each case drives one form's tail position TAIL_DEPTH deep. All but the `cond`
 * and `if` cases lean on `if` for the base case, which those two cover on their
 * own -- a form whose own tail position regressed is still the only thing that
 * can fail its case, since `if` failing would fail every case at once. */

void test_scheme_tail_calls_if_consequent() {
    assert_program_yields_integer("(define (spin n) (if (> n 0) (spin (- n 1)) n))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

void test_scheme_tail_calls_if_alternate() {
    assert_program_yields_integer("(define (spin n) (if (zero? n) n (spin (- n 1))))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

/* Two body expressions, so the case is about the *last* one rather than about
 * bodies of length one. */
void test_scheme_tail_calls_procedure_body_last_expression() {
    assert_program_yields_integer("(define (spin n) 1 (if (zero? n) n (spin (- n 1))))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

void test_scheme_tail_calls_begin_last_expression() {
    assert_program_yields_integer("(define (spin n) (if (zero? n) n (begin 1 (spin (- n 1)))))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

/* No `if` in sight: `cond` alone decides the base case and the step. */
void test_scheme_tail_calls_cond_selected_clause() {
    assert_program_yields_integer("(define (spin n) (cond ((zero? n) n) (else (spin (- n 1)))))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

/* The else clause specifically, reached only after a test that did not match. */
void test_scheme_tail_calls_cond_else_clause() {
    assert_program_yields_integer("(define (spin n) (cond ((< n 0) 99) ((zero? n) n) (else (spin (- n 1)))))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

/* The frame `let` makes is also the frame the tail call has to leave, so this
 * one covers the release as much as the position. */
void test_scheme_tail_calls_let_last_body_expression() {
    assert_program_yields_integer("(define (spin n) (if (zero? n) n (let ((k (- n 1))) (spin k))))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

void test_scheme_tail_calls_let_star_last_body_expression() {
    assert_program_yields_integer("(define (spin n) (if (zero? n) n (let* ((j 1) (k (- n j))) (spin k))))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

/* letrec binds a procedure into the frame it closes over, so every iteration
 * makes a reference cycle as well as a tail call. The collector has to keep up
 * with the loop for this to finish. */
void test_scheme_tail_calls_letrec_last_body_expression() {
    assert_program_yields_integer("(define (spin n)"
                                  "  (if (zero? n)"
                                  "      n"
                                  "      (letrec ((step (lambda (k) (spin k)))) (step (- n 1)))))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

void test_scheme_tail_calls_when_last_body_expression() {
    assert_program_yields_integer("(define (spin n) (if (zero? n) n (when #t (spin (- n 1)))))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

void test_scheme_tail_calls_unless_last_body_expression() {
    assert_program_yields_integer("(define (spin n) (if (zero? n) n (unless #f (spin (- n 1)))))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

/* `and` and `or` return the operand that decided them, and the last operand is
 * always the one that decides -- there is nothing left to test after it. */
void test_scheme_tail_calls_and_last_operand() {
    assert_program_yields_integer("(define (spin n) (if (zero? n) n (and #t (spin (- n 1)))))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

void test_scheme_tail_calls_or_last_operand() {
    assert_program_yields_integer("(define (spin n) (if (zero? n) n (or #f (spin (- n 1)))))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

/* An application whose operator is an expression rather than a name. Both
 * halves of the loop are tail calls -- `spin` calls the lambda, the lambda
 * calls `spin` -- and the lambda's body is owned by the procedure value the
 * evaluator just produced rather than by any binding, which is the second of
 * the trampoline's two body anchors. */
void test_scheme_tail_calls_application_through_an_expression_operator() {
    assert_program_yields_integer("(define (spin n) (if (zero? n) n ((lambda (k) (spin k)) (- n 1))))"
                                  "(spin " TAIL_DEPTH ")",
                                  0);
}

#pragma endregion Tail positions

#pragma region Depth

/* The acceptance number from the issue: a million iterations, where the old
 * guard stopped at 998. Nothing about this program is unusual -- it is what an
 * ordinary `for` loop looks like in Scheme. */
void test_scheme_tail_calls_run_a_million_deep() {
    assert_program_yields_integer("(define (count n acc) (if (zero? n) acc (count (- n 1) (+ acc 1))))"
                                  "(count 1000000 0)",
                                  1000000);
}

/* Two procedures calling each other is the same optimization seen from the
 * other side: nothing is self-recursive, so no amount of cleverness about a
 * single procedure's own body would help. */
void test_scheme_tail_calls_mutual_recursion_runs_a_million_deep() {
    assert_program_yields_integer("(define (ping n) (if (zero? n) 0 (pong (- n 1))))"
                                  "(define (pong n) (if (zero? n) 1 (ping (- n 1))))"
                                  "(ping 1000000)",
                                  0);
}

/* The hazard #27 left behind, and the one most likely to be silently broken:
 * the procedure being tail-called is bound *in the frame the tail call is
 * leaving*. `(f f (- n 1))` releases the frame that owns `f`, and `f`'s body is
 * what runs next -- so the binding lookup has to report which environment held
 * it and the evaluator has to retain that environment for the call.
 *
 * Every iteration does it again, so a use-after-free here is not a one-in-a-run
 * event. Under ASan it is a hard failure; without it, a wrong answer. */
void test_scheme_tail_calls_procedure_bound_in_the_frame_being_released() {
    assert_program_yields_integer("(define (run f n) (if (zero? n) 0 (f f (- n 1))))"
                                  "(run run 20000)",
                                  0);
}

/* The `(define (twice f x) (f (f x)))` shape from the issue, spelled out: the
 * inner call is an operand and recurses, the outer one is a tail call through
 * a parameter. */
void test_scheme_tail_calls_through_a_parameter_bound_procedure() {
    assert_program_yields_integer("(define (spin n) (if (zero? n) 0 (spin (- n 1))))"
                                  "(define (twice f x) (f (f x)))"
                                  "(twice spin " TAIL_DEPTH ")",
                                  0);
}

#pragma endregion Depth

#pragma region Memory

/* A tail call must *release* the frame it leaves, not merely stop growing the C
 * stack -- an evaluator that loops forever while retaining every frame has
 * swapped a crash for a leak, and both of those are the same bug from the
 * program's point of view.
 *
 * Read after the run rather than during it, and compared across iteration
 * counts three orders of magnitude apart rather than against a constant. A
 * retained frame is unreachable but counted, so it is still in the registry
 * when the run ends: leaking one per iteration would make these three numbers
 * differ by exactly the iteration counts. The global environment is still alive
 * at each reading, so what is left over is the program's, not the harness's. */
static size_t live_count_after_spinning(const char* iterations) {
    char   program[256];
    size_t live = 0;

    snprintf(program,
             sizeof(program),
             "(define (spin n) (if (zero? n) n (spin (- n 1))))"
             "(spin %s)",
             iterations);

    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       result = {};
        const SummaSchemeError err    = run_program(env, program, &result);
        SUMMA_TEST_ASSERT_MSG(!err.had, err.message);
        live = summa_scheme_environment_live_count();
        summa_scheme_value_free(&result);
    }
    return live;
}

void test_scheme_tail_calls_live_environment_count_is_flat() {
    const size_t ten          = live_count_after_spinning("10");
    const size_t ten_thousand = live_count_after_spinning("10000");
    const size_t million      = live_count_after_spinning("1000000");

    SUMMA_TEST_ASSERT_EQ_INT((int64_t)ten, (int64_t)ten_thousand);
    SUMMA_TEST_ASSERT_EQ_INT((int64_t)ten, (int64_t)million);
}

#pragma endregion Memory

#pragma region What is still not a tail position

/* `(+ n (sum (- n 1)))` calls in an *operand*: the caller has to add `n` to
 * whatever comes back, so it needs a C frame and gets one. This is the shape
 * proper tail calls do not help, and it still has to give the right answer. */
void test_scheme_tail_calls_operands_still_recurse() {
    assert_program_yields_integer("(define (sum n) (if (zero? n) 0 (+ n (sum (- n 1)))))"
                                  "(sum 500)",
                                  125250);
}

/* And past the budget it is an error rather than a crash, which is the whole
 * reason SUMMA_SCHEME_MAX_DEPTH survives the trampoline. Deleting the guard
 * would turn this case into a segfault, not into a pass. */
void test_scheme_tail_calls_deep_non_tail_recursion_is_an_error_not_a_crash() {
    assert_program_fails_with("(define (sum n) (if (zero? n) 0 (+ n (sum (- n 1)))))"
                              "(sum 1000000)",
                              "recursion limit exceeded");
}

/* The same guard from the other direction: a runaway with no base case at all.
 * `(define (loop) (loop))` would be a correct non-terminating program now, so
 * the runaway that still has to be caught is a non-tail one. */
void test_scheme_tail_calls_non_tail_runaway_is_an_error_not_a_crash() {
    assert_program_fails_with("(define (loop) (+ 1 (loop)))"
                              "(loop)",
                              "recursion limit exceeded");
}

#pragma endregion What is still not a tail position

int main(int argc, char** argv) {
    summa_test_begin("scheme.tail_calls", argc, argv);

    SUMMA_TEST_RUN(test_scheme_tail_calls_if_consequent);
    SUMMA_TEST_RUN(test_scheme_tail_calls_if_alternate);
    SUMMA_TEST_RUN(test_scheme_tail_calls_procedure_body_last_expression);
    SUMMA_TEST_RUN(test_scheme_tail_calls_begin_last_expression);
    SUMMA_TEST_RUN(test_scheme_tail_calls_cond_selected_clause);
    SUMMA_TEST_RUN(test_scheme_tail_calls_cond_else_clause);
    SUMMA_TEST_RUN(test_scheme_tail_calls_let_last_body_expression);
    SUMMA_TEST_RUN(test_scheme_tail_calls_let_star_last_body_expression);
    SUMMA_TEST_RUN(test_scheme_tail_calls_letrec_last_body_expression);
    SUMMA_TEST_RUN(test_scheme_tail_calls_when_last_body_expression);
    SUMMA_TEST_RUN(test_scheme_tail_calls_unless_last_body_expression);
    SUMMA_TEST_RUN(test_scheme_tail_calls_and_last_operand);
    SUMMA_TEST_RUN(test_scheme_tail_calls_or_last_operand);
    SUMMA_TEST_RUN(test_scheme_tail_calls_application_through_an_expression_operator);

    SUMMA_TEST_RUN(test_scheme_tail_calls_run_a_million_deep);
    SUMMA_TEST_RUN(test_scheme_tail_calls_mutual_recursion_runs_a_million_deep);
    SUMMA_TEST_RUN(test_scheme_tail_calls_procedure_bound_in_the_frame_being_released);
    SUMMA_TEST_RUN(test_scheme_tail_calls_through_a_parameter_bound_procedure);

    SUMMA_TEST_RUN(test_scheme_tail_calls_live_environment_count_is_flat);

    SUMMA_TEST_RUN(test_scheme_tail_calls_operands_still_recurse);
    SUMMA_TEST_RUN(test_scheme_tail_calls_deep_non_tail_recursion_is_an_error_not_a_crash);
    SUMMA_TEST_RUN(test_scheme_tail_calls_non_tail_runaway_is_an_error_not_a_crash);

    return summa_test_end();
}
