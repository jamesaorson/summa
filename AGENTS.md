# AGENTS.md — summa

## What This Repo Is

**summa** is a mega-repo of single-file C libraries, plus a growing set of
applications built on top of them (a Scheme interpreter, a game engine, etc.).

Two distinct kinds of output live here:

| Kind                         | Location                             | Output                             |
| ---------------------------- | ------------------------------------ | ---------------------------------- |
| Single-file header libraries | `include/summa/<name>.h`             | header only — no compiled artifact |
| Applications                 | `<app>/` (e.g. `scheme/`, `engine/`) | binary or static/shared library    |

---

## Hard Rules

### Zero External Dependencies

**The library headers must have no third-party dependencies — ever.**

- Only the C23 standard library is permitted inside `include/summa/`.
- Applications may use summa headers as building blocks, but still no third-party
  runtime deps. Write the utility code you need as a summa header first, then use it.
- Do not add a `bazel_dep` for anything but `rules_cc` (build rules, not code). The answer to
  "can I pull in lib X?" is always "write it as a summa header instead."
- The test framework is `summa/test/test.h` — in-repo, zero external deps.

### C23, No Extensions

- `-std=c2x` (C23; the `c2x` spelling so GCC 13 accepts it), set once in `.bazelrc`.
- No compiler-specific extensions (`__attribute__`, `#pragma once`, etc.) unless
  guarded by a feature macro and with a portable fallback.
- All code must compile clean under `-Wall -Wextra -Wpedantic -Werror` (also
  `.bazelrc`, so every target gets them).

---

## Repository Layout

```plaintext
include/summa/          # Public headers — one .h per library
tests/                  # Test files — one .test.c per library
examples/               # Usage examples — one subdir per library
benchmarks/             # Benchmarks — built by the normal build, never run by `bazel test`
docs/                   # Reference docs (e.g. r7rs-small.pdf)
git/hooks/              # Dev hooks (install via make setup/hooks)
<app>/                  # Application subdirectories (scheme/, engine/, …)
```

---

## Dev Workflow

```sh
make setup       # Install tools (bazelisk, clang-format) + git hooks (run once)
make build       # bazel build --config=asan //...
make test        # bazel test --config=asan //...
make format      # clang-format -i all .h/.c files
make check       # clang-format --dry-run --Werror (runs in pre-commit hook)
make benchmark   # bazel run //benchmarks:scheme (add CONFIG=release for real numbers)
```

`CONFIG` is a `.bazelrc` config: `asan` (the default — debug build with ASan +
UBSan) or `release` (`-c opt`). Plain `bazel build //...` / `bazel test //...`
work too, with neither. Run one test case on its own with
`bazel test //tests:<name> --test_arg=<case>`.

CI matrices over `asan` and `release`. Always keep both green.

---

## Single-File Header Layout (STB-style)

Every header follows the same two-section structure:

```c
#ifndef SUMMA_NAME_H
#define SUMMA_NAME_H

/*
 * summa/<name>.h — one-line description
 * No external dependencies. C23.
 */

/* ── Types ─── */
/* struct / typedef / enum declarations */

/* ── API ───── */
/* extern declarations and macros only — no definitions */
void summa_name_do_thing(int x);

#endif /* SUMMA_NAME_H */

/* ── Implementation ── */
#ifdef SUMMA_NAME_IMPLEMENTATION

void summa_name_do_thing(int x) { /* ... */ }

#endif /* SUMMA_NAME_IMPLEMENTATION */
```

Rules:

- Everything above `#endif /* SUMMA_NAME_H */` is the **declaration section** — types,
  `extern` function declarations, and macros. Compiled in every translation unit.
- Everything inside `#ifdef SUMMA_NAME_IMPLEMENTATION` is the **definition section** —
  function bodies and global variable definitions. Exactly one translation unit must
  `#define SUMMA_NAME_IMPLEMENTATION` before the include.
- The `IMPLEMENTATION` guard sits **outside** the header guard so it can be triggered
  independently of include deduplication.

---

## Adding a Single-File Header Library

Follow this checklist exactly — every library needs all three steps.

### 1. The header — `include/summa/<name>.h`

Write the declaration section (types + function signatures) first, then the
implementation section. See the STB-style layout above.

Naming conventions:

- Include guard: `SUMMA_<NAME>_H`
- Implementation guard: `SUMMA_<NAME>_IMPLEMENTATION`
- Functions: `summa_<name>_<verb>(…)`
- Types: `summa_<name>_t`
- Constants / macros: `SUMMA_<NAME>_<CONSTANT>`

### 2. The test — `tests/<name>.test.c`

```c
#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

#include <summa/name.h>
#include <summa/name.h> /* verify include guard */

void test_name_something(void)
{
    SUMMA_TEST_ASSERT(/* ... */);
}

int main(int argc, char** argv)
{
    summa_test_begin("name", argc, argv);
    SUMMA_TEST_RUN(test_name_something);
    return summa_test_end();
}
```

Nothing to register: `tests/BUILD.bazel` globs `tests/**/*.test.c` and makes
each file a `cc_test` named after it (`tests/array.test.c` → `//tests:array`).
Just drop the file in and build.

#### Scope every allocation

**Anything a test allocates gets released by a scope, not by a trailing
`free`.** Use `SUMMA_TEST_SCOPED_VALUE(T, var, init, destroy)` — it binds `var`
for one block and hands it to `destroy` on the way out:

```c
SUMMA_TEST_SCOPED_VALUE(SummaString, str, summa_string_make("hi"), summa_string_free) {
    SUMMA_TEST_ASSERT_EQ_STR("hi", str->value);
}
```

Give each file a one-line alias so the type and destructor aren't repeated at
every call site, then nest scopes for multiple values — inner ones are destroyed
first:

```c
#define SCOPED_STRING(var, init) SUMMA_TEST_SCOPED_VALUE(SummaString, var, init, summa_string_free)

SCOPED_STRING(dest, summa_string_make("x"))
SCOPED_STRING(src, summa_string_make("hello world")) {
    summa_string_copy(dest, src);
}
```

When a type owns things its own `_free` doesn't reach, write a small `static`
destructor next to the alias and scope against that, rather than open-coding the
teardown at each site:

```c
static void free_symbol_list(SummaSchemeSymbolList symbols) {
    for (size_t i = 0; i < symbols->length; i++) {
        summa_string_free(symbols->value[i].value);
    }
    summa_symbol_list_free(symbols);
}
```

Why it matters: cleanup can't drift away from the allocation, a new early exit
can't skip it, and a failing assertion still releases (assertions record and
continue rather than unwinding). Two limits worth knowing — `return` and `break`
out of the block *do* skip the cleanup, so let blocks end normally; and a value
filled in through an output parameter wasn't made by the scope, so those stay
manual. If a manual `free` is genuinely unavoidable, say why in a comment.

`SUMMA_TEST_SCOPED_FILE(f)` is the same idea for a captured `FILE*`.

#### Checking for leaks locally

CI runs ASan/UBSan (with LeakSanitizer) on Linux, but LeakSanitizer doesn't run
on Darwin — so a leak introduced locally will not fail `make test`. Use `leaks`
on a build without ASan instead:

```sh
bazel build -c dbg //tests:<name>
leaks --atExit -- ./bazel-bin/tests/<name>
```

### 3. Add an example — `examples/<name>/main.c`

Write a minimal `examples/<name>/main.c` that shows idiomatic usage.
`examples/BUILD.bazel` globs it into `//examples:<name>`.

---

## Adding an Application

Applications (Scheme interpreter, game engine, etc.) live in their own top-level
subdirectory with its own `BUILD.bazel`.

Guidelines:

- The application's own `BUILD.bazel` owns its targets.
- It may depend on `//:summa` freely.
- It must still have zero external runtime dependencies — use or extend summa headers
  for any utility code the application needs.
- It produces a binary (executable) or a library target, not a header.

---

## Adding a Benchmark

Write `benchmarks/<name>/main.c`; `benchmarks/BUILD.bazel` globs it into
`//benchmarks:<name>`. It builds like an example and is deliberately a
`cc_binary`, **not** a `cc_test`, so `make test` never pays for it while a
benchmark that stopped compiling still fails `bazel build //...`.
`make benchmark` runs it.

A benchmark prints numbers; it asserts nothing. Keep the output one line per
case in a fixed order so two runs diff to what moved, and give it a `--help`
like any other CLI here.

---

## Things Not to Do

- Do not add third-party headers to `include/summa/`.
- Do not `#include` non-standard headers inside a summa header.
- Do not hand-add `cc_test` entries for a library test —
  `tests/BUILD.bazel` discovers `tests/**/*.test.c` automatically.
- Do not commit without running `make check` (the pre-commit hook enforces this).
- Do not open a PR without a linked issue (`Closes #N` in the description).
- Do not merge your own PRs.
