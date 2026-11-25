# Runs `quux FILE.qx INPUT` and checks its exit status against EXPECTED_STATUS (the command-line
# contract of feature 002: 2 on a usage or read error, which includes a malformed INPUT).
#   cmake -DQUUX=... -DPROGRAM=... -DINPUT_TEXT=... -DEXPECTED_STATUS=... -P run_status_test.cmake
execute_process(COMMAND ${QUUX} ${PROGRAM} ${INPUT_TEXT}
                OUTPUT_VARIABLE output ERROR_VARIABLE errors RESULT_VARIABLE status)
if(NOT status EQUAL EXPECTED_STATUS)
  message(FATAL_ERROR "quux exited with ${status}, expected ${EXPECTED_STATUS}\n${output}${errors}")
endif()
