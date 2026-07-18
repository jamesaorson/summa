cmake_minimum_required(VERSION 3.25...4.2)

# Pick a GCC 13+ off PATH before the compiler is detected, so this works the
# same whether invoked via `make`, an IDE, or `cmake` directly. Homebrew on
# macOS never installs a bare `gcc`, so search versioned names too.
if(NOT DEFINED CACHE{CMAKE_C_COMPILER})
    find_program(SUMMA_GCC NAMES gcc-13 gcc-14 gcc-15 gcc-16 gcc)
    if(SUMMA_GCC)
        set(CMAKE_C_COMPILER ${SUMMA_GCC})
    endif()
endif()

project(
    summa
    DESCRIPTION "A mega-repo of single-file C libraries"
    LANGUAGES C
    VERSION 0.1.0
)

if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_VERSION VERSION_LESS "13")
    message(
        FATAL_ERROR
        "summa requires GCC 13 or newer (found ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}). "
        "Run `make setup` to install a compatible compiler."
    )
endif()

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(
    SUMMA_BUILD_TESTS
    "Enable building tests and test infrastructure. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

option(
    SUMMA_BUILD_EXAMPLES
    "Enable building examples. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

add_library(summa INTERFACE)
add_library(summa::summa ALIAS summa)

target_include_directories(
    summa
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# summa/array.h's SUMMA_ARRAY_GENERATE_TYPE_* macros give every generated
# collection type (SummaList, SummaHashMapKeys, ...) the same field layout as
# SummaArray_t and reinterpret-cast between them. That's legal by layout but
# not by C's strict aliasing rules, since the named struct types are still
# distinct — GCC's TBAA can cache a read across a write made through the
# other type's pointer, silently going stale under -O2/-O3 (observed as
# summa_hash_map_copy's length update not being visible to the caller).
# -fno-strict-aliasing is the standard fix for this exact pattern (the same
# reason the Linux kernel builds with it) and is supported identically by
# GCC and Clang, so it's not a portability hazard.
target_compile_options(summa INTERFACE -fno-strict-aliasing)

if(SUMMA_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

if(SUMMA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
