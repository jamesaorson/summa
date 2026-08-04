#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include "euler.h"

/* Problem 4 -- the largest palindrome made from two three-digit numbers.
 *
 * Differs from daybreak: that version sprintf's the product and walks the
 * characters inward. Reversing the number arithmetically says the same thing
 * without needing strings, and keeps this problem blocked on nothing but
 * arithmetic and depth.
 *
 * Iterations: ~810,000, every pair in 100..999. Needs tail calls. */
void test_scheme_euler_004_largest_palindrome_product() {
    SUMMA_TEST_TODO("needs tail calls for ~810k iterations");
    assert_euler_answer(EULER_PRELUDE
                        "(define (reverse-number n acc)"
                        "  (if (= n 0) acc (reverse-number (quotient n 10) (+ (* acc 10) (modulo n 10)))))"
                        "(define (palindrome? n) (= n (reverse-number n 0)))"
                        "(define (find-pal a b best)"
                        "  (cond ((< a 100) best)"
                        "        ((< b a) (find-pal (- a 1) 999 best))"
                        "        (else (let ((p (* a b)))"
                        "                (if (if (palindrome? p) (> p best) #f)"
                        "                    (find-pal a (- b 1) p)"
                        "                    (find-pal a (- b 1) best))))))"
                        "(find-pal 999 999 0)",
                        906609);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.euler.004", argc, argv);

    SUMMA_TEST_RUN(test_scheme_euler_004_largest_palindrome_product);

    return summa_test_end();
}
