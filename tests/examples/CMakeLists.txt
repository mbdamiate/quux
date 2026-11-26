# The specification example suite (T035, FR-026): one test per section. Sections that need a
# later user story are labelled `pending` until the task that makes them pass moves them.
set(quux_example_sections_ready
  01-values-and-equality 02-collections 03-evaluation-and-tail-calls 04-macros 05-numerics
  06-futures 07-scopes-and-cancellation 08-combinators 09-agents 12-errors
  13-serialization-and-identity 14-modules)
set(quux_example_sections_pending
  10-streams 11-data-parallelism appendix-input-output)

foreach(section IN LISTS quux_example_sections_ready)
  add_test(NAME examples_${section}
    COMMAND quux-test examples ${CMAKE_SOURCE_DIR}/docs/spec/${section}.md)
  set_tests_properties(examples_${section} PROPERTIES LABELS examples)
  # The same examples under the root verifier (D-34): every allocation outside the long loops of
  # section 03, which uses the amortized mode.
  if(section STREQUAL "03-evaluation-and-tail-calls")
    set(verify_option --verify-roots)
  else()
    set(verify_option --verify-roots-every-allocation)
  endif()
  add_test(NAME examples_${section}_verified
    COMMAND quux-test ${verify_option} examples ${CMAKE_SOURCE_DIR}/docs/spec/${section}.md)
  set_tests_properties(examples_${section}_verified PROPERTIES LABELS "examples;verified")
endforeach()

foreach(section IN LISTS quux_example_sections_pending)
  add_test(NAME examples_${section}
    COMMAND quux-test examples
            ${CMAKE_SOURCE_DIR}/docs/spec/${section}.md)
  set_tests_properties(examples_${section} PROPERTIES LABELS pending)
endforeach()
