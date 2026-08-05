# Builtins — the upfront set

The minimum surface needed for a usable, Turing-complete Scheme. Everything here
is a target list, not a status report — see [Current state](#current-state).

Two categories, and the split is not cosmetic. `summa_scheme_evaluate` evaluates
*every* operand via `summa_scheme_evaluate_arguments` before it calls
`summa_scheme_procedure_dispatch`. Anything that must leave an operand
unevaluated cannot live in `procedure_dispatch` — it has to be intercepted
earlier, in the `SummaSchemeListType` case of `summa_scheme_evaluate`, ahead of
the `summa_scheme_environment_get` lookup.

---

## Special forms — implemented

Dispatched from `summa_scheme_evaluate` through `SUMMA_SCHEME_SPECIAL_FORMS`,
ahead of the binding lookup. None of them reach `procedure_dispatch`.

| Form     | Why it can't be a procedure          |
| -------- | ------------------------------------ |
| `quote`  | must not evaluate its operand        |
| `if`     | must evaluate exactly one branch     |
| `define` | binds a name; operand 1 unevaluated  |
| `lambda` | body unevaluated until called        |
| `set!`   | target unevaluated                   |
| `begin`  | sequencing; matters once `set!` does |

`lambda` + `if` + `define` alone is Turing-complete. The rest are
Church-encodable in principle and present because nobody wants to write Church
numerals.

### Derivable

`let` · `let*` · `letrec` · `cond` · `and` · `or` · `when` · `unless`

Each is a macro over `lambda`/`if`, and each is an evaluator case here only
because there is no `define-syntax` yet. They sit below the line in the table so
it stays obvious which six are load-bearing.

`and` and `or` return the operand that decided them rather than a boolean, and
stop evaluating once the answer is known — which is why neither is a builtin.

---

## Procedures

Arithmetic, comparison, the string accessors and the list procedures exist; the
equality and predicate families do not. Adding one is a function of type
`SummaSchemeBuiltinFn` plus a row in `SUMMA_SCHEME_BUILTINS`; the global
environment binds a procedure per row at startup and dispatch finds it by the
same name, so there is nothing else to register. `summa_scheme_require_arity`,
`summa_scheme_require_min_arity`, `summa_scheme_require_type`,
`summa_scheme_require_arity_of_type`, `summa_scheme_require_nonempty_list`,
`summa_scheme_require_numbers`, `summa_scheme_has_floating` and
`summa_scheme_number_to_double` are already there as the shared argument
checking.

### Pairs and lists — implemented

Unbounded data structure — what makes the language usable rather than merely
complete.

`cons` · `car` · `cdr` · `list` · `null?` · `pair?`

All four constructors copy: `summa_scheme_apply` releases the argument list the
instant dispatch returns, so anything handed back still pointing into it is
freed memory. `summa_scheme_value_copy` takes a reference, which is what makes
that correct — and is why a builtin building a result out of its arguments still
has to go through it rather than aliasing the list.

`cons` takes a list as its second argument or errors, and the message names why:
`(cons 1 2)` is an improper pair, which a dynamic array has no way to be.
Answering `(1 2)` instead would be a different value wearing the same name.
Prepending therefore builds a list of length n+1 rather than a cell pointing at
the old one, so a list head nests — `(cons '(1) '(2))` is `((1) 2)`, not
`(1 2)`.

`car` and `cdr` share one checker: one operand, a list, and not the empty one.
`(car '())` is an error, but `(cdr '(1))` is `()` — an exhausted tail is the
value a recursion stops on, not a mistake. `cdr` builds a new list, so no two
values ever observe the same tail — which is now the load-bearing half of why
sharing a payload is safe. See
[`set-car!` and `set-cdr!` have nothing to mutate](#set-car-and-set-cdr-have-nothing-to-mutate).

`(list)` is `()`. `null?` is true for the empty list alone — `#()` is an empty
*vector*, a distinct type, and is not null — and `pair?` is its complement over
lists: true from one element up, false for `()` and for every other type.
Neither is an error on a non-list.

A list value can carry a null `SummaList` handle as readily as a zero-length
one, so `summa_scheme_list_length` reads both as empty rather than one of them
as a segfault.

`set-car!` and `set-cdr!` are **not** on this list. See the gotcha below.

### Equality — still to write

`eq?` · `eqv?` · `equal?`

`summa_scheme_value_equals` already backs `equal?`.

### Type predicates — one written, the rest still to write

One per arm of `SummaSchemeValueType`.

`procedure?` is implemented. It came in ahead of its family because environment
lifetime needed it: a closure stored into the frame it captured can only be
checked from Scheme by asking what came back out.

Still to write: `boolean?` · `char?` · `number?` · `integer?` · `real?` ·
`string?` · `symbol?` · `vector?`

### Arithmetic — implemented

`+` · `-` · `*` · `=` · `<` · `>` · `<=` · `>=` · `quotient` · `remainder` ·
`modulo` · `zero?`

`+`, `-` and `*` are variadic and split on `summa_scheme_has_floating`: one
inexact operand makes the result inexact. `(-)` is an error — there is no
identity to return — while `(+)` is 0 and `(*)` is 1.

The five comparisons chain the R7RS way, so `(< 1 2 3)` is #t, and require two
operands: a one-operand comparison is vacuously true and almost certainly a
typo. Two integers are compared as integers rather than through `double`, so
magnitudes past 2^53 still order.

`quotient`, `remainder` and `modulo` take exactly two *integers*. R7RS admits
integral floats there, but `SummaSchemeValue` tracks no exactness, so an
integral float is indistinguishable from one that merely rounded to look
integral — rejecting is the honest answer until there is a tower to consult.
`remainder` truncates toward zero and takes the dividend's sign; `modulo` takes
the divisor's. A zero divisor is an error, and so is `INT64_MIN / -1`, whose
answer is not representable and which C leaves undefined rather than wrapping.

`/` is deliberately absent. See the gotcha below.

### Booleans — implemented

`not`

### Strings and characters — implemented

`string-length` · `string-ref` · `char->integer`

The three euler problem 8 needs to read a digit out of a string. `string-ref`
names both the index and the length when the index is out of range, and
`char->integer` goes through `unsigned char` so a byte past 127 does not come
back negative. The rest of the string library — `substring`, `string-append`,
`string->list` — is unwritten.

### I/O — still to write

Not required for completeness; required to tell whether any of the rest works.

`display` · `newline`

`summa_scheme_display` exists and is the one to call — `summa_scheme_print`
quotes strings, which is `write`, not `display`.

---

## Gotchas

### `if` as a builtin is the trap

The natural move is to add `if` as a row in `SUMMA_SCHEME_BUILTINS` next to `+`.
That silently breaks recursion:

```scheme
(if (null? l) 0 (recurse (cdr l)))
```

Both branches would be evaluated, so this never terminates. `if` is a special
form for that reason, dispatched from `summa_scheme_evaluate` ahead of the
binding lookup.

The same reasoning applies to every form in the special-forms table. A special
form that type-checks as a procedure is a bug that only shows up at runtime, on
recursive input — `test_scheme_if_does_not_evaluate_the_untaken_alternate` is
the regression guard.

### Closures own their frames, and cycles are what that costs

A procedure captures its defining environment in
`SummaSchemeProcedure::closure`, and that capture is a *reference* — the same
rule `SummaSchemeEnvironment_t::parent` follows. Environments are reference
counted, through `ref_count` with the counter carved out of the same allocation
as the environment, so a handle is the payload and `env - 1` is its counter.

The rules, in full:

- `summa_scheme_environment_make` hands back the only reference, and retains the
  parent it was given. A child holds its parent up.
- `summa_scheme_value_copy` acquires a procedure's closure;
  `summa_scheme_value_free` releases it. Those two are the counted edge set.
- `summa_scheme_apply` and `summa_scheme_let` release the frame they made rather
  than freeing it, so a procedure created in the body keeps the frame alive by
  holding a reference of its own — and so does the trampoline, which took a
  reference before either of them let go.

That is what makes a closure escaping its frame ordinary rather than unsound:

```scheme
(define (make-adder n) (lambda (x) (+ x n)))  ; the lambda's closure is the
(define add5 (make-adder 5))                  ; call frame, and holds it up
(add5 10)                                     ; => 15
```

What it costs is cycles. A procedure bound *into* the frame it closes over holds
that frame up from inside — `environment -> binding -> procedure -> closure ->
environment` — and the count never reaches zero. `letrec`, an internal `define`
and a top-level recursive `define` are all that shape, so this is normal code
rather than clever code, and the per-frame ones leak once per evaluation rather
than once per program.

A trial-deletion cycle collector reclaims them, and lives with the environment
code in `scheme.h`. Three things worth knowing about it here:

- **It runs by itself.** `summa_scheme_environment_make` collects once the
  registry of live environments reaches a threshold, which is retuned to twice
  the survivors after every pass. `summa_scheme_environment_free` collects
  unconditionally, so a finished program leaves nothing behind even if it never
  reached the threshold. `summa_scheme_collect_cycles` is public for a host that
  wants a pass on its own schedule.
- **It is trial deletion, not mark-and-sweep**, because the roots are
  `SummaSchemeEnvironment` values in C locals across the evaluator and no
  collector can see the C stack. Trial deletion needs no roots: subtract the
  references environments hold in each other, and whatever still has a count
  left is held from outside.
- **One function enumerates an environment's outgoing references**, and both the
  destructor and the collector call it. Trial deletion's one failure mode is the
  two traversals disagreeing, and the only reliable way to stop that is to have
  one traversal. Adding a value type that can carry an environment means adding
  it to `summa_scheme_environment_for_each_edge` in the same commit.

`tests/scheme/scheme.lifetime.test.c` holds both halves: the escapes are the
specification refcounting has to meet, and the cycle cases are the regression
guard on what it costs.

#### What a shared payload costs the collector

Counted list payloads put "one traversal" under a pressure the rule above did
not anticipate, and the shape of the pressure is worth stating precisely.

A list holds **one** reference to the closure of a procedure inside it, however
many values name that list. Two environments binding the same list would each
enumerate that one reference, so step 2 of trial deletion would subtract two for
a count of one — and a trial count driven below the truth condemns environments
that are still alive. That is edge-set drift reached by a second route, and its
failure mode is corruption rather than a leak.

The rule that closes it is one line: **descend into a payload only when this is
its sole handle.** A payload with one handle belongs to whoever is enumerating,
so counting its references once is exact. A payload with more belongs to nobody
in particular, so the traversal stops at it, and the environments it points at
keep the references it holds — they then read as held-from-outside, which is the
conservative answer and the safe direction to be wrong in. The same line keeps
teardown honest: nulling a closure slot inside a payload somebody else still
holds would take that procedure's captured environment away from them.

The cost is a cycle that runs through a **shared** payload — a list bound under
two names, holding a procedure whose closure is an environment in that cycle.
Counting cannot reclaim it and the collector no longer walks into it, so it
survives the program. Two things bound it, and both matter:

- **It does not accumulate.** A shared payload built and dropped inside a loop
  is reclaimed by counting the moment its last handle goes;
  `test_scheme_lifetime_a_shared_list_holding_a_closure_does_not_accumulate`
  runs fifty iterations and reclaims every frame. What survives is the top-level
  case, which is bounded by the program text the way the pre-existing
  one-cycle-per-top-level-procedure leak is.
- **It is a leak and never a free.** Every way the conservatism can be wrong
  leaves something alive too long.

Fixing it properly means making a payload a node of the collector's graph in its
own right — its own trial count, its own registry, its own place in the marking
and breaking steps — which is a larger change than #40 was and wants its own
issue. The cheaper-looking alternatives do not survive contact: subtracting each
payload's edges once per pass loses the signal that says a payload is *also*
held by a C local, and a payload flag saying "this one can reach an environment"
turns a missed update into corruption rather than a leak.

### Tail calls are optimized; the depth guard still is not going anywhere

R7RS mandates proper tail calls, and `summa_scheme_evaluate_inner` is a
trampoline: an expression in tail position is handed *back* rather than
recursed into, and the loop goes round again with a new `(environment,
expression)` pair. A tail call therefore costs one iteration, not a C frame,
and a tail loop runs as long as its own arithmetic says it should.

Tail positions, all of them: the taken branch of an `if`, the last expression
of a `begin`, of any procedure body, of a selected `cond` clause, of a `let` /
`let*` / `letrec` body and of a `when` / `unless` body, and the last operand of
`and` or `or`. `SummaSchemeSpecialFormFn` takes a `SummaSchemeStep*` for
exactly this: a form with a tail position fills it in instead of calling
`summa_scheme_evaluate`.

Two things follow, and both are load-bearing:

- **A tail call releases the frame it leaves.** Looping forever while retaining
  every frame would trade a stack overflow for a leak. The trampoline holds one
  environment reference, and swaps it for the next one — acquire first, release
  second, since the frame being left is often what holds the next one up.
- **The procedure being called may be bound in that very frame.**
  `(define (twice f x) (f (f x)))` calls `f`, whose only owner is the frame
  `twice` is running in, so releasing the frame would free the body about to
  run. `summa_scheme_environment_get_owner` reports which environment held the
  binding and the evaluator retains it for the call. A refcount bump — not a
  copy of the body, which is the cost #27 made avoidable.

**Operands are not tail positions**, and that is why `SUMMA_SCHEME_MAX_DEPTH`
survives. `(+ n (sum (- n 1)))` has work left to do when the call returns, so
it genuinely needs a C frame per level; without the guard, a deep non-tail
recursion segfaults instead of erroring. The ceiling is around 1997 levels of
non-tail recursion, up from 665 before the trampoline — one evaluate frame per
level now rather than three. `tests/scheme/scheme.tail_calls.test.c` pins both
halves: every tail position past the old limit, and non-tail recursion still
erroring cleanly.

The runaway test that used to live in
`tests/scheme/scheme.special_forms.test.c` is written around `(+ 1 (loop))` for
the same reason. `(define (loop) (loop))` is a *tail* call, and now runs as the
correct non-terminating program it always was.

What is *not* optimized is per-call cost. A call is cheaper than it was — three
allocations rather than eight, and no copy of what it binds — but it is still
three allocations and a walk down the environment chain. Tail calls make a
long-running program terminate; they do not make it fast. `benchmarks/scheme`
prices each of those separately — see [Current state](#current-state).

### A name is a pointer, and the table that makes it one

Symbols, binding names and procedure names are **interned**: one canonical
`SummaSchemeSymbolName` record per distinct name, for the life of the process,
and every value that uses one borrows a pointer to it. Two things follow, and
both are the point.

**A name is compared by identity.** `left == right` decides two symbols where
`strcmp` used to, and that comparison is on the hot path four times over — the
special-form lookup on the head of every combination, the builtin dispatch, the
binding walk in `summa_scheme_environment_get_owner`, and `equal?` on a symbol.
The evaluator's own vocabulary is interned once by
`summa_scheme_symbols_ensure`, so those tables compare against records rather
than against literals.

**A name is never allocated twice.** Binding a parameter used to allocate a
`SummaString` for a name that already existed in the source; it now copies a
pointer. Nothing owns a name, so `summa_scheme_value_copy` copies it and
`summa_scheme_value_free` has nothing to give back — which is the rule to keep
in mind when adding a value type that carries one. It is *not* an edge for
`summa_scheme_environment_for_each_edge`: a name is uncounted and unowned, so
there is nothing there for the collector to subtract.

Three details worth knowing about the table itself:

- **A record never moves.** The bucket array doubles, but a growth rewires
  `next` and nothing else, which is what makes an interned pointer safe to hold
  indefinitely.
- **It cannot be emptied, deliberately.** Every symbol in every live value
  points into it, so a call that released it would invalidate all of them at
  once — and a table that *can* be emptied invites exactly that call. Its size
  is bounded by the distinct identifiers a program mentions rather than by how
  long the program runs. `summa_scheme_symbol_interned_count` reports it.
- **`SummaHashMap` was the other candidate and does not fit.** It keys on the
  hash code alone, so two colliding names would be one entry; and its key must
  be a fixed size, which a name is not. `summa_hash` — djb2, from
  `summa/hash_set/hash_set.h` — is the part that *is* reused, and each record
  caches its own hash so a hash-based binding lookup has nothing to recompute.

### `set-car!` and `set-cdr!` have nothing to mutate

Lists are `SummaList` — dynamic arrays, not cons cells. Two consequences:

- `(cons 1 2)` has no representation. There are no improper pairs, so `cons`
  takes a list as its second argument or errors.
- There is no shared *structure*. `cdr` builds a new list, so no two values ever
  observe the same tail, and a mutation through one of them could not be seen
  through another. `set-car!` would type-check and do nothing observable.

Shipping them would be worse than omitting them. Implementing them for real
means cons cells, which means reference counting — and cons cells can point at
each other, so it means the cycle collector too. Environments already have both;
this is the same machinery pointed at a second type.

#### And now there is shared *representation*, which is a different thing

A payload is shared the moment two values name it, which since #40 is every
variable reference — `(define ys xs)` gives one list two names, and
`((lambda (l) l) xs)` gives it two for the length of a call. Nothing can observe
that today, and the reason is exactly the paragraph above: **there is no
operation in this language that writes through a payload.** `cdr` builds,
`cons` builds, `set!` replaces a binding rather than editing a value, and
`set-car!`/`set-cdr!` do not exist.

That is a property of the current builtin set, not of the design, and it is the
one thing sharing depends on. Whoever adds an operation that mutates a list,
vector or string in place is adding it to a value that other names already hold,
and has to answer for that before writing the function:

- **Copy on write**, which needs the count — `summa_scheme_list_ref_count` is
  already there — and a rule for what "sole owner" means when the evaluator has
  a handle in flight.
- **Or mutate, and mean it**, which is R7RS's answer and makes the sharing
  observable on purpose. Then `(define ys xs)` aliasing is the semantics rather
  than an implementation detail, and the copies `cons` and `cdr` make become the
  surprising part.

Either is defensible. Adding `set-car!` without picking one is how a language
gets aliasing nobody designed.

### `/` has no correct answer with the current value types

R7RS `(/ 1 3)` is the exact rational ⅓. `SummaSchemeValue` carries `int64_t` and
`double` and nothing else.

Pick one — coerce to double, or truncate — and pick it before callers start
depending on the behavior. Document the divergence from R7RS either way.

### Numeric tower

`integer?` and `real?` above are the two arms that exist. R7RS has
`exact?`/`inexact?`, rationals, and complex numbers. Nothing here needs them, but
the predicate names should not imply a tower that isn't there.

---

## Current state

**Done.** All fourteen special forms, dispatched through
`SUMMA_SCHEME_SPECIAL_FORMS`. User-defined procedures, with lexically scoped
closures and arity checking. Application from either operator position — a bound
name (`(f 1)`) or an expression (`((lambda (x) x) 1)`), both routed through
`summa_scheme_apply`. Counted payloads, which is what lets values outlive the
frame that produced them without being walked to get there.
`summa_scheme_read`, so a program can be written as source rather than built as
values.

Twenty-three builtins: the arithmetic, comparison, boolean, string and list sets
above, plus `procedure?`. Recursion has a numeric base case now, which is what
the euler suites were waiting on.

Environment lifetime is settled: reference counted frames, closures that own
what they captured, and a trial-deletion collector for the cycles that
counting cannot reclaim. Covered by
`tests/scheme/scheme.special_forms.test.c`, `tests/scheme/scheme.builtins.test.c`
and `tests/scheme/scheme.lifetime.test.c`.

Proper tail calls, through the trampoline in `summa_scheme_evaluate_inner`. Nine
of the ten euler suites run and pass; a tail loop iterating a million deep is
ordinary now, where the old ceiling was 998.

Symbols, binding names and procedure names are interned, so a name is a pointer
compare and costs one allocation for the life of the process rather than one per
binding per call. See
[A name is a pointer](#a-name-is-a-pointer-and-the-table-that-makes-it-one).
`tests/scheme/scheme.symbols.test.c` is where the table's behaviour is pinned.

A call frame's two arrays are sized to the call rather than to
`SUMMA_ARRAY_DEFAULT_CAPACITY`. The arity is known before either exists — the
operator's position in the form gives the argument count, and the arity check
gives the binding count — so both are one exact allocation, and a call that
binds nothing allocates no element storage at all. See
[A frame is the size of its call](#a-frame-is-the-size-of-its-call).

An argument is **moved** into the frame rather than copied into it, and the two
array headers a call used to allocate are now fields of things that were being
allocated anyway. A call is three allocations where it was eight, and binding a
list no longer walks it. See
[An argument is moved into the frame](#an-argument-is-moved-into-the-frame).

A value is copied by **retaining** it. A list, a vector and a string carry a
reference-counted payload, so naming one costs a counter increment rather than a
walk, and the last thing that scaled with the size of a value rather than the
number of calls is gone. See
[A value is copied by retaining it](#a-value-is-copied-by-retaining-it).

**Not done.** `/`, the equality procedures, the remaining type predicates,
`display` and `newline`.

The remaining structural gap is not depth any more, it is *speed*, and euler
problem 10 is where it shows: roughly 10^8 Scheme-level calls, each one
mallocing a frame and finding both the procedure and each parameter by walking a
linked chain of environments. That case is left un-run for exactly that reason.

All three of the obvious buys have had their measurement now, and they did not
agree. Interning was worth about half of every case. Arguments moved rather than
copied is worth a quarter to two fifths, and the allocations that went with it
were worth more than the copy. A binding lookup that is not a linear scan was
built, measured and taken back out; see
[The binding lookup is still a scan](#the-binding-lookup-is-still-a-scan-and-the-measurement-is-why).
The *other* copy — a variable reference duplicating the value it names — was the
fourth, and the largest of the four on the cases it touched; it is gone, and
what is left is not a copy at all. See
[A value is copied by retaining it](#a-value-is-copied-by-retaining-it).

That measurement exists. `benchmarks/scheme` is a set of programs written to
isolate one cost each, printing wall time, allocations and bytes per call; it is
built by the ordinary build and run by hand (`make benchmark`), never by
`ctest`, because a benchmark asserts nothing and a CI run should not pay for
one. Every case comes in a pair or a series, since the reading that travels
between machines is the *ratio* between two of them. What it says about the
evaluator as it stands, Release, on an M-series Mac, with the columns to its left
being what it said before interning, before frames were right-sized, and before
arguments were moved:

| Reading                                        | Before interning               | Interned                       | Right-sized                       | Arguments moved                   | Now                                       |
| ---------------------------------------------- | ------------------------------ | ------------------------------ | --------------------------------- | --------------------------------- | ----------------------------------------- |
| a user procedure call                          | ~1.2 µs, 15 allocations, 2 KB  | ~0.63 µs, 11 alloc., 1.9 KB    | ~0.62 µs, 11 allocations, 0.64 KB | ~0.31 µs, 6 allocations, 0.50 KB  | ~0.38 µs, 6 allocations, 0.50 KB          |
| a call binding nothing, over the loop it is in | +356 ns, +5 allocations, 832 B | +186 ns, +5 allocations, 832 B | +137 ns, +3 allocations, 128 B    | +47 ns, +1 allocation, 88 B       | +55 ns, +1 allocation, 88 B               |
| each argument bound                            | +123 ns, +2 allocations        | +38 ns, **+0 allocations**     | +56 ns, +0 allocations, +88 B     | +43 ns, +0 allocations, +88 B     | +43 ns, +0 allocations, +88 B             |
| 32 lexical frames rather than 1                | +52% per call                  | +40% per call                  | +42% per call                     | +54% per call                     | +47% per call                             |
| the last of 256 globals rather than of 8       | +117% per call                 | +41% per call                  | +42% per call                     | +51% per call                     | +47% per call                             |
| a 128-element list argument rather than 1      | 3.3× the time, 9.5× the bytes  | 5.2× the time, 9.7× the bytes  | 5.1× the time, 16.5× the bytes    | 4.0× the time, 14.1× the bytes    | **1.00× the time, 1.00× the bytes**       |

The two rightmost columns come from one alternated measurement of the same
binaries, so they are differences rather than two separate readings of the
machine; the three on the left are as they were published. The "arguments
moved" column reads 4.0× and 14.1× where #32 published 4.1× and 14.1×, which is
the same binary measured on a faster day and is the reason the two columns being
compared have to come from one run.

Three rows want reading carefully.

**The 128-element row did not shrink, it went away.** Its ratio had risen for
two releases running — 3.3× to 5.2× to 5.1× — because everything being removed
was *fixed* per-call cost, which left the O(length) copy a larger share of what
was left. Moving the argument took one of the two copies away and brought it to
4.0×. Counting the payload takes the other, and threading a 128-element list
through a call now costs **491 ns and 632 bytes against the 1-element case's 491
ns and 632 bytes** — the same numbers, to the last significant figure, because
nothing the evaluator does to run that program depends on the length any more.
The series `args/list-thread-{1,16,128}` exists to be read as a flat line, and
for the first time it is one.

**`args/list-walk` fell by half and cannot go flat**, and the difference between
those two sentences is the language rather than the evaluator. `cdr` builds a
new list of length n-1, so walking n elements copies n(n-1)/2 of them however
cheap a reference is. That is
[`set-car!` and `set-cdr!` have nothing to mutate](#set-car-and-set-cdr-have-nothing-to-mutate)
again from the other side: cons cells are what would fix it.

**A user procedure call got slower, and it is the compiler.** 0.31 µs to
0.38 µs is GCC 13.4 at `-O3`, where nothing about that case touches a counted
payload — the frame, the two integer bindings and the arithmetic are byte for
byte the same work at the same six allocations. The same source under Apple
clang is 338 ns before and 340 ns after, and under **GCC at `-O2`** it is 372 ns
before and 367 ns after. `-O3` finds something in the old shape of
`summa_scheme_value_copy` that it does not find in the new one, and it is worth
about 17% of the cases that allocate a frame. See
[What it cost, and what turned out not to be the cost](#what-it-cost-and-what-turned-out-not-to-be-the-cost)
— a control build settles that it is not the counting.

### A frame is the size of its call

The benchmark found a fourth buy the list of three did not have, and it has been
taken. A call binding nothing used to allocate 832 bytes, of which 704 were
element storage at `SUMMA_ARRAY_DEFAULT_CAPACITY` — eight `SummaSchemeValue`
slots of argument list and eight `SummaSchemeBinding` slots of frame bindings,
for a call with no arguments and no bindings. Both arrays grew to eight because
that was the default, not because anything asked for eight.

Nothing had to be discovered to fix it, only used. `summa_scheme_evaluate_arguments`
knows the argument count from the form before it evaluates the first one;
`summa_scheme_procedure_frame` has just checked the arity against the parameter
list. `summa/array.h` grew `summa_array_make_with_capacity` and
`summa_array_reserve` — a capacity of zero allocates no element storage at all,
which is the whole of the nullary case — and `summa_scheme_environment_make` is
now `summa_scheme_environment_make_with_capacity(parent, 0)`, with the two
callers that know better saying so. `let` reserves its clause count and the
global frame reserves the builtins.

That leaves **128 bytes** for a frame that binds nothing: the environment, and
the two `SummaArray_t` headers. Growth is unaffected — an exact fit is full, so
a body with an internal `define` doubles from there the ordinary way, and
`tests/scheme/scheme.environment.test.c` pins that alongside the sizing itself.

The honest part: **wall time rose by up to 5% on the cases that still allocate
a frame**, and fell 5% on the one case where the allocation goes away entirely.
That is the allocator rather than the evaluator, and the benchmark can show it —
a build carrying every one of these code changes but with the old capacities
restored tracks the baseline to within a percent on every case, so the block
sizes are the only thing that moved. Darwin's `malloc` charges slightly more for
a 144-byte block than for a 384-byte one in this access pattern. The trade taken
is up to 5% of time for 27–72% of the bytes, because bytes per call are what
stop euler 10 and 3% is not.

The reading that points at what to do next is the nullary row: the case that got
*faster* is the one where a `malloc`/`free` pair disappeared, not the one where
it shrank. A frame is still five allocations. Folding the binding storage into
the environment's own block would remove one outright rather than shrink it, and
on this evidence that is worth more than any amount of sizing. That prediction
was taken up next, and it was right — see
[An argument is moved into the frame](#an-argument-is-moved-into-the-frame).

Walking a list is bounded by memory rather than by `SUMMA_SCHEME_MAX_DEPTH` when
the walk is written tail recursively; written the other way —
`(cons (f (car l)) (map f (cdr l)))` — it still costs a C frame per element, and
always will, because it has work left to do after the call.

### An argument is moved into the frame

`summa_scheme_apply` evaluates the operands into an argument list, and the frame
then **takes** each value rather than copying it. The values were evaluated a
moment ago, for this call and nothing else, so no copy was ever buying anything
— and for a list argument the copy was the whole list, walked and reallocated on
every call.

The rule is one sentence and the rest follows from it. **The frame takes all the
arguments or none of them.** Arity is the only thing that can fail and it is
checked before the first value changes hands; `args->length` goes to zero
*before* the first move rather than after the last, so there is no instant at
which the list and the frame would both free the same value. The storage does
not move — only who owns what is in it.

Three things stay exactly as they were, and each one is load-bearing:

- **A builtin still borrows.** `summa_scheme_procedure_dispatch_global` gets the
  same list and the teardown after it is unconditional, so a builtin returning
  something built out of an argument still has to copy it. Only the
  user-procedure path moves.
- **A host still gets a copy.** `summa_scheme_procedure_dispatch` is the entry
  point for a caller that already has the arguments, and those arguments are the
  caller's. Both paths go through one frame builder with a `move` flag, so there
  is one piece of code to keep correct rather than two.
- **`summa_scheme_environment_set` still frees what it replaces**, which is what
  makes a duplicated parameter name — `(lambda (x x) …)` — release the first
  moved value exactly once, from the frame that took it.

`tests/scheme/scheme.lifetime.test.c` carries eleven cases for this, and most of
them are failures rather than successes: arity wrong in both directions, an
operand that errors after an earlier one has already allocated, a builtin
failing with everything still in the list. None of them can go red by printing
the wrong answer. What reads the verdict is `leaks --atExit` and ASan.

#### Fewer allocations, not smaller ones

[A frame is the size of its call](#a-frame-is-the-size-of-its-call) ended by
predicting that making an allocation *disappear* would be worth more than
shrinking one, on the evidence that the only case that got faster there was the
one where a `malloc`/`free` pair went away. That prediction is the larger half of
this change.

Two of a call's five allocations were `SummaArray_t` headers — thirty-two bytes
each, malloc'd and freed per call, sitting beside an owner that was being
allocated anyway. `summa/array.h` grew `summa_array_init_with_capacity` and
`summa_array_dispose`: `make_with_capacity` and `free` for a header the caller
supplies. `init` writes into storage it did not allocate; `dispose` releases the
elements and leaves the header behind emptied rather than dangling, so an owner
tearing itself down around one can still enumerate it. Nothing else in the header
changes, growth included — `elements` is an ordinary heap pointer and `realloc`
never knew where the header lived.

Two owners take it up:

| Owner                                | Was                            | Is                                                                                     |
| ------------------------------------ | ------------------------------ | -------------------------------------------------------------------------------------- |
| `SummaSchemeEnvironment_t::bindings` | a pointer to a malloc'd header | the header, inside the block `ref_count` already allocated                             |
| `summa_scheme_apply`'s argument list | a malloc'd header              | a C local — an argument list is wanted for exactly the length of the call that made it |

A call is **three** allocations now, and a call that binds nothing and passes
nothing is **one**: 88 bytes of `RefCount` header plus
`SummaSchemeEnvironment_t`, and nothing else at all.

#### What it measured

Release, minimum of thirty measured runs per binary, the binaries alternated, ten
rounds of `--repeat 3` each — the method [A frame is the size of its
call](#a-frame-is-the-size-of-its-call) landed on, because a single best-of-three
cannot separate a 3% effect from this machine's drift. Homebrew GCC 13.4 on an
M-series Mac, with Apple clang alongside.

**Every case is faster under both compilers and no case is slower** — 14% to
42%, with the two extremes being `args/list-thread-128` at −41% (GCC) / −42%
(clang) and `lookup/globals-256` at −20% / −14%. Allocations per call fall on
every case too — by five where a call binds scalars, by twenty on
`args/list-thread-128` — and bytes per call by 14% to 64%.

The interesting part is not the total, it is which piece bought what. Three
builds between the baseline and the tip, each adding one thing, and they do not
overlap at all:

| Piece                        | The list cases                    | The fixed-cost cases           |
| ---------------------------- | --------------------------------- | ------------------------------ |
| the exact-capacity list copy | −16 to −20% on `list-thread-128`  | +1 to +5%, i.e. slightly worse |
| the move                     | a further −25 to −27%             | ~0                             |
| the two folded headers       | −1 to −5% on the two longest      | **−17 to −31%, every one**     |

**The copy work buys the copies and the allocation work buys the calls, and
neither buys the other.** `args/list-thread-128` is dominated by walking a
128-element list, so removing a `malloc` from the frame is worth 1% there;
`call/nullary` allocates nothing per element, so removing the copy is worth 0%
there. Two separate costs that happened to be in the same issue.

That makes the folded headers the larger half of this change by a wide margin,
which is [A frame is the size of its
call](#a-frame-is-the-size-of-its-call)'s lesson holding for a second time: at
this scale an allocation *removed* beats an allocation shrunk, and here it also
beats a copy removed on every case that is not itself a copy.

Two honest notes.

**The exact-capacity list copy costs 1–5% on the cases that do not copy lists**,
on both compilers, and that is the same allocator effect
[A frame is the size of its call](#a-frame-is-the-size-of-its-call) documents: a
procedure body is a list, so copying a procedure value now asks Darwin's `malloc`
for a smaller block than it used to, and a smaller block is not a cheaper one
here. It is bought back several times over by the allocations the next commit
removes, and the tip is faster than the baseline on every case — but the line
item is real and it is a cost, not a win.

**A control build settles what the move is worth.** The tip's own source with
`move` turned back to `false`: same call graph, same inlining, same allocation
counts, only the `summa_scheme_value_copy` back. It is 38–40% slower on
`args/list-thread-128` under both compilers, which is the move, and it is 5–9%
slower on the scalar cases *under GCC only* — clang has it between −3% and +3%
there. So the scalar reading is the compiler's control-flow layout rather than
the copy, exactly as
[The binding lookup is still a scan](#the-binding-lookup-is-still-a-scan-and-the-measurement-is-why)
found. Three changes running now have turned out to be dominated by something
that was not the evaluator, and a control build is what says which term is being
read.

### A value is copied by retaining it

`summa_scheme_evaluate`'s symbol case used to end with a deep copy of the value
the name was bound to. Every mention of a variable therefore walked and
reallocated whatever it named, and for a list argument threaded through a loop
that was the whole list, once per call, forever. It was the last cost in the
evaluator that scaled with the *size of a value* rather than with the number of
calls.

Three payloads are counted now, and the counter is carved out of the same
allocation the way `SummaSchemeEnvironment`'s is — so the handle is the payload,
`handle - 1` is its `RefCount`, and nothing gained an indirection or a malloc:

| Value type              | Payload             | Copying one is |
| ----------------------- | ------------------- | -------------- |
| `SummaSchemeListType`   | a `SummaList`       | a retain       |
| `SummaSchemeVectorType` | a `SummaList`       | a retain       |
| `SummaSchemeStringType` | a `SummaString`     | a retain       |

The rest were already free. A boolean, a character, a float and an integer are
copied by value because they *are* the value, and a symbol has been an interned
pointer since #31. `summa_scheme_value_copy` becomes a retain, and
`summa_scheme_value_free` the matching release; the symbol case in the evaluator
did not change at all, because what it calls did.

One rule holds the whole thing up, and it is worth stating in the form that
breaks if it stops being true: **a `SummaList` that a `SummaSchemeValue` points
at comes from `summa_scheme_list_make_*`.** A plain `summa_list_make_empty()`
handed to `summa_make_scheme_list` produces a value in front of an allocation
with no counter in front of *it*, and the first release walks off the head of
the block. Two `SummaList`s here are deliberately uncounted, and neither is ever
inside a value: a procedure's `body`, and the argument list a call evaluates
into.

#### What this does to procedures, which the issue set aside

A procedure value is still copied deep: its parameter list is duplicated and its
body is walked. That is deliberate, and the reason is not the copy.

A body is a `SummaList` of `SummaSchemeValue`, so counting it would be the same
mechanism with no new machinery — but a shared body is a shared payload that can
reach an environment, which is the one case
[the collector has to be conservative about](#what-a-shared-payload-costs-the-collector),
and a body is exactly where a nested `lambda` lives. Counting bodies would make
that case the common one rather than the rare one. It also buys less than it
looks: the operator position of a call resolves through the binding without
copying anything, so a body is only walked when a procedure is passed as an
argument or bound to a second name.

There is one thing it *would* buy, and it is a bug rather than a benchmark.
`(define (f) (set! f 2) 7)` frees the running procedure's body from inside the
call, and `summa_scheme_evaluate_sequence_tail` then reads the next expression
out of freed memory — PR #29 flagged it, and **counting the payloads does not
fix it**, because the thing freed is the one list in a value that is not
counted. The same shape through a *list* is fixed:
`(define (grab l) (set! xs '(9)) (car l))` called as `(grab xs)` was safe before
only because the argument was a copy, and is safe now because the argument is a
handle and `summa_scheme_environment_assign` releases rather than frees.
`test_scheme_lifetime_rebinding_a_shared_list_does_not_free_it` pins that half.
The procedure half wants counting the body, and wants the collector question
answered first.

#### What it cost, and what turned out not to be the cost

Release, minimum of ten rounds of `--repeat 3` per binary, the binaries
alternated — the method the last three changes here settled on, because a single
best-of-three cannot separate a 3% effect from this machine's drift. Homebrew
GCC 13.4 on an M-series Mac, with Apple clang alongside.

| case                   | GCC before |  GCC after |      Δ | clang before | clang after |      Δ |
| ---------------------- | ---------: | ---------: | -----: | -----------: | ----------: | -----: |
| `call/tail-loop`       |      309.5 |      375.2 | +21.2% |        338.3 |       340.2 |  +0.6% |
| `call/nullary`         |      178.4 |      215.3 | +20.7% |        194.5 |   **191.0** |  −1.8% |
| `call/ternary`         |      243.1 |      279.4 | +14.9% |        263.3 |   **252.5** |  −4.1% |
| `args/list-thread-1`   |      526.1 |  **491.0** |  −6.7% |        562.2 |   **434.0** | −22.8% |
| `args/list-thread-16`  |      724.4 |  **486.8** | −32.8% |        820.9 |   **435.4** | −47.0% |
| `args/list-thread-128` |     2113.9 |  **489.4** | −76.8% |       2708.1 |   **438.2** | −83.8% |
| `args/list-walk-32`    |     1003.9 |  **588.4** | −41.4% |       1196.3 |   **552.3** | −53.8% |
| `args/list-walk-256`   |     3842.3 | **1421.1** | −63.0% |       5076.0 |  **1582.4** | −68.8% |
| `lookup/chain-1`       |      334.6 |      382.2 | +14.2% |        355.6 |   **329.2** |  −7.4% |
| `lookup/chain-32`      |      515.6 |      560.0 |  +8.6% |        468.1 |   **432.4** |  −7.6% |
| `lookup/globals-8`     |      350.3 |      391.8 | +11.8% |        372.0 |   **347.1** |  −6.7% |
| `lookup/globals-256`   |      530.4 |      577.0 |  +8.8% |        549.9 |   **523.0** |  −4.9% |
| `symbols/wide-frame`   |      526.5 |      590.7 | +12.2% |        569.0 |   **556.9** |  −2.1% |

Allocations and bytes are exact and identical on both compilers, and they are
where the change is least ambiguous:

| case                   | allocs before | allocs after | bytes before | bytes after |  Δ bytes |
| ---------------------- | ------------: | -----------: | -----------: | ----------: | -------: |
| `args/list-thread-1`   |         11.00 |     **7.00** |        776.1 |       632.1 |   −18.6% |
| `args/list-thread-16`  |         11.00 |     **7.00** |       1976.2 |       632.1 |   −68.0% |
| `args/list-thread-128` |         11.00 |     **7.00** |      10936.4 |   **632.3** |   −94.2% |
| `args/list-walk-32`    |         15.59 |     **8.71** |       3780.6 |      1087.0 |   −71.2% |
| `args/list-walk-256`   |         18.92 |     **8.97** |      29090.8 |      5571.7 |   −80.9% |

Every other case is byte for byte identical, which is the first thing the
control build has to explain.

**`args/list-thread` is flat.** 491.0 / 486.8 / 489.4 ns under GCC and 434.0 /
435.4 / 438.2 under clang, for 1, 16 and 128 elements, at 7 allocations and 632
bytes in every one of the six. Threading a list through a call has stopped being
a function of its length, which is the whole of issue #40 stated as a number.

**And the scalar cases went the other way under GCC**, by 9 to 21%, while clang
has the same source between −7% and +1%. That is the third change running whose
headline number is partly the toolchain, so it got the same treatment.

##### The control build

`SUMMA_SCHEME_COPY_PAYLOADS` is a compile-time switch on the tip's own source
that turns `summa_scheme_value_copy` back into a walk. It is not a supported
configuration; it exists so that one binary's worth of code can be measured with
and without the sharing. Everything else is identical — the counter still sits
in front of every payload, the allocation shape is the same, and a freshly built
payload starts at a count of one, so the release in `summa_scheme_value_free`
reclaims it either way.

It says two things, and they are cleanly separable:

| Comparison                     | What it isolates                 | Scalar cases                    | `args/list-thread-128`   |
| ------------------------------ | -------------------------------- | ------------------------------- | ------------------------ |
| control → after (same binary)  | the sharing, and only that       | GCC 0%, clang +2%               | GCC −80.9%, clang −83.9% |
| before → control (same result) | the new code's shape, no sharing | GCC +15 to +21%, clang −3 to 0% | GCC +21%, clang +0.4%    |

**The sharing is worth nothing at all on the cases that share nothing, and four
fifths on the case built to share.** That is the result. The GCC scalar
regression is carried by the control build in full, so it is not the retain, the
release, or the counter — it is what GCC 13.4 does with the code around them.

`-O2` settles the rest of it. The same GCC, the same two source trees, minimum
of eight alternated rounds:

| case                   | `-O2` before | `-O2` after |      Δ |
| ---------------------- | -----------: | ----------: | -----: |
| `call/tail-loop`       |        371.9 |       367.4 |  −1.2% |
| `call/nullary`         |        215.9 |       218.2 |  +1.1% |
| `call/ternary`         |        290.6 |       278.3 |  −4.2% |
| `args/list-thread-128` |       2814.2 |   **493.4** | −82.5% |
| `lookup/chain-32`      |        590.8 |       546.0 |  −7.6% |
| `symbols/wide-frame`   |        615.0 |       609.6 |  −0.9% |

There is no regression at `-O2` — and the *old* code is what `-O3` was
flattering, not the new code that `-O3` is hurting: `call/tail-loop` goes 371.9
→ 309.5 when the old source is given `-O3`, and 367.4 → 375.2 when the new one
is. The new shape is simply insensitive to the flag. Read against clang, which
sees none of it, the honest summary is that **GCC 13.4 at `-O3` had found
something in a deep-copying `summa_scheme_value_copy` that it cannot find in a
retaining one**, and it is worth about 17% of a frame. The trade is that against
a case that fell by four fifths and an allocation count that no longer depends
on the data.

### The binding lookup is still a scan, and the measurement is why

`summa_scheme_environment_get_owner` finds a binding by scanning a frame, then
walking to the parent and scanning again. That was the third of the three buys,
and it was written up as the largest of them: the last of 256 globals cost
**+113%** per call over the last of 8.

Interning took two thirds of that away. The +113% was mostly `strcmp`, and once
a name was a pointer the same reading was **+41%**. What survives is the walk
itself, and it is real — but it is smaller than the issue was written around,
and small enough that what a hash table costs to consult started to matter as
much as what it saves.

So it was built and measured rather than argued about. An open-addressed index
over the frame's existing bindings array, keyed on the djb2 hash each interned
name already carries, built only for a frame past a size threshold, and sized to
the frame at the moment the frame is made. It works, it is correct under every
test written for it, and it costs the frame no bytes at all — the index pointer
fits in the padding freed by narrowing the collector's `trial_count`. **It is
not here**, because on the one measurement that matters it loses.

Release, min of thirty measured runs per binary, the binaries alternated,
because a single best-of-three cannot separate a 3% effect from this machine's
drift. Homebrew GCC 13.4 on an M-series Mac, with Apple clang alongside it
because the first result needed a second opinion:

| case                   | GCC before | GCC indexed |      Δ | clang before | clang indexed |      Δ |
| ---------------------- | ---------: | ----------: | -----: | -----------: | ------------: | -----: |
| `call/tail-loop`       |      617.3 |       668.2 |  +8.2% |        641.4 |         668.6 |  +4.2% |
| `call/nullary`         |      375.6 |       410.6 |  +9.3% |        377.4 |         393.8 |  +4.3% |
| `call/ternary`         |      468.8 |       508.8 |  +8.5% |        470.3 |         496.8 |  +5.6% |
| `args/list-thread-1`   |     1010.6 |      1094.8 |  +8.3% |       1028.2 |        1060.7 |  +3.2% |
| `args/list-thread-16`  |     1541.1 |      1640.3 |  +6.4% |       1796.1 |        1840.9 |  +2.5% |
| `args/list-thread-128` |     5153.0 |      5275.5 |  +2.4% |       6680.0 |        6739.9 |  +0.9% |
| `args/list-walk-32`    |     2074.2 |      2126.7 |  +2.5% |       2427.5 |        2459.0 |  +1.3% |
| `args/list-walk-256`   |     8128.2 |      8219.3 |  +1.1% |      10525.4 |       10613.7 |  +0.8% |
| `lookup/chain-1`       |      630.4 |       657.4 |  +4.3% |        634.1 |         675.7 |  +6.6% |
| `lookup/chain-32`      |      898.9 |       924.1 |  +2.8% |        804.8 |         823.6 |  +2.3% |
| `lookup/globals-8`     |      647.1 |       682.5 |  +5.5% |        645.2 |         665.1 |  +3.1% |
| `lookup/globals-256`   |      903.6 |   **691.8** | −23.4% |        916.9 |     **678.7** | −26.0% |
| `symbols/wide-frame`   |      961.9 |      1013.7 |  +5.4% |        993.0 |        1042.1 |  +4.9% |

Bytes per call and allocations per call are identical everywhere, to two decimal
places, on both compilers. Nothing here is the allocator.

**Twelve cases get worse and one gets better.** The one that gets better is the
one the index was written for, and it gets 25% better, which is a real win over a
real cost. It is not enough. A program has to have hundreds of top-level
definitions *and* name a late one from a hot loop before the index has anything
to do; the euler suites reach a few dozen names and stay there, so every one of
them would pay the 3–5% and collect nothing. Euler 10 is 10^8 calls against a
global frame of about thirty bindings — precisely the shape that loses.

Three things the measurement said that the issue did not, and they are the part
worth keeping:

**Asking each frame whether it is indexed costs more than scanning it.** The
first version tested `scope->index` inside the chain walk, which is the obvious
place. That test — one predictable branch, per frame, per reference — cost
**7–13%** of every case. Confining the index to *root* frames, so the chain walk
is untouched and the test is paid once per lookup at the end of it, brought that
back to the 3–5% above. A frame's lookup is a few pointer compares, and there is
not room in it for a decision.

**Half of what GCC charged was the shape of the code, not the work in it.** A
control build — the *baseline* source, with `get_owner`'s one chain loop written
as "the chain, then the root", the same scan in the same order returning the same
answer, no index and no new field — costs +5 to +7% under GCC 13.4 and **nothing
at all** under clang, which tracks the baseline to within 0.4% on every case.
That is worth knowing before the next person reads a five-percent regression in
this evaluator as a fact about the evaluator. It joins the allocator finding from
[A frame is the size of its call](#a-frame-is-the-size-of-its-call): at this
scale the toolchain is a term in the measurement, and a control build is how you
find out which term.

**The chain is not the frame, and hashing cannot touch the chain.** Thirty-two
`let` frames of one binding each cost +42% per call over one frame, and every one
of those frames is searched in a single comparison. There is nothing in them to
hash. What would fix that reading is resolving a reference to a `(depth,
position)` pair when the body is first seen, so a lookup is two array indexes and
no search at all — lexical addressing, which is a different change, larger than
this one, and the only one of the two that touches `lookup/chain-32`.

One footnote on the in-repo candidate, since the issue named it.
`summa/hash_map/hash_map.h` is not usable here, and not for the reason
[A name is a pointer](#a-name-is-a-pointer-and-the-table-that-makes-it-one)
gives about the intern table. It is worse: `summa_hash_map_get` finds its key
through `summa_array_index_of`, which is a **linear scan** over the stored hash
codes. `SummaHashMap` is an association list keyed by hash, so it would have been
slower than the scan it replaced as well as wrong on a collision.

The implementation is not lost — it is one commit back on the branch that
measured it, complete and green, for whoever wants to resurrect it against a
different benchmark or a different target. What it needs to become worth landing
is a case that looks like a real program.
