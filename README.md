# summa

A mega-repo of single-file C libraries, plus applications built on top of them.

Zero external dependencies. C23.

## Structure

```plaintext
include/summa/   # Public headers — one .h per library
tests/           # Tests — one .test.c per library (uses test.h)
examples/        # Usage examples
docs/            # Reference documents
<app>/           # Application subdirectories (scheme/, engine/, …)
```

## Usage

Headers are included directly — no linking required.

```c
#include <summa/identity.h>

int x = summa_identity(42); /* 42 */
```

The test framework needs one translation unit to define the implementation:

```c
#define SUMMA_TEST_IMPLEMENTATION
#include <summa/test/test.h>

void test_something(void)
{
    SUMMA_TEST_ASSERT_EQ_INT(42, summa_identity(42));
}

int main(void)
{
    summa_test_begin("my suite");
    SUMMA_TEST_RUN(test_something);
    return summa_test_end();
}
```

## Building

```sh
make setup      # Install tools (bazelisk, clang-format) and git hooks
make build      # bazel build --config=asan //...
make test       # bazel test --config=asan //...
make format     # auto-format all .h/.c files
make check      # dry-run format check (also runs in pre-commit hook)
```

`asan` is a debug build with ASan + UBSan. Pass `CONFIG=release` to any target
for an optimized build:

```sh
make test CONFIG=release
```

## License

MIT No Attribution
