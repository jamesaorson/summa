# test.h

Single-file unit testing framework. STB-style: types and declarations are always
compiled; function definitions live behind `#define SUMMA_TEST_IMPLEMENTATION`.

## Usage

In exactly one translation unit, define the implementation before the include:

```c
#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>
```

All other translation units that need only the API:

```c
#include <summa/test/test.h>
```

## Example

```c
#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

void test_addition(void)
{
    SUMMA_TEST_ASSERT(1 + 1 == 2);
    SUMMA_TEST_ASSERT_EQ_INT(42, 40 + 2);
}

int main(int argc, char** argv)
{
    summa_test_begin("arithmetic", argc, argv);
    SUMMA_TEST_RUN(test_addition);
    return summa_test_end();
}
```

## API

### Lifecycle

#### `void summa_test_begin(const char *suite_name, int argc, char **argv)`

Initialises the test context and prints the suite header. Call once before any
`SUMMA_TEST_RUN`, passing `main`'s own `argc`/`argv` straight through.

`argv` selects what runs, mirroring how `gtest_filter`/`gtest_list_tests` work:

| Invocation             | Effect                                                                |
| ---------------------- | --------------------------------------------------------------------- |
| `<binary>`             | Runs every test in the suite (previous behavior, unchanged).          |
| `<binary> <test_name>` | Runs only the test whose function name matches `<test_name>` exactly. |
| `<binary> --list`      | Prints every test name in the suite, one per line, and runs none.     |

This makes each test case individually runnable from the command line, and is
what `summa_discover_tests()` (see `cmake/SummaTestDiscover.cmake`) uses to
register one CTest test per case instead of one per executable.

#### `int summa_test_end(void)`

Prints a pass/fail summary. Returns `0` if every test passed, `1` otherwise.
Pass the return value directly to `return` from `main`. In `--list` mode, prints
nothing and always returns `0`.

### Running tests

#### `SUMMA_TEST_RUN(fn)`

In list mode, prints `fn`'s name and does nothing else. If a filter is active
and doesn't match `fn`, skips it silently. Otherwise calls `fn()`, then records
it as passed or failed based on whether any assertion inside it fired, and
prints a one-line result.

All assertions in a test function are always evaluated — there is no early exit
on the first failure.

### Assertions

| Macro                                        | Fails when                    |
| -------------------------------------------- | ----------------------------- |
| `SUMMA_TEST_ASSERT(expr)`                    | `expr` is false               |
| `SUMMA_TEST_ASSERT_MSG(expr, msg)`           | `expr` is false; prints `msg` |
| `SUMMA_TEST_ASSERT_EQ(a, b)`                 | `a != b`                      |
| `SUMMA_TEST_ASSERT_NEQ(a, b)`                | `a == b`                      |
| `SUMMA_TEST_ASSERT_EQ_INT(expected, actual)` | integers differ; prints both  |
| `SUMMA_TEST_ASSERT_EQ_STR(expected, actual)` | strings differ; prints both   |
| `SUMMA_TEST_ASSERT_NULL(ptr)`                | `ptr != nullptr`              |
| `SUMMA_TEST_ASSERT_NOT_NULL(ptr)`            | `ptr == nullptr`              |

### Low-level hook

#### `void summa_test_assert_fail(const summa_test_failure_t *f)`

Called by assertion macros on failure. Can be overridden before
`SUMMA_TEST_IMPLEMENTATION` is defined if custom failure handling is needed.

Prints one GCC-style line per failure — `<file>:<line>: error: <expr>` (with
`(<message>)` appended when the macro provides one). This is deliberate: it's
the format editors and CTest-aware tooling already parse for file:line
linking, e.g. VS Code's "CMake Test Explorer" extension matches it with its
default `errorPattern` and places the diagnostic on the exact failing
assertion (`__LINE__` at the `SUMMA_TEST_ASSERT*` call site), not just the
enclosing test function.

## Types

### `summa_test_ctx_t`

Internal context. Initialised by `summa_test_begin`. Treat as opaque.

### `summa_test_failure_t`

Failure descriptor passed to `summa_test_assert_fail`.

| Field     | Type           | Description                          |
| --------- | -------------- | ------------------------------------ |
| `file`    | `const char *` | Source file of the failing assertion |
| `line`    | `int`          | Line number                          |
| `expr`    | `const char *` | Stringified expression               |
| `message` | `const char *` | Optional message; may be `NULL`      |

## Global state

`summa_test_ctx_t summa_test_ctx` — defined in the implementation TU. The macros
read and write it directly; do not modify it manually.
