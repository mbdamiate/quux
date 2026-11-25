add_executable(quux quux.c)
target_link_libraries(quux PRIVATE quux_harness)

add_executable(quux-test quux_test.c)
target_link_libraries(quux-test PRIVATE quux_unit_tests quux_harness)

foreach(quux_unit_module IN LISTS QUUX_UNIT_TEST_MODULES)
  add_test(NAME unit_${quux_unit_module} COMMAND quux-test unit ${quux_unit_module})
  set_tests_properties(unit_${quux_unit_module} PROPERTIES LABELS unit)
endforeach()

add_executable(quux-name-check quux_name_check.c)
target_link_libraries(quux-name-check PRIVATE quux_harness)
add_test(NAME names_quux COMMAND quux-name-check WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
add_test(NAME names_c COMMAND quux-name-check --c source tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(names_quux names_c PROPERTIES LABELS names)
