add_library(quux_values STATIC quux_values.c)
target_include_directories(quux_values PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quux_values PUBLIC quux_memory)
