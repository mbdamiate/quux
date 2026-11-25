add_executable(quux quux.c)
target_link_libraries(quux PRIVATE quux_harness)

add_executable(quux-test quux_test.c)
target_link_libraries(quux-test PRIVATE quux_unit_tests quux_harness)

foreach(quux_unit_module IN LISTS QUUX_UNIT_TEST_MODULES)
  add_test(NAME unit_${quux_unit_module} COMMAND quux-test unit ${quux_unit_module})
  set_tests_properties(unit_${quux_unit_module} PROPERTIES LABELS unit)
endforeach()
