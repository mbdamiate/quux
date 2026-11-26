/* Reuse: keep. The executor interface (D-32, contracts/module-interface.md): the seam between the
 * semantics of futures, scopes and cancellation (module `concurrency`) and the thread model
 * (`executor_blocking` in Phase 1, the work-stealing scheduler of the Phase 3 VM). `concurrency`
 * only calls these operations; it never includes a thread header.
 *
 * Locking: an executor has one lock. Every operation except start_task, join_task, lock, unlock
 * and current_time is called with the lock held. The semantic state of futures and scopes is
 * protected by the same lock, so a task parked on a condition never misses a wake-up. */
#ifndef QUUX_EXECUTOR_H
#define QUUX_EXECUTOR_H

#include "quux_values.h"

typedef struct quux_task quux_task;
typedef struct quux_executor quux_executor;

/* Runs on the new task when it starts. */
typedef void (*quux_task_entry)(quux_task *task);

/* data-model.md "Task" (the fields that the semantics needs; the thread, the mutex and the
 * condition are the executor's, behind executor_data). */
struct quux_task {
    quux_context *context;
    /* Set by cancel! (through the scope) or by a combinator; read at every cancellation
     * point. */
    quux_atomic_boolean cancellation_requested;
    /* Nesting of shield-from-cancellation; only the task itself reads or writes it. */
    uint32_t shield_depth;
    /* The future object this task is parked on, or NULL (deadlock detection). Under the lock. */
    quux_object *waiting_for;
    quux_task_entry entry;
    void *executor_data;
};

typedef enum quux_park_outcome {
    QUUX_PARK_SATISFIED, /* the condition became true */
    QUUX_PARK_DEADLINE,  /* the deadline passed */
    QUUX_PARK_CANCELLED, /* the task's cancellation flag is set (only when observed) */
} quux_park_outcome;

/* Evaluated with the lock held, after every wake-up. */
typedef bool (*quux_park_condition)(void *data);

enum { QUUX_NO_DEADLINE = 0 };

typedef struct quux_executor_operations {
    /* Runs entry(task) concurrently. Returns false if it cannot (no thread); the caller raises
     * :resource-exhausted. Called with the lock held. */
    bool (*start_task)(quux_executor *executor, quux_task *task, quux_task_entry entry);
    /* Makes the calling thread a task (the session's own task). */
    bool (*attach_current_thread)(quux_executor *executor, quux_task *task);
    void (*detach_current_thread)(quux_executor *executor, quux_task *task);
    void (*lock)(quux_executor *executor);
    void (*unlock)(quux_executor *executor);
    /* Suspends the task until condition(data) is true, the monotonic clock reaches
     * deadline_nanoseconds (QUUX_NO_DEADLINE: never), or, when observe_cancellation is true,
     * the task's cancellation flag is set. Spurious wake-ups are handled inside. */
    quux_park_outcome (*park)(quux_executor *executor, quux_task *task,
                              quux_park_condition condition, void *data,
                              uint64_t deadline_nanoseconds, bool observe_cancellation);
    /* Makes a parked task re-evaluate its condition. */
    void (*wake)(quux_executor *executor, quux_task *task);
    /* Sets the task's cancellation flag and wakes it (from blocked input/output too, D-44). */
    void (*cancel)(quux_executor *executor, quux_task *task);
    /* Monotonic nanoseconds. */
    uint64_t (*current_time)(quux_executor *executor);
    /* Waits until a started task has ended and releases the executor's part of it. Called
     * without the lock. */
    void (*join_task)(quux_executor *executor, quux_task *task);
    void (*destroy)(quux_executor *executor);
} quux_executor_operations;

struct quux_executor {
    const quux_executor_operations *operations;
};

#endif
