cmake_minimum_required(VERSION 3.28...4.2)

project(
    jamesaorson.template_cpp # CMake Project Name, which is also the name of the top-level
    # targets (e.g., library, executable, etc.).
    DESCRIPTION "C++ template"
    LANGUAGES CXX
    VERSION 0.1.0
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(
    JAMESAORSON_TEMPLATE_CPP_BUILD_TESTS
    "Enable building tests and test infrastructure. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

option(
    JAMESAORSON_TEMPLATE_CPP_BUILD_EXAMPLES
    "Enable building examples. Default: ${PROJECT_IS_TOP_LEVEL}. Values: { ON, OFF }."
    ${PROJECT_IS_TOP_LEVEL}
)

add_library(jamesaorson.template_cpp)
add_library(jamesaorson::template_cpp ALIAS jamesaorson.template_cpp)

set_property(TARGET jamesaorson.template_cpp PROPERTY CXX_STANDARD 20)

file(GLOB_RECURSE CPP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/template_cpp/*.cpp")

target_sources(
    jamesaorson.template_cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE
        ${CPP_SOURCES}
)
target_include_directories(
    jamesaorson.template_cpp
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

set_target_properties(jamesaorson.template_cpp PROPERTIES VERIFY_INTERFACE_HEADER_SETS ON)

if(JAMESAORSON_TEMPLATE_CPP_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()

if(JAMESAORSON_TEMPLATE_CPP_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
