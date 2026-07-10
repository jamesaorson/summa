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

add_executable(summa.tests.identity identity.test.c)
target_link_libraries(summa.tests.identity PRIVATE summa::summa)
add_test(NAME summa.identity COMMAND summa.tests.identity)
summa_add_test(summa.tests.identity)
