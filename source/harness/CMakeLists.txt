add_library(quux_harness STATIC quux_session.c quux_examples.c quux_programs.c)
target_include_directories(quux_harness PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quux_harness PUBLIC quux_concurrency quux_executor_blocking)

# The target and preset printed in every program report (contracts/harness-report.md).
if(APPLE)
  set(quux_target_system macos)
else()
  set(quux_target_system linux)
endif()
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
  set(quux_target_architecture arm64)
else()
  set(quux_target_architecture ${CMAKE_SYSTEM_PROCESSOR})
endif()
if(QUUX_SANITIZER STREQUAL "address-undefined")
  set(quux_preset_name asan-ubsan)
elseif(QUUX_SANITIZER STREQUAL "thread")
  set(quux_preset_name tsan)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(quux_preset_name release)
else()
  set(quux_preset_name debug)
endif()
target_compile_definitions(quux_harness PRIVATE
  QUUX_TARGET_NAME="${quux_target_system}-${quux_target_architecture}"
  QUUX_TARGET_SYSTEM="${quux_target_system}"
  QUUX_TARGET_ARCHITECTURE="${quux_target_architecture}"
  QUUX_PRESET_NAME="${quux_preset_name}")
