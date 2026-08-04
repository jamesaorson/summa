#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include "euler.h"

/* Problem 8 -- the largest product of thirteen adjacent digits in the
 * thousand-digit number.
 *
 * The one problem here that needs a type the interpreter has no procedures
 * for: the series is a string, and reading a digit out of it wants
 * `string-ref` and `char->integer`. daybreak indexes the cstring directly and
 * subtracts 48; `char->integer` is the same subtraction spelled portably.
 *
 * Iterations: ~13,000, thirteen multiplications across 988 windows. Needs
 * tail calls and string procedures. */
void test_scheme_euler_008_largest_product_in_a_series() {
    assert_euler_answer(EULER_PRELUDE "(define series "
                                      "\"73167176531330624919225119674426574742355349194934"
                                      "96983520312774506326239578318016984801869478851843"
                                      "85861560789112949495459501737958331952853208805511"
                                      "12540698747158523863050715693290963295227443043557"
                                      "66896648950445244523161731856403098711121722383113"
                                      "62229893423380308135336276614282806444486645238749"
                                      "30358907296290491560440772390713810515859307960866"
                                      "70172427121883998797908792274921901699720888093776"
                                      "65727333001053367881220235421809751254540594752243"
                                      "52584907711670556013604839586446706324415722155397"
                                      "53697817977846174064955149290862569321978468622482"
                                      "83972241375657056057490261407972968652414535100474"
                                      "82166370484403199890008895243450658541227588666881"
                                      "16427171479924442928230863465674813919123162824586"
                                      "17866458359124566529476545682848912883142607690042"
                                      "24219022671055626321111109370544217506941658960408"
                                      "07198403850962455444362981230987879927244284909188"
                                      "84580156166097919133875499200524063689912560717606"
                                      "05886116467109405077541002256983155200055935729725"
                                      "71636269561882670428252483600823257530420752963450\")"
                                      "(define (digit-at s i) (- (char->integer (string-ref s i)) 48))"
                                      "(define (window-product s i n acc)"
                                      "  (if (= n 0) acc (window-product s (+ i 1) (- n 1) (* acc (digit-at s i)))))"
                                      "(define (largest-window s i width best)"
                                      "  (if (> (+ i width) (string-length s))"
                                      "      best"
                                      "      (let ((p (window-product s i width 1)))"
                                      "        (largest-window s (+ i 1) width (if (> p best) p best)))))"
                                      "(largest-window series 0 13 0)",
                        23514624000);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.euler.008", argc, argv);

    SUMMA_TEST_RUN(test_scheme_euler_008_largest_product_in_a_series);

    return summa_test_end();
}
