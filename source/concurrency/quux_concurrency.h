/* Reuse: adapt. Futures, scopes, cancellation and the wait-for graph (sections 06 and 07,
 * D-07..D-14, D-40), the combinators of section 08, fork-join (section 11) and agents (section 09),
 * written against the executor interface (quux_executor.h) and never against threads. The Phase 3
 * VM keeps the state machines and the propagation rules and supplies its own executor. */
#ifndef QUUX_CONCURRENCY_H
#define QUUX_CONCURRENCY_H

#include "quux_evaluator.h"
#include "quux_executor.h"

/* Installs the concurrency forms and functions into a runtime whose evaluator is initialized.
 * The calling thread becomes the session's task, and the context's current scope becomes the
 * implicit root scope (section 07 rule 2). The runtime owns the executor from now on. */
bool quux_concurrency_initialize(quux_context *context, quux_executor *executor);

/* Joins the root scope (waits for every task still running), then releases the session's task
 * and the executor. Returns false, with the root scope's primary error pending, if a future
 * created directly in the root scope failed. */
bool quux_concurrency_finish(quux_context *context);

/* with-scope from C: runs body in a new child of the current scope and joins it (section 07
 * rules 4-9). Each evaluation of the interactive loop runs this way (section 07 rule 2). */
typedef bool (*quux_scope_body)(quux_context *context, quux_value scope, void *data,
                                quux_value *result);
bool quux_concurrency_with_scope(quux_context *context, quux_scope_body body, void *data,
                                 quux_value *result);

/* A cancellation point that waits (sleep, and the injected delays of the harness). */
bool quux_concurrency_sleep(quux_context *context, uint64_t milliseconds);

/* ---- Observation (harness) ---- */

typedef enum quux_future_state {
    QUUX_FUTURE_EMPTY,
    QUUX_FUTURE_FILLED,
    QUUX_FUTURE_FAILED,
} quux_future_state;

quux_future_state quux_future_current_state(quux_value future);
/* The error of a :failed future, else nothing. */
quux_value quux_future_error(quux_value future);
/* Whether any scope ended with a suppressed error, that is, whether the choice of its primary
 * error depended on the order of completion of racing failures (H9 evidence, D-49). */
bool quux_concurrency_primary_error_raced(quux_runtime *runtime);
/* Seeds the generator of scope identifiers (quux-test --seed); otherwise they come from the
 * operating system's entropy. */
void quux_concurrency_set_seed(quux_runtime *runtime, uint64_t seed);

#endif
