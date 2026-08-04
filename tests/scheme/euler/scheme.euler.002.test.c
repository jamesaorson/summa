#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include "euler.h"

/* Problem 2 -- the even Fibonacci numbers below four million.
 *
 * Iterations: ~33, one per Fibonacci number. Within the depth budget. */
void test_scheme_euler_002_even_fibonacci_sum() {
    assert_euler_answer(EULER_PRELUDE "(define (fib-add-evens limit a b acc)"
                                      "  (cond ((> b limit) acc)"
                                      "        ((= 0 (modulo b 2)) (fib-add-evens limit b (+ a b) (+ acc b)))"
                                      "        (else (fib-add-evens limit b (+ a b) acc))))"
                                      "(fib-add-evens 4000000 0 1 0)",
                        4613732);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.euler.002", argc, argv);

    SUMMA_TEST_RUN(test_scheme_euler_002_even_fibonacci_sum);

    return summa_test_end();
}
