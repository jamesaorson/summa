# summa

A mega-repo of single-file C libraries.

## Structure

```
include/summa/   # Public headers (one .h per library)
tests/           # Tests (Unity via FetchContent)
examples/        # Usage examples
```

## Usage

```c
#include <summa/identity.h>

int x = summa_identity(42); /* 42 */
```

## Building

```sh
make setup      # Install dependencies
make configure  # Configure CMake
make build      # Build the library
make test       # Run tests
```

## License

Apache-2.0 WITH LLVM-exception
