#include "quux_executor_blocking.h"

#include <stdlib.h>

typedef struct blocking_executor {
    quux_executor executor;
    quux_mutex lock;
    size_t stack_bytes;
} blocking_executor;

/* The executor's part of a task (data-model.md Task.thread, Task.condition; Task.mutex is the
 * executor's single lock). */
typedef struct blocking_task {
    quux_thread *thread;
    quux_condition condition;
    quux_task *task;
} blocking_task;

static blocking_executor *blocking(quux_executor *executor) {
    return (blocking_executor *)(void *)executor;
}

static blocking_task *task_data(quux_task *task) {
    return task->executor_data;
}

static void *run(void *argument) {
    quux_task *task = argument;
    task->entry(task);
    return NULL;
}

static blocking_task *create_task_data(quux_task *task) {
    blocking_task *data = calloc(1, sizeof *data);
    if (data != NULL) {
        quux_condition_initialize(&data->condition);
        data->task = task;
    }
    return data;
}

static void destroy_task_data(quux_task *task) {
    blocking_task *data = task_data(task);
    if (data != NULL) {
        quux_condition_destroy(&data->condition);
        free(data);
        task->executor_data = NULL;
    }
}

static bool start_task(quux_executor *executor, quux_task *task, quux_task_entry entry) {
    blocking_task *data = create_task_data(task);
    if (data == NULL) {
        return false;
    }
    task->executor_data = data;
    task->entry = entry;
    if (!quux_thread_start(&data->thread, blocking(executor)->stack_bytes, run, task)) {
        destroy_task_data(task);
        return false;
    }
    return true;
}

static bool attach_current_thread(quux_executor *executor, quux_task *task) {
    (void)executor;
    task->executor_data = create_task_data(task);
    return task->executor_data != NULL;
}

static void detach_current_thread(quux_executor *executor, quux_task *task) {
    (void)executor;
    destroy_task_data(task);
}

static void lock(quux_executor *executor) {
    quux_mutex_lock(&blocking(executor)->lock);
}

static void unlock(quux_executor *executor) {
    quux_mutex_unlock(&blocking(executor)->lock);
}

static quux_park_outcome park(quux_executor *executor, quux_task *task,
                              quux_park_condition condition, void *data,
                              uint64_t deadline_nanoseconds, bool observe_cancellation) {
    blocking_task *own = task_data(task);
    for (;;) {
        if (condition(data)) {
            return QUUX_PARK_SATISFIED;
        }
        if (observe_cancellation && quux_atomic_load_acquire(&task->cancellation_requested)) {
            return QUUX_PARK_CANCELLED;
        }
        if (deadline_nanoseconds == QUUX_NO_DEADLINE) {
            quux_condition_wait(&own->condition, &blocking(executor)->lock);
        } else if (quux_monotonic_nanoseconds() >= deadline_nanoseconds ||
                   !quux_condition_wait_until(&own->condition, &blocking(executor)->lock,
                                              deadline_nanoseconds)) {
            if (condition(data)) {
                return QUUX_PARK_SATISFIED;
            }
            if (quux_monotonic_nanoseconds() >= deadline_nanoseconds) {
                return QUUX_PARK_DEADLINE;
            }
        }
    }
}

static void wake(quux_executor *executor, quux_task *task) {
    (void)executor;
    blocking_task *data = task_data(task);
    if (data != NULL) {
        quux_condition_signal(&data->condition);
    }
}

static void cancel(quux_executor *executor, quux_task *task) {
    quux_atomic_store_release(&task->cancellation_requested, true);
    wake(executor, task);
}

static uint64_t current_time(quux_executor *executor) {
    (void)executor;
    return quux_monotonic_nanoseconds();
}

static void join_task(quux_executor *executor, quux_task *task) {
    (void)executor;
    blocking_task *data = task_data(task);
    if (data != NULL && data->thread != NULL) {
        quux_thread_join(data->thread);
        data->thread = NULL;
    }
    destroy_task_data(task);
}

static void destroy(quux_executor *executor) {
    quux_mutex_destroy(&blocking(executor)->lock);
    free(blocking(executor));
}

static const quux_executor_operations blocking_operations = {
    start_task,
    attach_current_thread,
    detach_current_thread,
    lock,
    unlock,
    park,
    wake,
    cancel,
    current_time,
    join_task,
    destroy,
};

quux_executor *quux_executor_blocking_create(size_t stack_bytes) {
    blocking_executor *executor = calloc(1, sizeof *executor);
    if (executor == NULL) {
        return NULL;
    }
    executor->executor.operations = &blocking_operations;
    executor->stack_bytes = stack_bytes;
    quux_mutex_initialize(&executor->lock);
    return &executor->executor;
}
