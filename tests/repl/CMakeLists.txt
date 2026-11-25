add_test(NAME repl_session
  COMMAND ${CMAKE_COMMAND} -DQUUX=$<TARGET_FILE:quux>
          -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/repl_session_input.txt
          -DEXPECTED=${CMAKE_CURRENT_SOURCE_DIR}/repl_session_expected.txt
          -P ${CMAKE_CURRENT_SOURCE_DIR}/run_repl_test.cmake)
set_tests_properties(repl_session PROPERTIES LABELS unit)
