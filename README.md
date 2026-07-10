# template-cpp

A minimal C++ library template.

## Structure

```
include/template_cpp/   # Public headers
src/template_cpp/       # Library sources
tests/                  # Tests (GTest via FetchContent)
examples/               # Usage examples
```

## Usage

```cpp
#include <template_cpp/identity.hpp>

template_cpp::identity id;
auto x = id(42); // 42
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
