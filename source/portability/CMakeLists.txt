add_library(quux_portability STATIC quux_portability.c)
target_include_directories(quux_portability PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quux_portability PUBLIC Threads::Threads)
