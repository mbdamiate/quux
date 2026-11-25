add_library(quux_collections STATIC quux_sequences.c quux_champ.c quux_equality.c)
target_include_directories(quux_collections PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quux_collections PUBLIC quux_values PRIVATE quux_hashing m)
