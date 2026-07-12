if(NOT DEFINED BIBOCHAIN_EXECUTABLE OR
   NOT DEFINED INSPECTOR_EXECUTABLE OR
   NOT DEFINED TEST_STORAGE)
    message(FATAL_ERROR "Automatic mode test arguments are missing")
endif()

file(REMOVE_RECURSE "${TEST_STORAGE}")

execute_process(
    COMMAND "${BIBOCHAIN_EXECUTABLE}"
            "${TEST_STORAGE}"
            4096
            --auto
            --auto-count 12
            --batch-size 5
    RESULT_VARIABLE FIRST_RESULT
    OUTPUT_VARIABLE FIRST_OUTPUT
    ERROR_VARIABLE FIRST_ERROR)

if(NOT FIRST_RESULT EQUAL 0)
    message(FATAL_ERROR
            "First automatic run failed (${FIRST_RESULT})\n"
            "${FIRST_OUTPUT}\n${FIRST_ERROR}")
endif()

execute_process(
    COMMAND "${BIBOCHAIN_EXECUTABLE}"
            --auto
            --batch-size 3
            "${TEST_STORAGE}"
            --auto-count 7
            4096
    RESULT_VARIABLE SECOND_RESULT
    OUTPUT_VARIABLE SECOND_OUTPUT
    ERROR_VARIABLE SECOND_ERROR)

if(NOT SECOND_RESULT EQUAL 0)
    message(FATAL_ERROR
            "Automatic reload failed (${SECOND_RESULT})\n"
            "${SECOND_OUTPUT}\n${SECOND_ERROR}")
endif()

execute_process(
    COMMAND "${INSPECTOR_EXECUTABLE}" "${TEST_STORAGE}"
    RESULT_VARIABLE INSPECTOR_RESULT
    OUTPUT_VARIABLE INSPECTOR_OUTPUT
    ERROR_VARIABLE INSPECTOR_ERROR)

if(NOT INSPECTOR_RESULT EQUAL 0)
    message(FATAL_ERROR
            "Inspector found invalid automatic-mode storage "
            "(${INSPECTOR_RESULT})\n"
            "${INSPECTOR_OUTPUT}\n${INSPECTOR_ERROR}")
endif()

file(REMOVE_RECURSE "${TEST_STORAGE}")
