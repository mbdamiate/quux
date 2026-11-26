# quux — top-level build (feature 002, decisions D-29..D-33, D-46).
cmake_minimum_required(VERSION 3.28)

# Compiler selection (D-29): clang major 22 everywhere unless QUUX_C_COMPILER says otherwise.
# It must happen before project(), which locks the compiler in.
if(NOT CMAKE_C_COMPILER)
  if(DEFINED ENV{QUUX_C_COMPILER} AND NOT "$ENV{QUUX_C_COMPILER}" STREQUAL "")
    set(CMAKE_C_COMPILER "$ENV{QUUX_C_COMPILER}")
  else()
    find_program(quux_default_compiler
      NAMES clang-22 clang
      PATHS /opt/homebrew/opt/llvm@22/bin /usr/lib/llvm-22/bin
      NO_DEFAULT_PATH)
    if(NOT quux_default_compiler)
      find_program(quux_default_compiler NAMES clang-22)
    endif()
    if(quux_default_compiler)
      set(CMAKE_C_COMPILER "${quux_default_compiler}")
    endif()
  endif()
endif()

project(quux LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON) # gnu17; extensions are used only through the portability header (D-48)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Sanitizer selection, set by the presets: none, address-undefined, thread.
set(QUUX_SANITIZER "none" CACHE STRING "Sanitizer set: none, address-undefined, thread")
set_property(CACHE QUUX_SANITIZER PROPERTY STRINGS none address-undefined thread)

set(quux_warning_options -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes
  -Wmissing-prototypes -Wconversion -Wno-gnu-zero-variadic-macro-arguments)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
  list(REMOVE_ITEM quux_warning_options -Wno-gnu-zero-variadic-macro-arguments)
endif()
add_compile_options(${quux_warning_options})
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-Werror)
endif()

if(QUUX_SANITIZER STREQUAL "address-undefined")
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
elseif(QUUX_SANITIZER STREQUAL "thread")
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread)
elseif(NOT QUUX_SANITIZER STREQUAL "none")
  message(FATAL_ERROR "Unknown QUUX_SANITIZER value: ${QUUX_SANITIZER}")
endif()

find_package(Threads REQUIRED)
enable_testing()

# Modules (D-32): one static library per directory under source/, each with one public header
# source/<module>/quux_<module>.h. The order follows the dependency direction of
# contracts/module-interface.md rule 3; a module may link only the modules before it.
set(quux_modules
  portability memory values hashing collections reader printer namespaces expander encoding
  evaluator concurrency executor_blocking input_output simulation harness)
foreach(quux_module IN LISTS quux_modules)
  set(quux_module_directory "${CMAKE_SOURCE_DIR}/source/${quux_module}")
  if(EXISTS "${quux_module_directory}/CMakeLists.txt")
    add_subdirectory("${quux_module_directory}")
  endif()
endforeach()
add_subdirectory(tests/unit)
if(EXISTS "${CMAKE_SOURCE_DIR}/source/main/CMakeLists.txt")
  add_subdirectory(source/main)
endif()

# Tests
add_subdirectory(tests/smoke)
add_subdirectory(tests/repl)
add_subdirectory(tests/examples)
add_subdirectory(tests/ergonomics)
add_subdirectory(tests/programs)
add_subdirectory(tests/harness)
