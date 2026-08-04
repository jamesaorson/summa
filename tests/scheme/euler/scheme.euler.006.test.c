#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include "euler.h"

/* Problem 6 -- the difference between the square of the sum and the sum of the
 * squares, for 1 to 100.
 *
 * Iterations: 100, counting down through the squares. Within the depth budget,
 * though only just. */
void test_scheme_euler_006_sum_square_difference() {
    assert_euler_answer(EULER_PRELUDE "(define (sum-of-squares n acc)"
                                      "  (if (> n 0) (sum-of-squares (- n 1) (+ acc (* n n))) acc))"
                                      "(define (problem6 count)"
                                      "  (let ((total (partial-sum (+ count 1) 1)))"
                                      "    (- (* total total) (sum-of-squares count 0))))"
                                      "(problem6 100)",
                        25164150);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.euler.006", argc, argv);

    SUMMA_TEST_RUN(test_scheme_euler_006_sum_square_difference);

    return summa_test_end();
}
