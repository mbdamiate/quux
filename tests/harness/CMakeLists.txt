# Exit statuses of `quux-test program` (contracts/command-line.md, T040): 2 for an unknown key in
# conditions.qx or an unknown assertion in expected.qx (FR-025), 3 when the watchdog stops a
# program that runs past :bounds :wall-clock-milliseconds.
foreach(case_and_status unknown-key:2 unknown-assertion:2 never-ends:3)
  string(REPLACE ":" ";" parts ${case_and_status})
  list(GET parts 0 case)
  list(GET parts 1 status)
  add_test(NAME harness_${case}
    COMMAND ${CMAKE_COMMAND} -DQUUX_TEST=$<TARGET_FILE:quux-test>
            -DDIRECTORY=${CMAKE_CURRENT_SOURCE_DIR}/${case} -DEXPECTED_STATUS=${status}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/run_program_status.cmake)
  set_tests_properties(harness_${case} PROPERTIES LABELS unit TIMEOUT 60)
endforeach()
