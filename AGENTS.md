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
- Do not add `FetchContent_Declare` calls for anything. The answer to
  "can I pull in lib X?" is always "write it as a summa header instead."
- The test framework is `summa/test.h` — in-repo, zero external deps.

### C23, No Extensions

- `LANGUAGES C`, `CMAKE_C_STANDARD 23`, `CMAKE_C_EXTENSIONS OFF`.
- No compiler-specific extensions (`__attribute__`, `#pragma once`, etc.) unless
  guarded by a feature macro and with a portable fallback.
- All code must compile clean under `-Wall -Wextra -Wpedantic -Werror`.

---

## Repository Layout

```plaintext
include/summa/          # Public headers — one .h per library
tests/                  # Test files — one .test.c per library
examples/               # Usage examples — one subdir per library
docs/                   # Reference docs (e.g. r7rs-small.pdf)
git/hooks/              # Dev hooks (install via make setup/hooks)
<app>/                  # Application subdirectories (scheme/, engine/, …)
```

---

## Dev Workflow

```sh
make setup       # Install tools + git hooks (run once)
make configure   # cmake --preset debug (or release via CMAKE_BUILD_TYPE=Release)
make build       # cmake --build --preset debug
make test        # build + ctest --preset debug
make format      # clang-format -i all .h/.c files
make check       # clang-format --dry-run --Werror (runs in pre-commit hook)
```

CI matrices over `Debug` and `Release`. Debug runs ASan + UBSan. Always keep both
green.

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

Follow this checklist exactly — every library needs all four steps.

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
#include <summa/test.h>

#include <summa/name.h>
#include <summa/name.h> /* verify include guard */

void test_name_something(void)
{
    SUMMA_TEST_ASSERT(/* ... */);
}

int main(void)
{
    summa_test_begin("name");
    SUMMA_TEST_RUN(test_name_something);
    return summa_test_end();
}
```

### 3. Register the test — `tests/CMakeLists.txt`

```cmake
add_executable(summa.tests.<name> <name>.test.c)
target_link_libraries(summa.tests.<name> PRIVATE summa::summa)
add_test(NAME summa.<name> COMMAND summa.tests.<name>)
summa_add_test(summa.tests.<name>)   # strict flags + ASan/UBSan
```

`summa_add_test()` is already defined at the top of `tests/CMakeLists.txt`.
Always call it — never skip it.

### 4. Add an example — `examples/<name>/main.c` + `examples/CMakeLists.txt`

Add `<name>` to the `ALL_EXAMPLES` list in `examples/CMakeLists.txt`, then
write a minimal `examples/<name>/main.c` that shows idiomatic usage.

---

## Adding an Application

Applications (Scheme interpreter, game engine, etc.) live in their own top-level
subdirectory and are wired into the root `CMakeLists.txt` via `add_subdirectory`.

Guidelines:

- The application's own `CMakeLists.txt` owns its targets, flags, and install rules.
- It may link against `summa::summa` freely.
- It must still have zero external runtime dependencies — use or extend summa headers
  for any utility code the application needs.
- It produces a binary (executable) or a library target, not a header.
- Add a `SUMMA_BUILD_<APP>` option in the root `CMakeLists.txt` following the
  existing pattern for `SUMMA_BUILD_TESTS` and `SUMMA_BUILD_EXAMPLES`.

---

## Things Not to Do

- Do not add third-party headers to `include/summa/`.
- Do not `#include` non-standard headers inside a summa header.
- Do not skip `summa_add_test()` on a test target.
- Do not commit without running `make check` (the pre-commit hook enforces this).
- Do not open a PR without a linked issue (`Closes #N` in the description).
- Do not merge your own PRs.
