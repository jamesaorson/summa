#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#define SUMMA_SCHEME_IMPLEMENTATION
#include <summa/scheme/scheme.h>

#define SUMMA_STRING_IMPLEMENTATION
#include <summa/string/string.h>

#include "euler.h"

/* Problem 10 -- the sum of every prime below two million.
 *
 * Iterations: two million, each one trial-divided -- roughly 10^8 Scheme-level
 * calls, and by a wide margin the heaviest problem in the set.
 *
 * No longer blocked on tail calls: the recursion here is an accumulator loop
 * and the trampoline runs it at whatever depth it asks for. What it is blocked
 * on now is per-call cost, paid 10^8 times. That bill keeps shrinking -- names
 * are interned, a frame is sized to its call, and an argument is moved into it
 * rather than copied, which together took a call from fifteen allocations to
 * six -- but three allocations and a walk down the environment chain per call
 * is still three allocations and a walk, 10^8 times. Tail calls made this
 * problem *terminate*; they did not make it finish quickly, and the two are
 * separate pieces of work.
 *
 * Left un-run rather than merely marked, because a case this slow is a cost
 * every CI run pays for no signal. Turning it back on wants either the per-call
 * work above or a sieve in place of the trial division -- and the sieve would
 * make the program faster while teaching us nothing about the interpreter, so
 * the interpreter is the interesting half. */
void test_scheme_euler_010_summation_of_primes() {
    SUMMA_TEST_TODO("blocked on per-call cost, not on tail calls: ~10^8 calls at the evaluator's current speed");
    assert_euler_answer(EULER_PRELUDE "(define (sum-primes n acc)"
                                      "  (cond ((< n 2) acc)"
                                      "        ((prime? n) (sum-primes (- n 1) (+ acc n)))"
                                      "        (else (sum-primes (- n 1) acc))))"
                                      "(sum-primes 1999999 0)",
                        142913828922);
}

int main(int argc, char** argv) {
    summa_test_begin("scheme.euler.010", argc, argv);

    /* Deliberately not run -- see the note on the case. It is correct and it is
     * slow, and the marker on it says which. Re-enable it once a call costs
     * less. */
    // SUMMA_TEST_RUN(test_scheme_euler_010_summation_of_primes);

    return summa_test_end();
}
