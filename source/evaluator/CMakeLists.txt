set(quux_prelude_header ${CMAKE_CURRENT_BINARY_DIR}/generated/quux_core_prelude.h)
add_custom_command(
  OUTPUT ${quux_prelude_header}
  COMMAND ${CMAKE_COMMAND} -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/quux_core_prelude.qx
          -DOUTPUT=${quux_prelude_header} -DNAME=quux_core_prelude_text
          -P ${CMAKE_CURRENT_SOURCE_DIR}/embed_text.cmake
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/quux_core_prelude.qx ${CMAKE_CURRENT_SOURCE_DIR}/embed_text.cmake
  COMMENT "Embedding the core prelude")

add_library(quux_evaluator STATIC quux_evaluator.c quux_core_functions.c ${quux_prelude_header})
target_include_directories(quux_evaluator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(quux_evaluator PUBLIC quux_encoding quux_reader quux_printer PRIVATE m)
