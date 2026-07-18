# Discovery worker for summa_discover_tests(). Not meant to be included
# directly — invoked via `cmake -P` from a POST_BUILD custom command.
#
# Runs TEST_EXECUTABLE with `--list`, and writes one add_test() per line of
# output to CTEST_FILE, named "<TEST_PREFIX><test case name>". For each case,
# also locates its function definition in SOURCE_FILE (the first line
# containing "<case name>(", which is the definition itself since test
# functions are never called except by SUMMA_TEST_RUN, whose call syntax is
# "SUMMA_TEST_RUN(<case name>)" — no "<case name>(" substring) and records it
# via ENVIRONMENT TEST_FILE/TEST_LINE.
#
# Expected on the command line:
#   -D TEST_EXECUTABLE=... -D TEST_PREFIX=... -D SOURCE_FILE=... -D CTEST_FILE=...

execute_process(
    COMMAND "${TEST_EXECUTABLE}" --list
    OUTPUT_VARIABLE test_list_output
    RESULT_VARIABLE list_result
)

if(NOT list_result EQUAL 0)
    message(FATAL_ERROR "summa_discover_tests: '${TEST_EXECUTABLE} --list' failed")
endif()

string(REPLACE "\n" ";" test_names "${test_list_output}")

file(STRINGS "${SOURCE_FILE}" source_lines)
list(LENGTH source_lines source_line_count)

set(content "")
foreach(name ${test_names})
    string(STRIP "${name}" name)
    if(name)
        set(test_id "${TEST_PREFIX}${name}")
        string(APPEND content "add_test(\"${test_id}\" \"${TEST_EXECUTABLE}\" \"${name}\")\n")

        set(def_line "")
        if(source_line_count GREATER 0)
            math(EXPR last_index "${source_line_count} - 1")
            foreach(idx RANGE 0 ${last_index})
                list(GET source_lines ${idx} line)
                if(line MATCHES "(^|[^A-Za-z0-9_])${name}[ ]*\\(")
                    math(EXPR def_line "${idx} + 1")
                    break()
                endif()
            endforeach()
        endif()

        if(def_line)
            string(APPEND content
                "set_tests_properties(\"${test_id}\" PROPERTIES ENVIRONMENT \"TEST_FILE=${SOURCE_FILE};TEST_LINE=${def_line}\")\n")
        endif()
    endif()
endforeach()

file(WRITE "${CTEST_FILE}" "${content}")
