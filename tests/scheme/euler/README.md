# Project Euler 1-10

Ported from the daybreak solutions at
<https://github.com/exokomodo/daybreak/tree/main/euler>

SPOILER WARNING, carried over from daybreak's own README: these are answers.
If you mean to solve Project Euler yourself, close this file.

── What this suite is ────────────────────────────────────────────────────

A specification first, and now half a passing suite. Problems 1, 2, 5, 6 and
9 are green; the other five still fail, and each fails with the
interpreter's own message naming what it ran out of, so the list of failures
IS the list of work remaining. As the gap closes, cases go green without
anyone editing this file.

One thing stands between here and all green:

1. Tail calls. Every solution below is recursion with an accumulator, which
   is how daybreak writes them and how Scheme wants them written -- but
   `summa_scheme_evaluate` has no tail-call optimization, so each iteration
   costs a C frame. SUMMA_SCHEME_MAX_DEPTH stops that at roughly 200 levels
   of nesting. Each problem's comment records the iteration count it needs,
   and the ones over that budget -- 3, 4, 7, 8 and 10 -- cannot pass until
   the evaluator stops growing the stack per call. Problem 10 will want a
   sieve on top of that, to finish in reasonable time rather than merely to
   finish.

The procedures are no longer a blocker: `+ - * = < > <= >= quotient
remainder modulo zero? not string-length string-ref char->integer` all
exist (see BUILTINS.md), which is every one these programs name.

── Where these differ from daybreak ──────────────────────────────────────

daybreak has structs and C interop; this Scheme has neither, so three
solutions take a different route to the same algorithm. Each is flagged at
the problem. Nothing here reaches for a feature beyond the builtin list
above -- the point is a target that is actually reachable.
