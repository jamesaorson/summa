# summa_add_test(<target>)
# Configures a test executable with strict compiler flags and, in Debug builds,
# AddressSanitizer + UndefinedBehaviorSanitizer.
function(summa_add_test target)
    target_compile_options(${target} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror
        $<$<C_COMPILER_ID:Clang,AppleClang>:-Wc23-extensions>
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
    target_link_options(${target} PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
endfunction()

include("${CMAKE_SOURCE_DIR}/cmake/SummaTestDiscover.cmake")

# Every tests/<name>.test.c becomes a tests.<name> executable, with one CTest
# test per case (<name>.<case>, via summa_discover_tests). Drop a new
# *.test.c file in this directory and it's picked up automatically — nothing
# to edit here.
file(GLOB test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.test.c")
list(SORT test_sources)

foreach(test_source ${test_sources})
    get_filename_component(test_source_name "${test_source}" NAME)
    string(REGEX REPLACE "\\.test\\.c$" "" test_name "${test_source_name}")

    set(target "tests.${test_name}")

    add_executable(${target} "${test_source_name}")
    target_link_libraries(${target} PRIVATE summa::summa)
    summa_discover_tests(${target} TEST_PREFIX "${test_name}." SOURCE_FILE "${test_source}")
    summa_add_test(${target})
endforeach()
