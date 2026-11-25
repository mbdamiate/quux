add_library(quux_printer STATIC quux_printer.c)
target_include_directories(quux_printer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quux_printer PUBLIC quux_collections PRIVATE m)
