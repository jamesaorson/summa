#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include "euler.h"

/* Problem 3 -- the largest prime factor of 600851475143.
 *
 * Differs from daybreak: that version carries a FilterResult struct through a
 * 6k+/-1 wheel. With no structs here, the same divide-it-out trial division
 * is written as one flat recursion, which needs no struct because the factor
 * that last divided `n` is simply the largest so far.
 *
 * Iterations: ~775,000, up to sqrt(600851475143). Needs tail calls. */
void test_scheme_euler_003_largest_prime_factor() {
    SUMMA_TEST_TODO("needs tail calls for ~775k iterations");
    assert_euler_answer(EULER_PRELUDE "(define (lpf n factor largest)"
                                      "  (cond ((> (* factor factor) n) (if (> n 1) n largest))"
                                      "        ((= 0 (modulo n factor)) (lpf (quotient n factor) factor factor))"
                                      "        (else (lpf n (+ factor 1) largest))))"
                                      "(lpf 600851475143 2 1)",
                        6857);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.euler.003", argc, argv);

    SUMMA_TEST_RUN(test_scheme_euler_003_largest_prime_factor);

    return summa_test_end();
}
