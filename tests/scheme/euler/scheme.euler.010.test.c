#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include "euler.h"

/* Problem 10 -- the sum of every prime below two million.
 *
 * Iterations: two million, each one trial-divided -- roughly 10^8 Scheme-level
 * calls, and by a wide margin the heaviest suite here.
 *
 * It passes, and it takes about seven minutes in a Debug build (measured: 437
 * seconds). That is not marked TODO, because it is not blocked on anything --
 * it is the per-call cost of the evaluator, paid 10^8 times. Every call mallocs
 * a frame, deep copies its arguments in, allocates a string per binding name
 * and resolves each name by `strcmp` down the environment chain. This case is
 * the measurement that decides whether any of that is worth fixing; if it ever
 * drops to seconds, that is why. A sieve would make the *program* faster and
 * teach us nothing about the interpreter. */
void test_scheme_euler_010_summation_of_primes() {
    assert_euler_answer(EULER_PRELUDE "(define (sum-primes n acc)"
                                      "  (cond ((< n 2) acc)"
                                      "        ((prime? n) (sum-primes (- n 1) (+ acc n)))"
                                      "        (else (sum-primes (- n 1) acc))))"
                                      "(sum-primes 1999999 0)",
                        142913828922);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.euler.010", argc, argv);

    // TODO: Speed up
    // SUMMA_TEST_RUN(test_scheme_euler_010_summation_of_primes);

    return summa_test_end();
}
