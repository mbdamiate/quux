# The ergonomics programs of feature 003 (D-55..D-57, contracts/test-driver.md). Each ready
# program is a plain run with `quux`, a run under the root verifier with `quux-test run`, and a
# valgrind run on Linux. Pending programs carry the `pending` label, which CI and the local check
# skip. Moving a program from the pending list to the ready list turns its tests on.
set(quux_ergonomics_ready
  01-nested-data
  02-text-prepared-input
  04-tree-and-parser
  05-errors-prepared-input
  07-macros
  08-namespaces
  09-numerics
  10-common-mistakes)
set(quux_ergonomics_pending
  03-text-raw
  06-errors-reading-text)

set(quux_ergonomics_directory ${CMAKE_SOURCE_DIR}/programs/ergonomics)
set(quux_ergonomics_driver ${CMAKE_CURRENT_SOURCE_DIR}/run_program_test.cmake)
find_program(QUUX_ERGONOMICS_VALGRIND valgrind)

# The verified run uses --verify-roots-every-allocation unless the program's README says
# "Verifier: amortized" (contracts/program-layout.md).
function(quux_ergonomics_verify_option program result)
  file(READ ${quux_ergonomics_directory}/${program}/README.md readme)
  if(readme MATCHES "Verifier: amortized")
    set(${result} --verify-roots PARENT_SCOPE)
  else()
    set(${result} --verify-roots-every-allocation PARENT_SCOPE)
  endif()
endfunction()

function(quux_ergonomics_add_test name program labels)
  # The remaining arguments are the runner command and its options.
  set(runner ${ARGN})
  string(REPLACE ";" "|" runner_text "${runner}")
  add_test(NAME ${name}
    COMMAND ${CMAKE_COMMAND} "-DRUNNER=${runner_text}"
            -DPROGRAM_DIRECTORY=${quux_ergonomics_directory}/${program}
            -P ${quux_ergonomics_driver})
  set_tests_properties(${name} PROPERTIES LABELS "${labels}" TIMEOUT 30)
endfunction()

foreach(program IN LISTS quux_ergonomics_ready)
  quux_ergonomics_add_test(ergonomics_${program} ${program} ergonomics $<TARGET_FILE:quux>)
  quux_ergonomics_verify_option(${program} verify_option)
  quux_ergonomics_add_test(ergonomics_${program}_verified ${program} "ergonomics;verified"
    $<TARGET_FILE:quux-test> ${verify_option} run)
  # valgrind cannot run sanitized binaries: only the plain debug build (as in tests/smoke).
  if(QUUX_ERGONOMICS_VALGRIND AND QUUX_SANITIZER STREQUAL "none"
     AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    quux_ergonomics_add_test(ergonomics_${program}_valgrind ${program} "ergonomics;valgrind"
      ${QUUX_ERGONOMICS_VALGRIND} --error-exitcode=99 --leak-check=full --quiet
      $<TARGET_FILE:quux>)
  endif()
endforeach()

foreach(program IN LISTS quux_ergonomics_pending)
  quux_ergonomics_add_test(ergonomics_${program} ${program} pending $<TARGET_FILE:quux>)
endforeach()
