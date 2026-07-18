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

add_executable(summa.tests.scheme.array scheme.array.test.c)
target_link_libraries(summa.tests.scheme.array PRIVATE summa::summa)
summa_discover_tests(summa.tests.scheme.array TEST_PREFIX "summa.scheme.array.")
summa_add_test(summa.tests.scheme.array)

add_executable(summa.tests.scheme.string scheme.string.test.c)
target_link_libraries(summa.tests.scheme.string PRIVATE summa::summa)
summa_discover_tests(summa.tests.scheme.string TEST_PREFIX "summa.scheme.string.")
summa_add_test(summa.tests.scheme.string)

add_executable(summa.tests.scheme scheme.test.c)
target_link_libraries(summa.tests.scheme PRIVATE summa::summa)
summa_discover_tests(summa.tests.scheme TEST_PREFIX "summa.scheme.")
summa_add_test(summa.tests.scheme)

add_executable(summa.tests.hash_set hash_set.test.c)
target_link_libraries(summa.tests.hash_set PRIVATE summa::summa)
summa_discover_tests(summa.tests.hash_set TEST_PREFIX "summa.hash_set.")
summa_add_test(summa.tests.hash_set)

add_executable(summa.tests.hash_map hash_map.test.c)
target_link_libraries(summa.tests.hash_map PRIVATE summa::summa)
summa_discover_tests(summa.tests.hash_map TEST_PREFIX "summa.hash_map.")
summa_add_test(summa.tests.hash_map)
