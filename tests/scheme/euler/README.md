# Project Euler 1-10

Ported from the daybreak solutions at
<https://github.com/exokomodo/daybreak/tree/main/euler>

SPOILER WARNING, carried over from daybreak's own README: these are answers.
If you mean to solve Project Euler yourself, close this file.

── What this suite is ────────────────────────────────────────────────────

A specification first, and now a passing suite. All ten are green.

Proper tail calls are what closed it. Every solution below is recursion with
an accumulator -- how daybreak writes them and how Scheme wants them written
-- and until the evaluator became a trampoline each iteration cost a C frame,
so SUMMA_SCHEME_MAX_DEPTH stopped 3, 4, 7, 8 and 10 at roughly a thousand
iterations. Each problem's comment records the iteration count it needs; none
of them is a limit any more.

What is left is not depth but time. The suite runs in about eight minutes,
and problem 10 is essentially all of it -- roughly 10^8 Scheme-level calls at
the evaluator's current per-call cost, which is a malloc'd frame, a deep copy
per argument, a string per binding name and an `strcmp` walk to resolve each
one. Problem 4 and problem 7 take about seven seconds each; the other seven
are under a second between them. Those numbers are the measurement any
per-call optimization should be judged against, which is why problem 10 keeps
its trial division rather than growing a sieve: a sieve would make the
program faster and tell us nothing about the interpreter.

The procedures are no longer a blocker: `+ - * = < > <= >= quotient
remainder modulo zero? not string-length string-ref char->integer` all
exist (see BUILTINS.md), which is every one these programs name.

── Where these differ from daybreak ──────────────────────────────────────

daybreak has structs and C interop; this Scheme has neither, so three
solutions take a different route to the same algorithm. Each is flagged at
the problem. Nothing here reaches for a feature beyond the builtin list
above -- the point is a target that is actually reachable.
