#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include "euler.h"

/* Problem 9 -- the Pythagorean triplet summing to 1000.
 *
 * Euclid's parametrisation, as daybreak does it: half the target must factor
 * as m * (m + n), so walk m down from sqrt(500) and take the first divisor
 * giving m > n > 0. The struct daybreak returns is unnecessary here because
 * the product can be formed at the point the pair is found.
 *
 * Iterations: at most 22, m from sqrt(500) down. Within the depth budget --
 * the one "search" problem that is, because Euclid replaces the search. */
void test_scheme_euler_009_special_pythagorean_triplet() {
    assert_euler_answer(EULER_PRELUDE "(define (euclid-pair target m)"
                                      "  (cond ((< m 1) 0)"
                                      "        ((= 0 (modulo target m))"
                                      "         (let* ((n (- (quotient target m) m))"
                                      "                (a (- (* m m) (* n n)))"
                                      "                (b (* 2 (* m n)))"
                                      "                (c (+ (* m m) (* n n))))"
                                      "           (if (if (> m n) (> n 0) #f)"
                                      "               (* a (* b c))"
                                      "               (euclid-pair target (- m 1)))))"
                                      "        (else (euclid-pair target (- m 1)))))"
                                      "(euclid-pair 500 22)",
                        31875000);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.euler.009", argc, argv);

    SUMMA_TEST_RUN(test_scheme_euler_009_special_pythagorean_triplet);

    return summa_test_end();
}
