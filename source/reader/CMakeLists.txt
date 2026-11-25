add_library(quux_reader STATIC quux_reader.c)
target_include_directories(quux_reader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quux_reader PUBLIC quux_collections PRIVATE m)
