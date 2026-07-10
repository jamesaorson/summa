include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.1
)
FetchContent_MakeAvailable(unity)

add_executable(summa.tests.identity identity.test.c)
target_link_libraries(summa.tests.identity PRIVATE unity summa::summa)
add_test(NAME summa.identity COMMAND summa.tests.identity)
