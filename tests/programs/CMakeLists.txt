# The 12 reference programs (T039, D-46): `quux-test program programs/NN-name`. A program that does
# not pass yet carries the `pending` label, which CI and the local check skip; the task that makes
# it pass moves it to the ready list. `concurrent` marks the programs of the 20-run TSan loop
# (contracts/command-line.md).
set(quux_programs_ready
  01-parallel-search
  02-first-response-wins
  03-timeout-and-errors
  08-fork-join
  09-deadlock)
set(quux_programs_pending
  04-bank-account-agent
  05-parallel-transform
  06-file-line-stream
  07-backpressure
  10-chat-server
  11-remote-futures-crash
  12-remote-agent-stream)
set(quux_programs_concurrent
  01-parallel-search 02-first-response-wins 03-timeout-and-errors 04-bank-account-agent
  06-file-line-stream 07-backpressure 08-fork-join 09-deadlock 10-chat-server
  11-remote-futures-crash 12-remote-agent-stream)

foreach(program IN LISTS quux_programs_ready)
  add_test(NAME programs_${program}
    COMMAND quux-test program ${CMAKE_SOURCE_DIR}/programs/${program})
  set(labels programs)
  if(program IN_LIST quux_programs_concurrent)
    list(APPEND labels concurrent)
  endif()
  set_tests_properties(programs_${program} PROPERTIES LABELS "${labels}" TIMEOUT 120)
  # The same run under the root verifier, which checks while one task runs (D-34, FR-027).
  if(NOT QUUX_SANITIZER STREQUAL "thread")
    add_test(NAME programs_${program}_verified
      COMMAND quux-test --verify-roots program ${CMAKE_SOURCE_DIR}/programs/${program})
    set_tests_properties(programs_${program}_verified PROPERTIES
      LABELS "programs;verified" TIMEOUT 300)
  endif()
endforeach()

foreach(program IN LISTS quux_programs_pending)
  add_test(NAME programs_${program}
    COMMAND quux-test program ${CMAKE_SOURCE_DIR}/programs/${program})
  set_tests_properties(programs_${program} PROPERTIES LABELS pending TIMEOUT 120)
endforeach()
