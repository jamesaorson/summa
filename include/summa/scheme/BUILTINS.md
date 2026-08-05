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
What is left is the *other* copy — a variable reference still duplicates the
value it names — which is issue #40 and a larger question than any of the three,
because it is about what `SummaSchemeValue` ownership means.

That measurement exists. `benchmarks/scheme` is a set of programs written to
isolate one cost each, printing wall time, allocations and bytes per call; it is
built by the ordinary build and run by hand (`make benchmark`), never by
`ctest`, because a benchmark asserts nothing and a CI run should not pay for
one. Every case comes in a pair or a series, since the reading that travels
between machines is the *ratio* between two of them. What it says about the
evaluator as it stands, Release, on an M-series Mac, with the columns to its left
being what it said before interning, before frames were right-sized, and before
arguments were moved:

| Reading                                        | Before interning               | Interned                         | Right-sized                       | Now                                  |
| ---------------------------------------------- | ------------------------------ | -------------------------------- | --------------------------------- | ------------------------------------ |
| a user procedure call                          | ~1.2 µs, 15 allocations, 2 KB  | ~0.63 µs, 11 allocations, 1.9 KB | ~0.62 µs, 11 allocations, 0.64 KB | **~0.45 µs, 6 allocations, 0.50 KB** |
| a call binding nothing, over the loop it is in | +356 ns, +5 allocations, 832 B | +186 ns, +5 allocations, 832 B   | +137 ns, +3 allocations, 128 B    | **+71 ns, +1 allocation, 88 B**      |
| each argument bound                            | +123 ns, +2 allocations        | +38 ns, **+0 allocations**       | +56 ns, +0 allocations, +88 B     | +57 ns, +0 allocations, +88 B        |
| 32 lexical frames rather than 1                | +52% per call                  | +40% per call                    | +42% per call                     | +54% per call                        |
| the last of 256 globals rather than of 8       | +117% per call                 | +41% per call                    | +42% per call                     | +53% per call                        |
| a 128-element list argument rather than 1      | 3.3× the time, 9.5× the bytes  | 5.2× the time, 9.7× the bytes    | 5.1× the time, 16.5× the bytes    | **4.1× the time, 14.1× the bytes**   |

The two rightmost columns come from one alternated measurement of the same
binaries, so they are differences rather than two separate readings of the
machine; the two on the left are as they were published.

Three rows want reading carefully.

**A frame that binds nothing is now one allocation and 88 bytes**, and 88 bytes
is the whole of it: a `RefCount` header plus `SummaSchemeEnvironment_t`, with
the binding array's header inside that block rather than beside it. There is
nothing else left in a call that binds nothing to remove.

**The 128-element row finally turned round.** Its ratio had risen for two
releases running — 3.3× to 5.2× to 5.1× — because everything being removed was
*fixed* per-call cost, which left the O(length) copy a larger share of what was
left. Moving the argument takes one of the two copies away, and the ratio falls
for the first time. It does not flatten, and it was never going to: a variable
reference still hands back a copy of the value it names, so a list argument is
still walked once per call. That is issue #40, and this row is what it will be
read against.

**The two lookup ratios got worse, and nothing about them changed.** The chain
walk and the global scan cost exactly what they always did; what fell is
everything they are divided by, so a fixed cost is now a larger fraction of a
smaller call. That is what a ratio does when the denominator moves, and it is
the reason absolute columns are kept beside it. Hashing the frame was measured
against these two rows and lost anyway — see
[The binding lookup is still a scan](#the-binding-lookup-is-still-a-scan-and-the-measurement-is-why).

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
