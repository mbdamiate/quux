add_library(quux_encoding STATIC quux_encoding.c)
target_include_directories(quux_encoding PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quux_encoding PUBLIC quux_expander quux_hashing PRIVATE m)
