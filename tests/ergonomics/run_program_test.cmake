# Runs one ergonomics program and compares what a user sees (D-56, contracts/test-driver.md):
# standard output, byte for byte, and the exit status.
#   cmake -DRUNNER="command|option|…" -DPROGRAM_DIRECTORY=<dir> -P run_program_test.cmake
# RUNNER separates its words with "|". The program's input.txt, if present, is passed as one more
# argument with its final newline removed. The expected status is expected-exit-status.txt, or 0.
string(REPLACE "|" ";" runner "${RUNNER}")
set(arguments ${runner} ${PROGRAM_DIRECTORY}/main.qx)
if(EXISTS ${PROGRAM_DIRECTORY}/input.txt)
  file(READ ${PROGRAM_DIRECTORY}/input.txt input)
  string(REGEX REPLACE "\n$" "" input "${input}")
  list(APPEND arguments "${input}")
endif()

set(expected_status 0)
if(EXISTS ${PROGRAM_DIRECTORY}/expected-exit-status.txt)
  file(READ ${PROGRAM_DIRECTORY}/expected-exit-status.txt expected_status)
  string(STRIP "${expected_status}" expected_status)
endif()
file(READ ${PROGRAM_DIRECTORY}/expected-output.txt expected)

execute_process(COMMAND ${arguments}
  OUTPUT_VARIABLE output ERROR_VARIABLE error_output RESULT_VARIABLE status)

if(NOT output STREQUAL expected OR NOT status STREQUAL expected_status)
  message(FATAL_ERROR
    "Program ${PROGRAM_DIRECTORY} differs.\n"
    "--- expected (exit status ${expected_status})\n${expected}"
    "--- got (exit status ${status})\n${output}"
    "--- standard error\n${error_output}")
endif()
