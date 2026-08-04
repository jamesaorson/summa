#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include "euler.h"

/* Problem 5 -- the smallest number divisible by every number from 1 to 20.
 *
 * Differs from daybreak: that version reads the prime list out of an array and
 * takes the exponent as floor(log 20 / log p). With no arrays and no `log`,
 * the eight primes are written out and the exponent is found by multiplying up
 * until the next power would pass 20 -- the same prime-power factorisation.
 *
 * Iterations: at most 5 per prime. Within the depth budget. */
void test_scheme_euler_005_smallest_multiple() {
    SUMMA_TEST_TODO("needs the `>` and `*` builtins");
    assert_euler_answer(EULER_PRELUDE "(define (highest-power p limit acc)"
                                      "  (if (> (* acc p) limit) acc (highest-power p limit (* acc p))))"
                                      "(* (highest-power 2 20 1)"
                                      "   (* (highest-power 3 20 1)"
                                      "      (* (highest-power 5 20 1)"
                                      "         (* (highest-power 7 20 1)"
                                      "            (* 11 (* 13 (* 17 19)))))))",
                        232792560);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.euler.005", argc, argv);

    SUMMA_TEST_RUN(test_scheme_euler_005_smallest_multiple);

    return summa_test_end();
}
