/* Shared start-up for the quux executables: a runtime and a context on a thread with a large
 * stack, so that deep (non-tail) recursion reaches :stack-exhausted before the C stack ends. */
#ifndef QUUX_SESSION_H
#define QUUX_SESSION_H

#include "quux_evaluator.h"

typedef int (*quux_session_body)(quux_context *context, void *data);

/* Creates a runtime, initializes the evaluator, runs body on a large-stack thread, and destroys
 * everything. Returns body's result, or 2 when start-up fails. */
int quux_session_run(quux_session_body body, void *data);

/* Prints an error value as "!! :kind: message". */
void quux_session_print_error(quux_context *context, FILE *stream, quux_value error);

/* The file runner shared by `quux FILE.qx [INPUT]` and `quux-test run` (D-56): evaluates the
 * file, reads input_text (or nothing when it is NULL) as quux data, calls (main input) in the
 * namespace current at the end of the file, and prints the returned value or the error on
 * standard output. verify_mode turns on the root verifier for the whole run. Returns the exit
 * status: 0 on return, 1 if loading, reading the input or main raised, 2 on a usage error. */
int quux_session_run_file(const char *path, const char *input_text, quux_verify_mode verify_mode);

/* Reads a whole file into a malloc'd buffer. */
char *quux_session_read_file(const char *path, size_t *length);

#endif
