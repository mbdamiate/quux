/* Reuse: keep. The test harness (D-46): the specification example suite now, the reference
 * program runner from User Story 3 on. */
#ifndef QUUX_HARNESS_H
#define QUUX_HARNESS_H

#include "quux_evaluator.h"

/* Features an example block may require with a line `<!-- example-requires: FEATURE -->` right
 * before its fence. Blocks whose feature is not available are skipped and counted. */
typedef struct quux_example_options {
    const char *available_features; /* space-separated, for example "core" */
    bool verify_roots;
    bool verify_every_allocation; /* much slower; catches more missing roots */
} quux_example_options;

/* Runs the ;=> and ;!! examples of one Markdown file in a fresh runtime. Prints one line per
 * failure and a summary. Returns the number of failures (0 when all pass). */
int quux_run_examples_file(const char *path, const quux_example_options *options);

#endif
