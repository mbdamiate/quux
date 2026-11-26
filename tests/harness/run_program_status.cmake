# Runs `quux-test program DIRECTORY` and checks its exit status.
#   cmake -DQUUX_TEST=... -DDIRECTORY=... -DEXPECTED_STATUS=... -P run_program_status.cmake
execute_process(COMMAND ${QUUX_TEST} program ${DIRECTORY}
                OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE status)
if(NOT status EQUAL EXPECTED_STATUS)
  message(FATAL_ERROR "quux-test exited with ${status}, expected ${EXPECTED_STATUS}\n${output}${errors}")
endif()
message(STATUS "${output}")
