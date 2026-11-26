add_library(quux_concurrency STATIC quux_futures.c quux_combinators.c quux_parallelism.c
  quux_agents.c)
target_include_directories(quux_concurrency PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(quux_concurrency PUBLIC quux_evaluator)
