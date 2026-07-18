# summa_discover_tests(<target> [TEST_PREFIX <prefix>])
#
# Registers one CTest test per summa_test case in <target>, instead of one
# test per executable. <target> must be a summa.test.h binary — discovery
# runs it with `--list` (see test.h) right after each build to read its case
# names, so `ctest -R <case-name>` runs and reruns a single case on its own.
#
# TEST_PREFIX defaults to "<target>." and is prepended to each case name to
# form the CTest test name.
include_guard(GLOBAL)

function(summa_discover_tests TARGET)
    cmake_parse_arguments(ARG "" "TEST_PREFIX" "" ${ARGN})
    if(NOT ARG_TEST_PREFIX)
        set(ARG_TEST_PREFIX "${TARGET}.")
    endif()

    set(ctest_file "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_tests.cmake")
    set(include_file "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_include.cmake")

    add_custom_command(
        TARGET ${TARGET} POST_BUILD
        BYPRODUCTS "${ctest_file}"
        COMMAND "${CMAKE_COMMAND}"
                -D "TEST_EXECUTABLE=$<TARGET_FILE:${TARGET}>"
                -D "TEST_PREFIX=${ARG_TEST_PREFIX}"
                -D "CTEST_FILE=${ctest_file}"
                -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/SummaTestAddTests.cmake"
        VERBATIM
    )

    # TEST_INCLUDE_FILES are read every time ctest runs, so this small wrapper
    # (rather than ctest_file directly) keeps configure from failing on a
    # clean tree, before the POST_BUILD step has produced ctest_file once.
    file(WRITE "${include_file}"
        "if(EXISTS \"${ctest_file}\")\n"
        "  include(\"${ctest_file}\")\n"
        "else()\n"
        "  add_test(\"${ARG_TEST_PREFIX}NOT_BUILT\" \"${TARGET}-NOTFOUND\")\n"
        "endif()\n"
    )

    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES "${include_file}")
endfunction()
