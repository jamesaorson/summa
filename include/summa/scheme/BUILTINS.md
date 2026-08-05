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
freed memory. `summa_scheme_value_copy` is deep, which is what makes that
correct.

`cons` takes a list as its second argument or errors, and the message names why:
`(cons 1 2)` is an improper pair, which a dynamic array has no way to be.
Answering `(1 2)` instead would be a different value wearing the same name.
Prepending therefore builds a list of length n+1 rather than a cell pointing at
the old one, so a list head nests — `(cons '(1) '(2))` is `((1) 2)`, not
`(1 2)`.

`car` and `cdr` share one checker: one operand, a list, and not the empty one.
`(car '())` is an error, but `(cdr '(1))` is `()` — an exhausted tail is the
value a recursion stops on, not a mistake. `cdr` copies, so no two values ever
observe the same tail.

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

What is *not* optimized is per-call cost. Every call still mallocs a frame and
copies each argument into it. Tail calls make a long-running program terminate;
they do not make it fast. `benchmarks/scheme` prices each of those separately —
see [Current state](#current-state).

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
- There is no shared structure. `cdr` copies, so no two values ever observe the
  same tail, and a mutation through one of them could not be seen through
  another. `set-car!` would type-check and do nothing observable.

Shipping them would be worse than omitting them. Implementing them for real
means cons cells, which means reference counting — and cons cells can point at
each other, so it means the cycle collector too. Environments already have both;
this is the same machinery pointed at a second type.

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
`summa_scheme_apply`. Deep copy and free, which is what lets values outlive the
frame that produced them. `summa_scheme_read`, so a program can be written as
source rather than built as values.

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

**Not done.** `/`, the equality procedures, the remaining type predicates,
`display` and `newline`.

The remaining structural gap is not depth any more, it is *speed*, and euler
problem 10 is where it shows: roughly 10^8 Scheme-level calls, each one
mallocing a frame and deep copying its arguments into it, and finding both the
procedure and each parameter by walking a linked chain of environments. That
case is left un-run for exactly that reason. Arguments moved rather than copied
and a binding lookup that is not a linear scan are what is left of the three
obvious buys — and both want a measurement first rather than a guess.

That measurement exists. `benchmarks/scheme` is a set of programs written to
isolate one cost each, printing wall time, allocations and bytes per call; it is
built by the ordinary build and run by hand (`make benchmark`), never by
`ctest`, because a benchmark asserts nothing and a CI run should not pay for
one. Every case comes in a pair or a series, since the reading that travels
between machines is the *ratio* between two of them. What it says about the
evaluator as it stands, Release, on an M-series Mac — and, in the middle column,
what it said before interning landed:

| Reading                                        | Before interning               | Now                              |
| ---------------------------------------------- | ------------------------------ | -------------------------------- |
| a user procedure call                          | ~1.2 µs, 15 allocations, 2 KB  | ~0.63 µs, 11 allocations, 1.9 KB |
| a call binding nothing, over the loop it is in | +356 ns, +5 allocations, 832 B | +186 ns, +5 allocations, 832 B   |
| each argument bound                            | +123 ns, +2 allocations        | +38 ns, **+0 allocations**       |
| 32 lexical frames rather than 1                | +52% per call                  | +40% per call                    |
| the last of 256 globals rather than of 8       | +117% per call                 | +41% per call                    |
| a 128-element list argument rather than 1      | 3.3× the time, 9.5× the bytes  | 5.2× the time, 9.7× the bytes    |

Read the last row the right way round: the absolute cost of the 128-element case
fell by 21%, and the *ratio* got worse because what interning removed was fixed
per-call cost. What is left in that case is the O(length) copy, which is exactly
what the next two buys are about.

Two of those readings say something the original list of three did not.
**Arguments are copied twice per call, not once** —
`summa_scheme_procedure_frame` makes one, and `summa_scheme_evaluate`'s symbol
case makes the other, since a variable reference hands back a copy of the bound
value. Moving arguments into the frame removes one of the two; the per-call cost
of a list argument stays linear in its length until the other goes as well. And
**most of what a frame costs is capacity nobody asked for**: of the 832 bytes a
call binding nothing allocates, 704 are element storage at
`SUMMA_ARRAY_DEFAULT_CAPACITY` — eight `SummaSchemeValue` slots of argument list
and eight `SummaSchemeBinding` slots of frame bindings, for a call with no
arguments and no bindings. Interning did not touch either number, and both are
now a larger share of what a call costs than they were. Sizing those two arrays
to the arity the evaluator already knows is the cheapest buy left.

Walking a list is bounded by memory rather than by `SUMMA_SCHEME_MAX_DEPTH` when
the walk is written tail recursively; written the other way —
`(cons (f (car l)) (map f (cdr l)))` — it still costs a C frame per element, and
always will, because it has work left to do after the call.
