# Feeds a session to the quux REPL on standard input and compares its output (T034, FR-013):
# printed values, the "!! kind: message" line, and a session that continues after an error.
#   cmake -DQUUX=... -DINPUT=... -DEXPECTED=... -P run_repl_test.cmake
execute_process(COMMAND ${QUUX} INPUT_FILE ${INPUT} OUTPUT_VARIABLE output RESULT_VARIABLE status)
file(READ ${EXPECTED} expected)
if(NOT status EQUAL 0)
  message(FATAL_ERROR "quux exited with ${status}")
endif()
if(NOT output STREQUAL expected)
  message(FATAL_ERROR "REPL output differs.\n--- expected\n${expected}--- got\n${output}")
endif()
