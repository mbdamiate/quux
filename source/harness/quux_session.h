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

/* Reads a whole file into a malloc'd buffer. */
char *quux_session_read_file(const char *path, size_t *length);

#endif
