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

add_executable(tests.array array.test.c)
target_link_libraries(tests.array PRIVATE summa::summa)
summa_discover_tests(tests.array TEST_PREFIX "array.")
summa_add_test(tests.array)

add_executable(tests.string string.test.c)
target_link_libraries(tests.string PRIVATE summa::summa)
summa_discover_tests(tests.string TEST_PREFIX "string.")
summa_add_test(tests.string)

add_executable(tests.scheme scheme.test.c)
target_link_libraries(tests.scheme PRIVATE summa::summa)
summa_discover_tests(tests.scheme TEST_PREFIX "scheme.")
summa_add_test(tests.scheme)

add_executable(tests.hash_set hash_set.test.c)
target_link_libraries(tests.hash_set PRIVATE summa::summa)
summa_discover_tests(tests.hash_set TEST_PREFIX "hash_set.")
summa_add_test(tests.hash_set)

add_executable(tests.hash_map hash_map.test.c)
target_link_libraries(tests.hash_map PRIVATE summa::summa)
summa_discover_tests(tests.hash_map TEST_PREFIX "hash_map.")
summa_add_test(tests.hash_map)
