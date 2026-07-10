include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(jamesaorson.template_cpp.tests.identity identity.test.cpp)
set_property(TARGET jamesaorson.template_cpp.tests.identity PROPERTY CXX_STANDARD 20)
target_link_libraries(
    jamesaorson.template_cpp.tests.identity
    PRIVATE jamesaorson::template_cpp GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(jamesaorson.template_cpp.tests.identity)
