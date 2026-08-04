# Project Euler 1-10

Ported from the daybreak solutions at
<https://github.com/exokomodo/daybreak/tree/main/euler>

SPOILER WARNING, carried over from daybreak's own README: these are answers.
If you mean to solve Project Euler yourself, close this file.

── What this suite is ────────────────────────────────────────────────────

A specification, not a passing suite. Every case here fails today, and each
one fails with the interpreter's own message naming what it ran out of --
`Unbound variable: -` and so on -- so the list of failures IS the list of
work remaining. As builtins land, cases go green without anyone editing
this file.

Two things stand between here and green:

1. Procedures. `+` is the only one that exists (see BUILTINS.md). These
   programs also want `-`, `*`, `quotient`, `modulo`, `=`, `<`, `>`, `>=`,
   and for problem 8, `string-length`, `string-ref` and `char->integer`.

2. Tail calls. Every solution below is recursion with an accumulator, which
   is how daybreak writes them and how Scheme wants them written -- but
   `summa_scheme_evaluate` has no tail-call optimization, so each iteration
   costs a C frame. SUMMA_SCHEME_MAX_DEPTH stops that at roughly 200 levels
   of nesting. Each problem's comment records the iteration count it needs,
   and the ones over that budget cannot pass on builtins alone.

── Where these differ from daybreak ──────────────────────────────────────

daybreak has structs and C interop; this Scheme has neither, so three
solutions take a different route to the same algorithm. Each is flagged at
the problem. Nothing here reaches for a feature beyond the builtin list
above -- the point is a target that is actually reachable.
