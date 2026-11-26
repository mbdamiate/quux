/* Private to the concurrency module: the runtime records of futures and scopes (data-model.md
 * "Future", "Scope") and the helpers shared by futures, combinators, fork-join and agents. */
#ifndef QUUX_CONCURRENCY_INTERNAL_H
#define QUUX_CONCURRENCY_INTERNAL_H

#include "quux_concurrency.h"

#include "quux_collections.h"

/* ---- Futures: a FUTURE object; values below, bytes hold a future_record ---- */

enum {
    QUUX_FUTURE_VALUE,
    QUUX_FUTURE_ERROR,
    QUUX_FUTURE_SCOPE,
    QUUX_FUTURE_VALUE_COUNT,
};

typedef struct quux_future_record {
    /* data-model.md Future.state: QUUX_FUTURE_EMPTY, then one transition (under the lock;
     * read without it with acquire order). */
    quux_atomic_unsigned state;
    uint64_t identifier;
    /* Future.computing_task: the task that fills it, NULL once that task has ended. */
    quux_task *computing_task;
    /* Future.waiters: tasks parked until it settles. */
    quux_task **waiters;
    uint32_t waiter_count;
    uint32_t waiter_capacity;
    /* Cancelled by a combinator that no longer needs it: its :cancelled does not fail the
     * scope (section 07 rule 8). */
    bool cancelled_by_combinator;
    /* An alternative of first-success: its failure does not fail the scope (D-80). */
    bool handled_by_combinator;
} quux_future_record;

/* ---- Scopes: a SCOPE object; values below, bytes hold a scope_record ---- */

enum {
    QUUX_SCOPE_PARENT,
    QUUX_SCOPE_CHILDREN, /* a list of the futures and child scopes created in it */
    QUUX_SCOPE_PRIMARY_ERROR,
    QUUX_SCOPE_SUPPRESSED, /* a list, latest first */
    QUUX_SCOPE_VALUE_COUNT,
};

typedef enum quux_scope_state {
    QUUX_SCOPE_RUNNING,
    QUUX_SCOPE_CANCELLING,
    QUUX_SCOPE_JOINED_SUCCESSFULLY,
    QUUX_SCOPE_JOINED_FAILED,
} quux_scope_state;

typedef struct quux_scope_record {
    quux_atomic_unsigned state; /* quux_scope_state; written under the lock */
    /* Cancelled by cancel!, by its parent or by its task, rather than by a failure: it raises
     * :cancelled when there is no primary error. */
    bool cancelled;
    quux_task *body_task;
    /* Tasks started in this scope that have not ended. The join waits until it is 0. */
    uint32_t running_task_count;
    /* Every task started in this scope, released by the join. */
    quux_task **tasks;
    size_t task_count;
    size_t task_capacity;
    unsigned char identifier[16]; /* Scope.identifier: 128 random bits */
} quux_scope_record;

/* ---- Module state (runtime->concurrency) ---- */

enum {
    QUUX_HELPER_ALL,
    QUUX_HELPER_FIRST_SUCCESS,
    QUUX_HELPER_THEN,
    QUUX_HELPER_WITH_TIMEOUT,
    QUUX_HELPER_FORK_JOIN_SOLVE,
    QUUX_HELPER_AGENT_UPDATE,
    QUUX_HELPER_COUNT,
};

typedef struct quux_concurrency_state {
    quux_executor *executor;
    quux_task session_task;
    quux_value root_scope;
    /* Primitives that are not in any namespace: the bodies of the tasks that combinators,
     * fork-join and agents start. */
    quux_value helpers[QUUX_HELPER_COUNT];
    quux_atomic_unsigned next_future_identifier;
    quux_atomic_boolean primary_error_raced;
    uint64_t random_state; /* under the lock; used when seeded */
    bool seeded;
} quux_concurrency_state;

static inline quux_concurrency_state *quux_concurrency_of(quux_context *context) {
    return context->runtime->concurrency;
}

static inline quux_future_record *quux_future_record_of(quux_value future) {
    return (quux_future_record *)(void *)quux_object_bytes(quux_checked_object(future));
}

static inline quux_scope_record *quux_scope_record_of(quux_value scope) {
    return (quux_scope_record *)(void *)quux_object_bytes(quux_checked_object(scope));
}

void quux_concurrency_lock(quux_context *context);
void quux_concurrency_unlock(quux_context *context);

/* Creates a future in `scope` and starts a task that fills it with (function arguments...),
 * where arguments is an ARGUMENTS object. */
bool quux_concurrency_spawn(quux_context *context, quux_value scope, quux_value function,
                            quux_value arguments, quux_value *future);

/* wait-for (section 06 rules 8-10). */
bool quux_concurrency_wait_for(quux_context *context, quux_value future, quux_value *result);

/* Waits until one of the futures of `futures` (a vector) that is still empty settles, the
 * deadline passes (QUUX_NO_DEADLINE: never), or the task is cancelled. Returns the outcome; on
 * QUUX_PARK_CANCELLED the caller raises :cancelled. Called without the lock. */
quux_park_outcome quux_concurrency_wait_for_any(quux_context *context, quux_value futures,
                                                uint64_t deadline_nanoseconds);

/* Cancels a future that a combinator no longer needs: its task observes :cancelled at its next
 * cancellation point, and the :cancelled does not fail the scope. Called with the lock held. */
void quux_concurrency_cancel_unneeded_locked(quux_context *context, quux_value future);

/* The executor's monotonic clock, in nanoseconds. */
uint64_t quux_concurrency_now(quux_context *context);

/* Marks the futures of `futures` (a vector) as handled by first-success (D-80). */
void quux_concurrency_handle_failures(quux_context *context, quux_value futures);

/* Whether the current task must observe cancellation at a cancellation point now. */
bool quux_concurrency_cancellation_observed(quux_context *context);
/* Raises :cancelled (always returns false). */
bool quux_concurrency_raise_cancelled(quux_context *context);

/* Wakes the given tasks after a change they may be parked on (agents). Lock held. */
void quux_concurrency_wake_locked(quux_context *context, quux_task *task);
/* Parks the current task until condition(data) holds, without observing cancellation. Lock
 * held. */
void quux_concurrency_park_locked(quux_context *context, quux_park_condition condition, void *data);

/* Registration of the functions of the other files of the module. */
bool quux_combinators_register(quux_context *context);
bool quux_parallelism_register(quux_context *context);
bool quux_agents_register(quux_context *context);

#endif
