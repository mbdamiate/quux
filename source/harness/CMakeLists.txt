add_library(quux_harness STATIC quux_session.c quux_examples.c)
target_include_directories(quux_harness PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quux_harness PUBLIC quux_evaluator)
