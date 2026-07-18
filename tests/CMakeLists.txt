# summa_add_test(<target>)
# Configures a test executable with strict compiler flags and, in Debug builds,
# AddressSanitizer + UndefinedBehaviorSanitizer.
function(summa_add_test target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
    target_link_options(${target} PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
endfunction()

include("${CMAKE_SOURCE_DIR}/cmake/SummaTestDiscover.cmake")

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
    target_link_libraries(${target} PRIVATE summa::summa)
    summa_discover_tests(${target} TEST_PREFIX "${test_name}." SOURCE_FILE "${test_source}")
    summa_add_test(${target})
endforeach()
