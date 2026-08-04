#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include "euler.h"

/* Problem 10 -- the sum of every prime below two million.
 *
 * Iterations: two million, each one trial-divided. The deepest ask in the set;
 * needs tail calls, and will want a sieve or a faster primality test before it
 * finishes in reasonable time. */
void test_scheme_euler_010_summation_of_primes() {
    SUMMA_TEST_TODO("needs tail calls for two million iterations");
    assert_euler_answer(EULER_PRELUDE "(define (sum-primes n acc)"
                                      "  (cond ((< n 2) acc)"
                                      "        ((prime? n) (sum-primes (- n 1) (+ acc n)))"
                                      "        (else (sum-primes (- n 1) acc))))"
                                      "(sum-primes 1999999 0)",
                        142913828922);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.euler.010", argc, argv);

    SUMMA_TEST_RUN(test_scheme_euler_010_summation_of_primes);

    return summa_test_end();
}
