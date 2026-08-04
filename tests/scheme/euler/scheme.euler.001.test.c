#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include "euler.h"

/* Problem 1 -- the multiples of 3 or 5 below 1000.
 *
 * Iterations: none. Inclusion-exclusion over three closed forms, so this is
 * the one problem that needs no recursion at all and is blocked on builtins
 * alone. */
void test_scheme_euler_001_multiples_of_3_and_5() {
    assert_euler_answer(EULER_PRELUDE "(define (problem1 limit)"
                                      "  (- (+ (partial-sum limit 3) (partial-sum limit 5))"
                                      "     (partial-sum limit 15)))"
                                      "(problem1 1000)",
                        233168);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.euler.001", argc, argv);

    SUMMA_TEST_RUN(test_scheme_euler_001_multiples_of_3_and_5);

    return summa_test_end();
}
