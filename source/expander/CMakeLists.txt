add_library(quux_expander STATIC quux_expander.c)
target_include_directories(quux_expander PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quux_expander PUBLIC quux_namespaces)
