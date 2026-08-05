#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

/* Environment lifetime -- who owns a frame, and when it stops being anyone's.
 *
 * Two failure modes, and they are each other's opposites. A frame freed while a
 * closure still points at it is a dangling capture; a frame nobody frees is a
 * leak. Today the evaluator has the first: `closure` is borrowed, so a
 * procedure escaping the frame it was defined in reads freed memory. Reference
 * counting the environments fixes exactly that, and introduces exactly the
 * second -- a procedure bound into the frame it closes over is a cycle, and a
 * cycle's count never reaches zero.
 *
 * Both halves live here because a fix for either that breaks the other is not a
 * fix. The escape cases are the specification refcounting has to meet; the
 * cycle cases are the regression guard on what it costs.
 *
 * See BUILTINS.md, "Closures outlive their frames". */

#define SCOPED_GLOBAL_ENV(var) \
    SUMMA_TEST_SCOPED_VALUE(   \
        SummaSchemeEnvironment, var, summa_scheme_environment_make_global(), summa_scheme_environment_free)

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

static void assert_prints(const char* program, const char* expected) {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       result = {};
        const SummaSchemeError err    = run_program(env, program, &result);

        SUMMA_TEST_ASSERT_MSG(!err.had, err.message);
        if (!err.had) {
            SUMMA_TEST_SCOPED_FILE(f) {
                SUMMA_TEST_ASSERT(!summa_scheme_print(result, f.file).had);
                SUMMA_TEST_ASSERT_FILE_EQ_STR(f, expected);
            }
        }
        summa_scheme_value_free(&result);
    }
}

/* The same, for a program that is supposed to fail. The message is checked
 * loosely -- a substring -- so an error's wording can be improved without
 * breaking a case about ownership.
 *
 * These matter more than the passing ones for arguments moved into a frame: a
 * failure is where a value can end up owned by nobody or by two things at once,
 * and `leaks` and ASan are what actually read the result. */
static void assert_errors(const char* program, const char* expected_fragment) {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       result = {};
        const SummaSchemeError err    = run_program(env, program, &result);

        SUMMA_TEST_ASSERT_MSG(err.had, "expected the program to fail, and it did not");
        if (err.had) {
            SUMMA_TEST_ASSERT_MSG(strstr(err.message, expected_fragment) != nullptr, err.message);
        }
        summa_scheme_value_free(&result);
    }
}

/* Runs a program to completion, tears the global environment down, and asserts
 * every environment the run made went with it.
 *
 * The count is taken before and compared after rather than against zero: the
 * suite's own scoped values come and go around it, and an absolute number would
 * pin this to the order the cases happen to run in.
 *
 * This is what a cycle looks like from the outside. The program is correct, the
 * answer is right, and an environment stays behind because two objects hold
 * each other up. */
static void assert_reclaims_every_environment(const char* program, const char* expected) {
    const size_t before = summa_scheme_environment_live_count();
    {
        SCOPED_GLOBAL_ENV(env) {
            SummaSchemeValue       result = {};
            const SummaSchemeError err    = run_program(env, program, &result);

            SUMMA_TEST_ASSERT_MSG(!err.had, err.message);
            if (!err.had) {
                SUMMA_TEST_SCOPED_FILE(f) {
                    SUMMA_TEST_ASSERT(!summa_scheme_print(result, f.file).had);
                    SUMMA_TEST_ASSERT_FILE_EQ_STR(f, expected);
                }
            }
            summa_scheme_value_free(&result);
        }
    }
    const size_t after = summa_scheme_environment_live_count();
    if (after != before) {
        char message[128];
        snprintf(message,
                 sizeof(message),
                 "%zu environment(s) survived the run -- unreachable and unfreed",
                 after - before);
        SUMMA_TEST_ASSERT_MSG(after == before, message);
    }
}

#pragma region Escaping closures

/* A procedure captures the environment it was defined in. When that
 * environment is a call frame, the frame used to be freed the moment the call
 * returned -- so the capture dangled, and reading the captured binding was a
 * use-after-free rather than a wrong answer.
 *
 * The capture is a reference now, so the frame outlives the call that made it
 * for exactly as long as the procedure does. */
void test_scheme_lifetime_closure_outlives_the_call_frame_it_captured() {
    assert_prints("(define (make-adder n) (lambda (x) (+ x n)))"
                  "(define add5 (make-adder 5))"
                  "(add5 10)",
                  "15");
}

/* The same shape through `let` rather than a call: `summa_scheme_let` releases
 * its frame on the way out with no more ceremony than dispatch does, and the
 * returned lambda is what keeps the count off zero. */
void test_scheme_lifetime_closure_outlives_the_let_frame_it_captured() {
    assert_prints("(define f (let ((n 7)) (lambda () n)))"
                  "(f)",
                  "7");
}

/* A procedure stored back into the frame it closes over, which is both an
 * escape and -- now that counting is what frees frames -- a cycle. It belongs
 * to both halves of this file, and is the case most likely to be fixed in a way
 * that breaks the other one. */
void test_scheme_lifetime_closure_stored_into_its_own_frame() {
    assert_prints("(define (make) (let ((self 0)) (set! self (lambda () self)) self))"
                  "(define s (make))"
                  "(procedure? (s))",
                  "#t");
}

#pragma endregion Escaping closures

#pragma region Reference cycles

/* Every one of these is correct Scheme that runs correctly today, because a
 * frame is freed by whoever made it and no count is consulted. They are here
 * because reference counting changes that, and each one is a shape whose count
 * will not reach zero.
 *
 * A red case here means an environment was made and never freed. */

/* The global environment binds `down`, and `down`'s closure is the global
 * environment. One cycle per top-level procedure -- bounded by the size of the
 * program, so it leaks once rather than without limit. */
void test_scheme_lifetime_top_level_recursion_reclaims_its_environment() {
    assert_reclaims_every_environment("(define (down n) (if (zero? n) 0 (down (- n 1))))"
                                      "(down 5)",
                                      "0");
}

/* The same cycle with two procedures in it, which is what a collector walking
 * one edge at a time has to follow all the way round. */
void test_scheme_lifetime_mutual_recursion_reclaims_its_environment() {
    assert_reclaims_every_environment("(define (even2? n) (if (zero? n) #t (odd2? (- n 1))))"
                                      "(define (odd2? n) (if (zero? n) #f (even2? (- n 1))))"
                                      "(even2? 6)",
                                      "#t");
}

/* letrec exists to let a procedure refer to itself, which is the same thing as
 * binding a procedure into the frame it closes over. Unlike the two above, this
 * cycle is per frame -- one for every evaluation of the form. */
void test_scheme_lifetime_letrec_self_reference_reclaims_its_frame() {
    assert_reclaims_every_environment("(letrec ((loop (lambda (n) (if (zero? n) 0 (loop (- n 1))))))"
                                      "  (loop 5))",
                                      "0");
}

/* An internal define is the same shape as letrec and the ordinary way to write
 * a helper, so this cycle appears in normal code rather than clever code. Per
 * call, again. */
void test_scheme_lifetime_internal_define_reclaims_its_frame() {
    assert_reclaims_every_environment("(define (outer n)"
                                      "  (define (inner k) (if (zero? k) 0 (inner (- k 1))))"
                                      "  (inner n))"
                                      "(outer 5)",
                                      "0");
}

/* The one that decides how much machinery this needs. A per-frame cycle inside
 * a loop is not a leak of fixed size -- it is one dead environment per
 * iteration, and the iteration count is the program's, not the language's.
 *
 * Fifty here to keep the case quick; the recursion budget is what stops it
 * being more, and tail calls will lift that. */
void test_scheme_lifetime_repeated_letrec_does_not_accumulate_environments() {
    assert_reclaims_every_environment("(define (spin n)"
                                      "  (if (zero? n)"
                                      "      0"
                                      "      (begin (letrec ((f (lambda () 1))) (f))"
                                      "             (spin (- n 1)))))"
                                      "(spin 50)",
                                      "0");
}

/* The baseline the others are read against: a program with no capture at all
 * must reclaim everything, before and after any of this changes. If this one
 * ever goes red the instrument is broken, not the collector. */
void test_scheme_lifetime_plain_calls_reclaim_every_frame() {
    assert_reclaims_every_environment("(define (add a b) (+ a b))"
                                      "(add (add 1 2) (let ((x 3)) (add x 4)))",
                                      "10");
}

#pragma endregion Reference cycles

#pragma region Arguments moved into the frame

/* A call frame takes its arguments rather than copying them, so every value the
 * argument list holds changes owner exactly once. These cases are about who
 * frees what, and none of them can fail by printing the wrong answer -- a
 * mistake here is a double free or a leak, and `leaks --atExit` and ASan are
 * what read the verdict. They are written as programs anyway, because the
 * evaluator is the only thing that builds an argument list the moving path
 * touches.
 *
 * See BUILTINS.md, "An argument is moved into the frame". */

/* The value that came out is the value that went in, and it survives the frame
 * that held it. Nothing here would notice a copy; what it pins is that a moved
 * argument is still a whole, owned value at the far end. */
void test_scheme_lifetime_moved_argument_outlives_the_call() {
    assert_prints("((lambda (x) x) '(1 2 3))", "(1 2 3)");
}

/* The caller's binding is untouched by the call. A variable reference still
 * hands back a *copy* of the bound value, so what the frame moves is that copy
 * and never the binding itself -- which is why moving is safe before the symbol
 * case stops copying (issue #40). */
void test_scheme_lifetime_moved_argument_does_not_alias_the_callers_binding() {
    assert_prints("(define l '(1 2 3))"
                  "(define (take v) v)"
                  "(take l)"
                  "l",
                  "(1 2 3)");
}

/* Arity is checked before the first value changes hands, so the frame takes all
 * the arguments or none of them. Here it takes none, and the argument list is
 * still holding two lists that somebody has to free. */
void test_scheme_lifetime_arity_failure_leaves_every_argument_to_the_list() {
    assert_errors("((lambda (x) x) '(1 2 3) '(4 5 6))", "expects 1 argument(s), got 2");
}

/* The other end of the same rule, one argument short rather than one over. */
void test_scheme_lifetime_arity_failure_with_too_few_arguments() {
    assert_errors("(define (pair a b) a)"
                  "(pair '(1 2 3))",
                  "expects 2 argument(s), got 1");
}

/* An error part way through *building* the list, which is the only partial
 * state that exists: the first operand evaluated to a list and the second did
 * not evaluate at all, so the list holds one value and the frame was never
 * made. */
void test_scheme_lifetime_operand_error_frees_what_already_evaluated() {
    assert_errors("(define (take a b) a)"
                  "(take '(1 2 3) nowhere)",
                  "Unbound variable: nowhere");
}

/* The same, with the failing operand a call rather than a name, so the list is
 * unwound from underneath a nested failure. */
void test_scheme_lifetime_nested_operand_error_frees_what_already_evaluated() {
    assert_errors("(define (take a b) a)"
                  "(take '(1 2 3) (car '()))",
                  "car");
}

/* Two parameters of the same name. `environment_set` rebinds rather than
 * pushes, and frees the value it replaces -- so the first argument is released
 * by the frame that took it, and the second is the one that survives. Each
 * moved value is still freed exactly once. */
void test_scheme_lifetime_duplicate_parameter_frees_the_shadowed_argument() {
    assert_prints("((lambda (x x) x) '(1 1 1) '(2 2 2))", "(2 2 2)");
}

/* A builtin *borrows* the argument list -- only the user-procedure path moves
 * -- so a builtin returning something built out of an argument has to copy it,
 * and the list is freed whole afterwards. This is the case that would break
 * first if the two paths were ever collapsed into one. */
void test_scheme_lifetime_builtin_arguments_are_borrowed_not_moved() {
    assert_prints("(define l '(1 2 3))"
                  "(car (cdr l))",
                  "2");
}

/* A builtin that fails after its arguments are evaluated, so the list is freed
 * on an error path with everything still in it. */
void test_scheme_lifetime_builtin_failure_frees_its_arguments() {
    assert_errors("(car '(1 2 3) '(4 5 6))", "car");
}

/* A procedure passed as an argument carries a counted reference to its closure.
 * Moving the value moves that reference; copying it would have taken a second
 * one and released it again. Either way the count has to balance, and the frame
 * this makes has to be reclaimed. */
void test_scheme_lifetime_moved_procedure_argument_keeps_its_closure() {
    assert_reclaims_every_environment("(define (apply-twice f x) (f (f x)))"
                                      "(define (make-adder n) (lambda (x) (+ x n)))"
                                      "(apply-twice (make-adder 3) 10)",
                                      "16");
}

/* The list cases, at the length the benchmark measures, run enough times that a
 * value leaked per call would be visible rather than a rounding error. The
 * answer is incidental; the environment count is the assertion. */
void test_scheme_lifetime_repeated_list_arguments_reclaim_every_frame() {
    assert_reclaims_every_environment("(define (thread l n) (if (zero? n) l (thread l (- n 1))))"
                                      "(car (thread '(1 2 3 4 5 6 7 8) 50))",
                                      "1");
}

#pragma endregion Arguments moved into the frame

int main(int argc, char** argv) {
    summa_test_begin("scheme.lifetime", argc, argv);

    SUMMA_TEST_RUN(test_scheme_lifetime_closure_outlives_the_call_frame_it_captured);
    SUMMA_TEST_RUN(test_scheme_lifetime_closure_outlives_the_let_frame_it_captured);
    SUMMA_TEST_RUN(test_scheme_lifetime_closure_stored_into_its_own_frame);

    SUMMA_TEST_RUN(test_scheme_lifetime_top_level_recursion_reclaims_its_environment);
    SUMMA_TEST_RUN(test_scheme_lifetime_mutual_recursion_reclaims_its_environment);
    SUMMA_TEST_RUN(test_scheme_lifetime_letrec_self_reference_reclaims_its_frame);
    SUMMA_TEST_RUN(test_scheme_lifetime_internal_define_reclaims_its_frame);
    SUMMA_TEST_RUN(test_scheme_lifetime_repeated_letrec_does_not_accumulate_environments);
    SUMMA_TEST_RUN(test_scheme_lifetime_plain_calls_reclaim_every_frame);

    SUMMA_TEST_RUN(test_scheme_lifetime_moved_argument_outlives_the_call);
    SUMMA_TEST_RUN(test_scheme_lifetime_moved_argument_does_not_alias_the_callers_binding);
    SUMMA_TEST_RUN(test_scheme_lifetime_arity_failure_leaves_every_argument_to_the_list);
    SUMMA_TEST_RUN(test_scheme_lifetime_arity_failure_with_too_few_arguments);
    SUMMA_TEST_RUN(test_scheme_lifetime_operand_error_frees_what_already_evaluated);
    SUMMA_TEST_RUN(test_scheme_lifetime_nested_operand_error_frees_what_already_evaluated);
    SUMMA_TEST_RUN(test_scheme_lifetime_duplicate_parameter_frees_the_shadowed_argument);
    SUMMA_TEST_RUN(test_scheme_lifetime_builtin_arguments_are_borrowed_not_moved);
    SUMMA_TEST_RUN(test_scheme_lifetime_builtin_failure_frees_its_arguments);
    SUMMA_TEST_RUN(test_scheme_lifetime_moved_procedure_argument_keeps_its_closure);
    SUMMA_TEST_RUN(test_scheme_lifetime_repeated_list_arguments_reclaim_every_frame);

    return summa_test_end();
}
