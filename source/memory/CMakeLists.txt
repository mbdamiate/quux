add_library(quux_memory STATIC quux_memory.c)
target_include_directories(quux_memory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quux_memory PUBLIC quux_portability)
