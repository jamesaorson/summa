# summa_add_test(<target>)
# Configures a test executable with strict compiler flags and, in Debug builds,
# AddressSanitizer + UndefinedBehaviorSanitizer.
#
# Homebrew's GCC on macOS ships the sanitizer headers but not the
# libasan/libubsan runtimes (Darwin sanitizer support is Clang-only there),
# so linking -fsanitize=address,undefined fails. Skip sanitizers for that one
# combination; CI (Linux + GCC) still gets full coverage.
set(SUMMA_SANITIZERS_SUPPORTED ON)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(SUMMA_SANITIZERS_SUPPORTED OFF)
endif()

function(summa_add_test target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
    )
    if(SUMMA_SANITIZERS_SUPPORTED)
        target_compile_options(${target} PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        )
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        )
    endif()
endfunction()

include("${CMAKE_SOURCE_DIR}/cmake/SummaTestDiscover.cmake")

# Concurrency tests spawn threads through <threads.h> where the platform has it
# (glibc) and <pthread.h> where it doesn't (macOS). Threads::Threads supplies
# whichever link flag that needs; targets with no threaded case ignore it.
find_package(Threads REQUIRED)

# Every *.c file anywhere under tests/ (including nested folders, e.g.
# tests/scheme/scheme.equals.c) becomes a tests.<name> executable, with one
# CTest test per case (<name>.<case>, via summa_discover_tests). A top-level
# file keeps its ".test.c" suffix (tests/array.test.c -> "array"); a file
# already nested in a folder drops it, since the folder makes it obvious
# (tests/scheme/scheme.equals.c -> "scheme.equals"). Drop a new file in
# either place and it's picked up automatically — nothing to edit here.
file(GLOB_RECURSE test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.c")
list(SORT test_sources)

foreach(test_source ${test_sources})
    get_filename_component(test_source_name "${test_source}" NAME)
    string(REGEX REPLACE "\\.test\\.c$|\\.c$" "" test_name "${test_source_name}")

    set(target "tests.${test_name}")

    add_executable(${target} "${test_source}")
    target_link_libraries(${target} PRIVATE summa::summa Threads::Threads)
    summa_discover_tests(${target} TEST_PREFIX "${test_name}." SOURCE_FILE "${test_source}")
    summa_add_test(${target})
endforeach()
