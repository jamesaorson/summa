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

add_executable(summa.tests.scheme.array scheme.array.test.c)
target_link_libraries(summa.tests.scheme.array PRIVATE summa::summa)
add_test(NAME summa.scheme.array COMMAND summa.tests.scheme.array)
summa_add_test(summa.tests.scheme.array)
