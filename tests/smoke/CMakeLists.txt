# Toolchain smoke tests (T008, T009). Label: smoke.
# Each deliberate defect is registered only where its tool runs, and passes only when the tool's
# report appears in the output (the exit status is then ignored by CTest). On macOS the
# sanitizers abort after a report by default, and CTest counts an abort as a failure whatever the
# output says, so the defect tests set abort_on_error=0 (docs/toolchain-record.md).

add_executable(clean_program clean_program.c)
target_link_libraries(clean_program PRIVATE Threads::Threads)
add_test(NAME smoke_clean_program COMMAND clean_program)
set_tests_properties(smoke_clean_program PROPERTIES LABELS smoke)

add_executable(heap_overflow heap_overflow.c)
add_executable(data_race data_race.c)
target_link_libraries(data_race PRIVATE Threads::Threads)
add_executable(memory_leak memory_leak.c)

if(QUUX_SANITIZER STREQUAL "address-undefined")
  add_test(NAME smoke_heap_overflow_reported COMMAND heap_overflow)
  set_tests_properties(smoke_heap_overflow_reported PROPERTIES
    LABELS smoke PASS_REGULAR_EXPRESSION "AddressSanitizer: heap-buffer-overflow"
    ENVIRONMENT "ASAN_OPTIONS=abort_on_error=0")

  # LeakSanitizer is part of AddressSanitizer on Linux; on macOS arm64 it is not reliable
  # (base toolchain), so the leak check there is not registered.
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(NAME smoke_memory_leak_reported COMMAND memory_leak)
    set_tests_properties(smoke_memory_leak_reported PROPERTIES
      LABELS smoke PASS_REGULAR_EXPRESSION "LeakSanitizer: detected memory leaks"
      ENVIRONMENT "ASAN_OPTIONS=abort_on_error=0:detect_leaks=1")
  endif()
endif()

if(QUUX_SANITIZER STREQUAL "thread")
  add_test(NAME smoke_data_race_reported COMMAND data_race)
  set_tests_properties(smoke_data_race_reported PROPERTIES
    LABELS smoke PASS_REGULAR_EXPRESSION "ThreadSanitizer: data race"
    ENVIRONMENT "TSAN_OPTIONS=abort_on_error=0")
endif()

# valgrind runs on Linux in the debug preset (it does not run on macOS arm64).
find_program(QUUX_VALGRIND valgrind)
if(QUUX_VALGRIND AND QUUX_SANITIZER STREQUAL "none" AND CMAKE_BUILD_TYPE STREQUAL "Debug"
   AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_test(NAME smoke_clean_program_valgrind
    COMMAND ${QUUX_VALGRIND} --error-exitcode=99 --leak-check=full --errors-for-leak-kinds=definite
            $<TARGET_FILE:clean_program>)
  set_tests_properties(smoke_clean_program_valgrind PROPERTIES LABELS "smoke;valgrind")

  add_test(NAME smoke_memory_leak_reported_by_valgrind
    COMMAND ${QUUX_VALGRIND} --leak-check=full $<TARGET_FILE:memory_leak>)
  set_tests_properties(smoke_memory_leak_reported_by_valgrind PROPERTIES
    LABELS "smoke;valgrind" PASS_REGULAR_EXPRESSION "definitely lost: [1-9]")
endif()
