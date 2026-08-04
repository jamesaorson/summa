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

Arithmetic, comparison and the string accessors exist; the list, equality and
predicate families do not. Adding one is a function of type
`SummaSchemeBuiltinFn` plus a row in `SUMMA_SCHEME_BUILTINS`; the global
environment binds a procedure per row at startup and dispatch finds it by the
same name, so there is nothing else to register. `summa_scheme_require_arity`,
`summa_scheme_require_min_arity`, `summa_scheme_require_type`,
`summa_scheme_require_arity_of_type`, `summa_scheme_require_numbers`,
`summa_scheme_has_floating` and `summa_scheme_number_to_double` are already
there as the shared argument checking.

### Pairs and lists — still to write

Unbounded data structure — what makes the language usable rather than merely
complete.

`cons` · `car` · `cdr` · `list` · `null?` · `pair?`

All four constructors must copy: `summa_scheme_apply` releases the argument list
the instant dispatch returns, so anything handed back still pointing into it is
freed memory. `summa_scheme_value_copy` is deep, which is what makes that
correct.

`set-car!` and `set-cdr!` are **not** on this list. See the gotcha below.

### Equality — still to write

`eq?` · `eqv?` · `equal?`

`summa_scheme_value_equals` already backs `equal?`.

### Type predicates — still to write

One per arm of `SummaSchemeValueType`.

`boolean?` · `char?` · `number?` · `integer?` · `real?` · `string?` · `symbol?` ·
`procedure?` · `vector?`

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

### Closures outlive their frames

A procedure captures its defining environment in
`SummaSchemeProcedure::closure`, borrowed rather than owned — the same rule
`SummaSchemeEnvironment_t::parent` already follows. That gives correct lexical
scoping (`test_scheme_procedures_are_lexically_scoped`) and costs nothing to
copy.

What it does not survive is a closure escaping the frame it closed over:

```scheme
(define (make-counter) (lambda () count))   ; the lambda's closure is the call
(make-counter)                              ; frame, freed on the way out
```

`summa_scheme_procedure_dispatch` frees the call frame when the body finishes,
so the returned procedure's `closure` handle dangles. Same for a `lambda`
returned out of a `let`. This is the case reference-counted environments would
fix, and it is the one part of the design that is knowingly unsound rather than
merely unfinished.

### Tail calls

R7RS mandates proper tail calls. Without them, "Turing-complete" is theoretical
— a loop written as tail recursion exhausts the C stack.

Evaluation is a plain recursive C call, so `SUMMA_SCHEME_MAX_DEPTH` (2000
evaluate frames, several per Scheme-level call) turns a runaway recursion into
a diagnosable error instead of a segfault. It is a stopgap, not a design:
`test_scheme_runaway_recursion_is_an_error_not_a_crash` pins the behavior and
should be deleted along with the guard.

Whether `summa_scheme_evaluate` becomes a trampoline or keeps an explicit stack
is worth deciding *before* much depends on the current shape; retrofitting means
touching every `case`.

### `set-car!` and `set-cdr!` have nothing to mutate

Lists are `SummaList` — dynamic arrays, not cons cells. Two consequences:

- `(cons 1 2)` has no representation. There are no improper pairs, so `cons`
  takes a list as its second argument or errors.
- There is no shared structure. `cdr` copies, so no two values ever observe the
  same tail, and a mutation through one of them could not be seen through
  another. `set-car!` would type-check and do nothing observable.

Shipping them would be worse than omitting them. Implementing them for real
means cons cells, which means reference counting, which is the same change the
closure gotcha above wants.

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

Sixteen builtins: the arithmetic, comparison, boolean and string sets above.
Recursion has a numeric base case now, which is what the euler suites were
waiting on — problems 1, 2, 5, 6 and 9 pass.

Covered by `tests/scheme/scheme.special_forms.test.c` and
`tests/scheme/scheme.builtins.test.c`.

**Not done.** `/`, the pair and list procedures, the equality procedures, the
type predicates, `display` and `newline`. Tail calls, which is what the five
remaining euler problems are blocked on — each of them iterates past
`SUMMA_SCHEME_MAX_DEPTH`.

The list builtins are the larger of the two gaps: without them there is no
unbounded data structure, so every program here threads its state through
accumulator arguments. That is the shape of the gap, not a limit on the
evaluator.
