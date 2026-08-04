#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include "euler.h"

/* Problem 7 -- the 10001st prime.
 *
 * Iterations: millions, counting odd candidates and trial-dividing each.
 * Needs tail calls. */
void test_scheme_euler_007_ten_thousand_and_first_prime() {
    SUMMA_TEST_TODO("needs the `>=`, `>`, `*`, `=` and `modulo` builtins, then tail calls for millions of iterations");
    assert_euler_answer(EULER_PRELUDE "(define (find-prime index current-index current-prime num)"
                                      "  (cond ((>= current-index index) current-prime)"
                                      "        ((prime? num) (find-prime index (+ current-index 1) num (+ num 2)))"
                                      "        (else (find-prime index current-index current-prime (+ num 2)))))"
                                      "(find-prime 10001 1 2 3)",
                        104743);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.euler.007", argc, argv);

    SUMMA_TEST_RUN(test_scheme_euler_007_ten_thousand_and_first_prime);

    return summa_test_end();
}
