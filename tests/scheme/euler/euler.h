#ifndef SUMMA_TESTS_SCHEME_EULER_H
#define SUMMA_TESTS_SCHEME_EULER_H

/* Shared harness for the Project Euler suites in this directory.
 *
 * One suite per problem -- scheme.euler.001.test.c and friends -- so ctest
 * gets a case per problem and `ctest -R scheme.euler.003` reruns one on its
 * own. Everything they have in common lives here: the read-evaluate loop, the
 * assertion, and the prelude ported from daybreak's helpers.day.
 *
 * Include this after the three summa headers, since it uses their types:
 *
 *     #define SUMMA_TEST_IMPLEMENTATION
 *     #include <summa/test/test.h>
 *     #define SUMMA_SCHEME_IMPLEMENTATION
 *     #include <summa/scheme/scheme.h>
 *     #define SUMMA_STRING_IMPLEMENTATION
 *     #include <summa/string/string.h>
 *     #include "euler.h"
 *
 * README.md covers what these suites are for, why the ones still red are red,
 * and where they depart from daybreak. SPOILER WARNING applies to the whole
 * directory: these are Project Euler answers.
 */

#include <stdint.h>

#define SCOPED_GLOBAL_ENV(var) \
    SUMMA_TEST_SCOPED_VALUE(   \
        SummaSchemeEnvironment, var, summa_scheme_environment_make_global(), summa_scheme_environment_free)

/* Read, evaluate, discard, repeat -- the same loop as the e2e suite. The last
 * expression's value is the program's answer. */
static SummaSchemeError
euler_run_program(const SummaSchemeEnvironment env, const char* program, SummaSchemeValue* out) {
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

/* Asserts the program's answer. The failure carries the interpreter's message
 * rather than just the expression, so a red run reads as a to-do list. */
static void assert_euler_answer(const char* program, int64_t expected) {
    SCOPED_GLOBAL_ENV(env) {
        SummaSchemeValue       result = {};
        const SummaSchemeError err    = euler_run_program(env, program, &result);

        SUMMA_TEST_ASSERT_MSG(!err.had, err.message);
        if (!err.had) {
            SUMMA_TEST_ASSERT_EQ(SummaSchemeIntegerType, result.type);
            if (result.type == SummaSchemeIntegerType) {
                SUMMA_TEST_ASSERT_EQ(expected, result.value.integer.value);
            }
        }
        summa_scheme_value_free(&result);
    }
}

/* helpers.day, minus the printing. `partial-sum` is the closed form for the
 * sum of every multiple of `scale` below `n`; `prime?` is trial division,
 * squaring the factor instead of taking a square root so it stays in
 * integers. Prefixed to every program, the way each daybreak problem opens
 * with `import "euler/helpers.day"`. */
#define EULER_PRELUDE                                    \
    "(define (partial-sum n scale)"                      \
    "  (let ((limit (quotient (- n 1) scale)))"          \
    "    (quotient (* scale (* limit (+ limit 1))) 2)))" \
    "(define (prime-from? num factor)"                   \
    "  (cond ((> (* factor factor) num) #t)"             \
    "        ((= 0 (modulo num factor)) #f)"             \
    "        (else (prime-from? num (+ factor 1)))))"    \
    "(define (prime? num) (if (< num 2) #f (prime-from? num 2)))"

#endif /* SUMMA_TESTS_SCHEME_EULER_H */
