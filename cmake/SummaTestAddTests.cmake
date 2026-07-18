# Discovery worker for summa_discover_tests(). Not meant to be included
# directly — invoked via `cmake -P` from a POST_BUILD custom command.
#
# Runs TEST_EXECUTABLE with `--list`, and writes one add_test() per line of
# output to CTEST_FILE, named "<TEST_PREFIX><test case name>".
#
# Expected on the command line: -D TEST_EXECUTABLE=... -D TEST_PREFIX=... -D CTEST_FILE=...

execute_process(
    COMMAND "${TEST_EXECUTABLE}" --list
    OUTPUT_VARIABLE test_list_output
    RESULT_VARIABLE list_result
)

if(NOT list_result EQUAL 0)
    message(FATAL_ERROR "summa_discover_tests: '${TEST_EXECUTABLE} --list' failed")
endif()

string(REPLACE "\n" ";" test_names "${test_list_output}")

set(content "")
foreach(name ${test_names})
    string(STRIP "${name}" name)
    if(name)
        string(APPEND content "add_test(\"${TEST_PREFIX}${name}\" \"${TEST_EXECUTABLE}\" \"${name}\")\n")
    endif()
endforeach()

file(WRITE "${CTEST_FILE}" "${content}")
