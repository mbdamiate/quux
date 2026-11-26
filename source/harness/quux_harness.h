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

/* quux-test program DIRECTORY (D-46): runs one reference program and prints its report
 * (contracts/harness-report.md). Returns the exit status of contracts/command-line.md: 0 when
 * every assertion holds, 1 otherwise, 2 when conditions.qx or expected.qx is invalid; a run that
 * exceeds its wall-clock bound ends the process with status 3. */
typedef struct quux_program_options {
    const char *report_path; /* --report FILE, or NULL */
    bool seeded;             /* --seed N */
    uint64_t seed;
    quux_verify_mode verify_mode; /* --verify-roots: checked while one task runs (FR-027) */
} quux_program_options;

int quux_run_program(const char *directory, const quux_program_options *options);

#endif
