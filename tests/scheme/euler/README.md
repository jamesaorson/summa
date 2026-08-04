# Project Euler 1-10

Ported from the daybreak solutions at
<https://github.com/exokomodo/daybreak/tree/main/euler>

SPOILER WARNING, carried over from daybreak's own README: these are answers.
If you mean to solve Project Euler yourself, close this file.

── What this suite is ────────────────────────────────────────────────────

A specification first, and now nine green out of ten. Problem 10 is correct
and is not run -- see below.

Proper tail calls are what closed the gap. Every solution below is recursion
with an accumulator -- how daybreak writes them and how Scheme wants them
written -- and until the evaluator became a trampoline each iteration cost a
C frame, so SUMMA_SCHEME_MAX_DEPTH stopped 3, 4, 7, 8 and 10 at roughly a
thousand iterations. Each problem's comment records the iteration count it
needs; depth is no longer a limit on any of them.

What is left is not depth but time, and it is one problem's worth. Problems
4 and 7 take a few seconds each in a Debug build; the other seven are under a
second between them. Problem 10 is roughly 10^8 Scheme-level calls, which is
minutes rather than seconds, so its `SUMMA_TEST_RUN` is commented out and its
`SUMMA_TEST_TODO` says what it is actually waiting for: the evaluator's
per-call cost, not tail calls. Turning it back on is the measurement any
per-call optimization should be judged against -- which is why it keeps its
trial division rather than growing a sieve. A sieve would make the program
faster and tell us nothing about the interpreter.

The procedures are no longer a blocker: `+ - * = < > <= >= quotient
remainder modulo zero? not string-length string-ref char->integer` all
exist (see BUILTINS.md), which is every one these programs name.

── Where these differ from daybreak ──────────────────────────────────────

daybreak has structs and C interop; this Scheme has neither, so three
solutions take a different route to the same algorithm. Each is flagged at
the problem. Nothing here reaches for a feature beyond the builtin list
above -- the point is a target that is actually reachable.
