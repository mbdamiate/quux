/* Futures, scopes, cancellation and deadlock detection (sections 06 and 07, D-07..D-14, D-40).
 *
 * All semantic state (future states, scope states and children, the wait-for graph) is protected
 * by the executor's lock. Futures and scopes are heap objects; their bookkeeping that is not a
 * quux value (tasks, waiters) lives in the object's bytes. A task's function, arguments and future
 * are roots of the task's own context, which its creator registers before the task starts. */
#include "quux_concurrency_internal.h"

#include <stdlib.h>
#include <string.h>

typedef struct spawned_task {
    quux_task task;
    quux_context context;
} spawned_task;

static quux_executor *executor_of(quux_context *context) {
    return quux_concurrency_of(context)->executor;
}

void quux_concurrency_lock(quux_context *context) {
    quux_executor *executor = executor_of(context);
    executor->operations->lock(executor);
}

void quux_concurrency_unlock(quux_context *context) {
    quux_executor *executor = executor_of(context);
    executor->operations->unlock(executor);
}

uint64_t quux_concurrency_now(quux_context *context) {
    quux_executor *executor = executor_of(context);
    return executor->operations->current_time(executor);
}

static quux_scope_state scope_state(quux_value scope) {
    return (quux_scope_state)quux_atomic_load_acquire(&quux_scope_record_of(scope)->state);
}

static void set_scope_state(quux_value scope, quux_scope_state state) {
    quux_atomic_store_release(&quux_scope_record_of(scope)->state, (uint64_t)state);
}

static bool grow(void **array, size_t *capacity, size_t element_size, size_t needed) {
    if (needed <= *capacity) {
        return true;
    }
    size_t new_capacity = *capacity == 0 ? 8 : *capacity * 2;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    void *grown = realloc(*array, new_capacity * element_size);
    if (grown == NULL) {
        return false;
    }
    *array = grown;
    *capacity = new_capacity;
    return true;
}

static bool error_has_kind(quux_value error, const char *kind_name) {
    return quux_is_kind(error, QUUX_KIND_ERROR) &&
           quux_name_equals(quux_error_field(error, QUUX_ERROR_KIND), kind_name);
}

/* ---- Cancellation (section 07 rules 10-15) ---- */

bool quux_concurrency_cancellation_observed(quux_context *context) {
    quux_task *task = context->task;
    if (task == NULL || task->shield_depth > 0) {
        return false;
    }
    if (quux_atomic_load_acquire(&task->cancellation_requested)) {
        return true;
    }
    /* The body of a scope that is cancelling observes it (rule 10). A cancelled scope cancels
     * its child scopes at once, so the current scope is enough. */
    return quux_is_kind(context->current_scope, QUUX_KIND_SCOPE) &&
           scope_state(context->current_scope) == QUUX_SCOPE_CANCELLING;
}

bool quux_concurrency_raise_cancelled(quux_context *context) {
    return quux_raise(context, "cancelled", "the task was cancelled");
}

static void cancel_task_locked(quux_context *context, quux_task *task) {
    quux_executor *executor = executor_of(context);
    executor->operations->cancel(executor, task);
}

void quux_concurrency_wake_locked(quux_context *context, quux_task *task) {
    quux_executor *executor = executor_of(context);
    executor->operations->wake(executor, task);
}

/* Moves a running scope to :cancelling and cancels its children and its body (rules 5, 10). */
static void cancel_scope_locked(quux_context *context, quux_value scope, bool explicitly) {
    quux_scope_record *record = quux_scope_record_of(scope);
    if (scope_state(scope) != QUUX_SCOPE_RUNNING) {
        return;
    }
    set_scope_state(scope, QUUX_SCOPE_CANCELLING);
    if (explicitly) {
        record->cancelled = true;
    }
    for (quux_value children = quux_checked_object(scope)->values[QUUX_SCOPE_CHILDREN];
         !quux_is_nothing(children); children = quux_pair_rest(children)) {
        quux_value child = quux_pair_first(children);
        if (quux_is_kind(child, QUUX_KIND_SCOPE)) {
            cancel_scope_locked(context, child, true);
        } else {
            quux_future_record *future = quux_future_record_of(child);
            if (quux_atomic_load_acquire(&future->state) == QUUX_FUTURE_EMPTY &&
                future->computing_task != NULL) {
                cancel_task_locked(context, future->computing_task);
            }
        }
    }
    if (record->body_task != NULL) {
        quux_concurrency_wake_locked(context, record->body_task);
    }
}

void quux_concurrency_cancel_unneeded_locked(quux_context *context, quux_value future) {
    quux_future_record *record = quux_future_record_of(future);
    if (quux_atomic_load_acquire(&record->state) != QUUX_FUTURE_EMPTY) {
        return;
    }
    record->cancelled_by_combinator = true;
    if (record->computing_task != NULL) {
        cancel_task_locked(context, record->computing_task);
    }
}

void quux_concurrency_handle_failures(quux_context *context, quux_value futures) {
    uint64_t count = 0;
    quux_count(context, futures, &count);
    quux_concurrency_lock(context);
    for (uint64_t i = 0; i < count; i++) {
        quux_future_record_of(quux_vector_element(futures, i))->handled_by_combinator = true;
    }
    quux_concurrency_unlock(context);
}

/* ---- Failures (rules 5, 7, 8) ---- */

static bool is_recorded(quux_value scope, quux_value error) {
    quux_object *object = quux_checked_object(scope);
    if (object->values[QUUX_SCOPE_PRIMARY_ERROR] == error) {
        return true;
    }
    for (quux_value list = object->values[QUUX_SCOPE_SUPPRESSED]; !quux_is_nothing(list);
         list = quux_pair_rest(list)) {
        if (quux_pair_first(list) == error) {
            return true;
        }
    }
    return false;
}

/* Records a failure of a child (or of the body) in completion order. Lock held. */
static void record_failure_locked(quux_context *context, quux_value scope, quux_value error) {
    quux_value list = quux_nothing;
    QUUX_ROOT_FRAME(context, &scope, &error, &list);
    quux_object *object = quux_checked_object(scope);
    if (!is_recorded(scope, error)) {
        if (quux_is_nothing(object->values[QUUX_SCOPE_PRIMARY_ERROR])) {
            quux_store(context, object, QUUX_SCOPE_PRIMARY_ERROR, error);
            cancel_scope_locked(context, scope, false);
        } else if (quux_list_prepend(context, error, object->values[QUUX_SCOPE_SUPPRESSED],
                                     &list)) {
            quux_store(context, quux_checked_object(scope), QUUX_SCOPE_SUPPRESSED, list);
            quux_atomic_store_release(&quux_concurrency_of(context)->primary_error_raced, true);
        } else {
            context->pending_error = quux_nothing; /* out of memory: the error is lost */
        }
    }
    QUUX_ROOT_FRAME_END(context);
}

/* ---- Settling a future (rule 5: the first transition wins) ---- */

static bool settle_locked(quux_context *context, quux_value future, bool filled,
                          quux_value value_or_error) {
    quux_future_record *record = quux_future_record_of(future);
    if (quux_atomic_load_acquire(&record->state) != QUUX_FUTURE_EMPTY) {
        return false;
    }
    quux_object *object = quux_checked_object(future);
    quux_store(context, object, filled ? QUUX_FUTURE_VALUE : QUUX_FUTURE_ERROR, value_or_error);
    quux_atomic_store_release(&record->state, filled ? QUUX_FUTURE_FILLED : QUUX_FUTURE_FAILED);
    for (uint32_t i = 0; i < record->waiter_count; i++) {
        quux_concurrency_wake_locked(context, record->waiters[i]);
    }
    if (!filled) {
        quux_value scope = object->values[QUUX_FUTURE_SCOPE];
        bool dropped =
            record->handled_by_combinator ||
            (error_has_kind(value_or_error, "cancelled") &&
             (record->cancelled_by_combinator || scope_state(scope) != QUUX_SCOPE_RUNNING));
        if (!dropped) {
            record_failure_locked(context, scope, value_or_error);
        }
    }
    return true;
}

/* ---- Tasks ---- */

static void run_task(quux_task *task) {
    quux_context *context = task->context;
    quux_value value = quux_nothing;
    quux_value future = context->task_future;
    quux_value scope = quux_nothing;
    QUUX_ROOT_FRAME(context, &value, &future, &scope);
    bool filled = quux_apply(context, context->task_function, context->task_arguments, &value);
    if (!filled) {
        value = context->pending_error;
        context->pending_error = quux_nothing;
    }
    scope = quux_checked_object(future)->values[QUUX_FUTURE_SCOPE];
    quux_concurrency_lock(context);
    settle_locked(context, future, filled, value);
    quux_future_record_of(future)->computing_task = NULL;
    quux_scope_record *scope_record = quux_scope_record_of(scope);
    scope_record->running_task_count--;
    if (scope_record->running_task_count == 0 && scope_record->body_task != NULL) {
        quux_concurrency_wake_locked(context, scope_record->body_task);
    }
    quux_concurrency_unlock(context);
    QUUX_ROOT_FRAME_END(context);
    /* From here on the task touches no heap object: the verifier may run in other tasks. */
    context->task_function = quux_nothing;
    context->task_arguments = quux_nothing;
    context->task_future = quux_nothing;
    context->current_scope = quux_nothing;
    quux_context_finish(context);
}

static bool make_future(quux_context *context, quux_value scope, quux_value *result) {
    QUUX_ROOT_FRAME(context, &scope);
    quux_object *object = quux_allocate(context, QUUX_KIND_FUTURE, QUUX_FUTURE_VALUE_COUNT,
                                        sizeof(quux_future_record));
    QUUX_ROOT_FRAME_END(context);
    if (object == NULL) {
        return false;
    }
    object->flags |= QUUX_OBJECT_FLAG_HAS_IDENTITY;
    object->values[QUUX_FUTURE_SCOPE] = scope;
    quux_future_record *record = (quux_future_record *)(void *)quux_object_bytes(object);
    quux_atomic_store_relaxed(&record->state, QUUX_FUTURE_EMPTY);
    record->identifier =
        quux_atomic_add_relaxed(&quux_concurrency_of(context)->next_future_identifier, 1) + 1;
    *result = quux_object_value(object);
    return true;
}

/* Adds a future or a child scope to the scope's children. Lock held. */
static bool add_child_locked(quux_context *context, quux_value scope, quux_value child) {
    quux_value list = quux_nothing;
    QUUX_ROOT_FRAME(context, &scope, &child, &list);
    bool done = quux_list_prepend(context, child,
                                  quux_checked_object(scope)->values[QUUX_SCOPE_CHILDREN], &list);
    if (done) {
        quux_store(context, quux_checked_object(scope), QUUX_SCOPE_CHILDREN, list);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool is_live_scope_of(quux_value scope, quux_value current) {
    for (quux_value candidate = current; quux_is_kind(candidate, QUUX_KIND_SCOPE);
         candidate = quux_checked_object(candidate)->values[QUUX_SCOPE_PARENT]) {
        if (candidate == scope) {
            return scope_state(scope) == QUUX_SCOPE_RUNNING ||
                   scope_state(scope) == QUUX_SCOPE_CANCELLING;
        }
    }
    return false;
}

bool quux_concurrency_spawn(quux_context *context, quux_value scope, quux_value function,
                            quux_value arguments, quux_value *result) {
    quux_value future = quux_nothing;
    QUUX_ROOT_FRAME(context, &scope, &function, &arguments, &future);
    bool done = make_future(context, scope, &future);
    spawned_task *spawned = done ? calloc(1, sizeof *spawned) : NULL;
    if (done && spawned == NULL) {
        done = quux_raise_value(context, context->runtime->resource_exhausted_error);
    }
    if (done) {
        quux_task *task = &spawned->task;
        quux_context_initialize(&spawned->context, context->runtime);
        spawned->context.task = task;
        spawned->context.current_scope = scope;
        spawned->context.current_namespace = context->current_namespace;
        spawned->context.current_node = context->current_node;
        spawned->context.task_function = function;
        spawned->context.task_arguments = arguments;
        spawned->context.task_future = future;
        task->context = &spawned->context;

        quux_concurrency_lock(context);
        quux_scope_record *record = quux_scope_record_of(scope);
        quux_scope_state state = scope_state(scope);
        done = (state == QUUX_SCOPE_RUNNING || state == QUUX_SCOPE_CANCELLING) &&
               grow((void **)&record->tasks, &record->task_capacity, sizeof(quux_task *),
                    record->task_count + 1) &&
               add_child_locked(context, scope, future);
        if (done) {
            if (state == QUUX_SCOPE_CANCELLING) {
                /* a future created in a cancelling scope starts cancelled */
                quux_atomic_store_release(&task->cancellation_requested, true);
            }
            quux_future_record_of(future)->computing_task = task;
            record->running_task_count++;
            quux_executor *executor = executor_of(context);
            if (executor->operations->start_task(executor, task, run_task)) {
                record->tasks[record->task_count++] = task;
            } else {
                /* :resource-exhausted fails the future instead of the process (D-40) */
                record->running_task_count--;
                quux_future_record_of(future)->computing_task = NULL;
                settle_locked(context, future, false, context->runtime->resource_exhausted_error);
                quux_context_finish(&spawned->context);
                free(spawned);
            }
        } else {
            if (state != QUUX_SCOPE_RUNNING && state != QUUX_SCOPE_CANCELLING) {
                (void)quux_raise(context, "wrong-type", "the scope has already ended");
            }
            quux_context_finish(&spawned->context);
            free(spawned);
        }
        quux_concurrency_unlock(context);
    }
    if (done) {
        *result = future;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- Waiting (section 06 rules 8-10, section 07 rule 16) ---- */

static bool add_waiter_locked(quux_value future, quux_task *task) {
    quux_future_record *record = quux_future_record_of(future);
    size_t capacity = record->waiter_capacity;
    if (!grow((void **)&record->waiters, &capacity, sizeof(quux_task *),
              (size_t)record->waiter_count + 1)) {
        return false;
    }
    record->waiter_capacity = (uint32_t)capacity;
    record->waiters[record->waiter_count++] = task;
    return true;
}

static void remove_waiter_locked(quux_value future, quux_task *task) {
    quux_future_record *record = quux_future_record_of(future);
    for (uint32_t i = 0; i < record->waiter_count; i++) {
        if (record->waiters[i] == task) {
            record->waiters[i] = record->waiters[--record->waiter_count];
            break;
        }
    }
    if (record->waiter_count == 0) {
        free(record->waiters);
        record->waiters = NULL;
        record->waiter_capacity = 0;
    }
}

/* If waiting for `future` would close a cycle, fails every future of the cycle with :deadlock,
 * whose :data has :cycle, the identifiers of those futures (D-14). Lock held. */
static bool fail_cycle_locked(quux_context *context, quux_value future) {
    quux_task *current = context->task;
    quux_object **path = NULL;
    size_t path_count = 0;
    size_t path_capacity = 0;
    bool cycle = false;
    quux_object *candidate = quux_checked_object(future);
    while (candidate != NULL) {
        if (!grow((void **)&path, &path_capacity, sizeof *path, path_count + 1)) {
            break;
        }
        path[path_count++] = candidate;
        quux_future_record *record = (quux_future_record *)(void *)quux_object_bytes(candidate);
        quux_task *computing = record->computing_task;
        if (computing == NULL || quux_atomic_load_acquire(&record->state) != QUUX_FUTURE_EMPTY) {
            break;
        }
        if (computing == current) {
            cycle = true;
            break;
        }
        candidate = computing->waiting_for;
    }
    if (cycle) {
        quux_value identifiers = quux_nothing;
        quux_value data = quux_nothing;
        quux_value error = quux_nothing;
        quux_value message = quux_nothing;
        quux_value number = quux_nothing;
        QUUX_ROOT_FRAME(context, &identifiers, &data, &error, &message, &number);
        bool done = quux_vector_empty(context, &identifiers);
        for (size_t i = 0; done && i < path_count; i++) {
            quux_future_record *record = (quux_future_record *)(void *)quux_object_bytes(path[i]);
            done = quux_make_integer(context, (int64_t)record->identifier, &number) &&
                   quux_vector_add(context, identifiers, number, &identifiers);
        }
        char text[96];
        snprintf(text, sizeof text, "waiting would close a cycle of %zu futures", path_count);
        done = done && quux_map_empty(context, &data) &&
               quux_map_put(context, data, quux_keyword(context, "cycle"), identifiers, &data) &&
               quux_make_string_from_c(context, text, &message) &&
               quux_make_error(context, quux_keyword(context, "deadlock"), message, data, &error);
        if (!done) {
            error = context->runtime->resource_exhausted_error;
            context->pending_error = quux_nothing;
        }
        for (size_t i = 0; i < path_count; i++) {
            settle_locked(context, quux_object_value(path[i]), false, error);
        }
        QUUX_ROOT_FRAME_END(context);
    }
    free(path);
    return cycle;
}

typedef struct wait_condition {
    quux_context *context;
    quux_value future;
} wait_condition;

static bool future_settled_or_cancelled(void *data) {
    wait_condition *condition = data;
    return quux_atomic_load_acquire(&quux_future_record_of(condition->future)->state) !=
               QUUX_FUTURE_EMPTY ||
           quux_concurrency_cancellation_observed(condition->context);
}

static bool result_of_settled(quux_context *context, quux_value future, quux_value *result) {
    quux_object *object = quux_checked_object(future);
    if (quux_atomic_load_acquire(&quux_future_record_of(future)->state) == QUUX_FUTURE_FILLED) {
        *result = object->values[QUUX_FUTURE_VALUE];
        return true;
    }
    return quux_raise_value(context, object->values[QUUX_FUTURE_ERROR]);
}

bool quux_concurrency_wait_for(quux_context *context, quux_value future, quux_value *result) {
    if (!quux_is_kind(future, QUUX_KIND_FUTURE)) {
        return quux_raise(context, "wrong-type", "wait-for expects a future, got %s",
                          quux_kind_phrase(quux_kind_of(future)));
    }
    if (quux_atomic_load_acquire(&quux_future_record_of(future)->state) != QUUX_FUTURE_EMPTY) {
        return result_of_settled(context, future, result);
    }
    if (quux_concurrency_cancellation_observed(context)) {
        return quux_concurrency_raise_cancelled(context);
    }
    quux_task *task = context->task;
    QUUX_ROOT_FRAME(context, &future);
    quux_concurrency_lock(context);
    bool cancelled = false;
    if (quux_atomic_load_acquire(&quux_future_record_of(future)->state) == QUUX_FUTURE_EMPTY &&
        !fail_cycle_locked(context, future)) {
        if (add_waiter_locked(future, task)) {
            task->waiting_for = quux_checked_object(future);
            wait_condition condition = {context, future};
            quux_executor *executor = executor_of(context);
            quux_park_outcome outcome =
                executor->operations->park(executor, task, future_settled_or_cancelled, &condition,
                                           QUUX_NO_DEADLINE, task->shield_depth == 0);
            task->waiting_for = NULL;
            remove_waiter_locked(future, task);
            cancelled = outcome == QUUX_PARK_CANCELLED ||
                        (quux_atomic_load_acquire(&quux_future_record_of(future)->state) ==
                             QUUX_FUTURE_EMPTY &&
                         quux_concurrency_cancellation_observed(context));
        } else {
            quux_concurrency_unlock(context);
            QUUX_ROOT_FRAME_END(context);
            return quux_raise_value(context, context->runtime->resource_exhausted_error);
        }
    }
    quux_concurrency_unlock(context);
    QUUX_ROOT_FRAME_END(context);
    if (cancelled) {
        return quux_concurrency_raise_cancelled(context);
    }
    return result_of_settled(context, future, result);
}

typedef struct any_condition {
    quux_context *context;
    quux_object *futures;
    uint32_t count;
} any_condition;

static bool any_settled_or_cancelled(void *data) {
    any_condition *condition = data;
    if (quux_concurrency_cancellation_observed(condition->context)) {
        return true;
    }
    for (uint32_t i = 0; i < condition->count; i++) {
        quux_value future = condition->futures->values[i];
        if (quux_atomic_load_acquire(&quux_future_record_of(future)->state) != QUUX_FUTURE_EMPTY) {
            return true;
        }
    }
    return false;
}

quux_park_outcome quux_concurrency_wait_for_any(quux_context *context, quux_value futures,
                                                uint64_t deadline_nanoseconds) {
    quux_task *task = context->task;
    quux_value holder = quux_nothing;
    QUUX_ROOT_FRAME(context, &futures, &holder);
    uint64_t count = 0;
    quux_count(context, futures, &count);
    quux_object *object = quux_allocate(context, QUUX_KIND_ARGUMENTS, (uint32_t)count, 0);
    if (object == NULL) {
        QUUX_ROOT_FRAME_END(context);
        context->pending_error = quux_nothing;
        return QUUX_PARK_CANCELLED;
    }
    holder = quux_object_value(object);
    for (uint64_t i = 0; i < count; i++) {
        object->values[i] = quux_vector_element(futures, i);
    }
    quux_concurrency_lock(context);
    any_condition condition = {context, object, (uint32_t)count};
    uint32_t registered = 0;
    while (registered < count && add_waiter_locked(object->values[registered], task)) {
        registered++;
    }
    quux_executor *executor = executor_of(context);
    quux_park_outcome outcome =
        executor->operations->park(executor, task, any_settled_or_cancelled, &condition,
                                   deadline_nanoseconds, task->shield_depth == 0);
    for (uint32_t i = 0; i < registered; i++) {
        remove_waiter_locked(object->values[i], task);
    }
    if (outcome == QUUX_PARK_SATISFIED && quux_concurrency_cancellation_observed(context)) {
        bool any_settled = false;
        for (uint32_t i = 0; i < condition.count; i++) {
            any_settled = any_settled || quux_atomic_load_acquire(
                                             &quux_future_record_of(object->values[i])->state) !=
                                             QUUX_FUTURE_EMPTY;
        }
        if (!any_settled) {
            outcome = QUUX_PARK_CANCELLED;
        }
    }
    quux_concurrency_unlock(context);
    QUUX_ROOT_FRAME_END(context);
    return outcome;
}

void quux_concurrency_park_locked(quux_context *context, quux_park_condition condition,
                                  void *data) {
    quux_executor *executor = executor_of(context);
    executor->operations->park(executor, context->task, condition, data, QUUX_NO_DEADLINE, false);
}

/* ---- Sleeping ---- */

static bool cancellation_observed_condition(void *data) {
    return quux_concurrency_cancellation_observed(data);
}

bool quux_concurrency_sleep(quux_context *context, uint64_t milliseconds) {
    if (quux_concurrency_cancellation_observed(context)) {
        return quux_concurrency_raise_cancelled(context);
    }
    quux_executor *executor = executor_of(context);
    uint64_t deadline = executor->operations->current_time(executor) + milliseconds * 1000000u;
    if (deadline == QUUX_NO_DEADLINE) {
        deadline = 1;
    }
    quux_concurrency_lock(context);
    quux_park_outcome outcome =
        executor->operations->park(executor, context->task, cancellation_observed_condition,
                                   context, deadline, context->task->shield_depth == 0);
    bool cancelled =
        outcome != QUUX_PARK_DEADLINE && quux_concurrency_cancellation_observed(context);
    quux_concurrency_unlock(context);
    if (cancelled) {
        return quux_concurrency_raise_cancelled(context);
    }
    return true;
}

/* ---- Scopes (section 07 rules 1-9) ---- */

static void next_identifier_locked(quux_concurrency_state *state, unsigned char identifier[16]) {
    if (state->seeded) {
        for (int half = 0; half < 2; half++) {
            /* splitmix64 */
            uint64_t z = (state->random_state += UINT64_C(0x9E3779B97F4A7C15));
            z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
            z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
            z ^= z >> 31;
            memcpy(identifier + 8 * half, &z, sizeof z);
        }
    } else if (!quux_random_bytes(identifier, 16)) {
        memset(identifier, 0, 16);
    }
}

static bool make_scope(quux_context *context, quux_value parent, quux_value *result) {
    QUUX_ROOT_FRAME(context, &parent);
    quux_object *object =
        quux_allocate(context, QUUX_KIND_SCOPE, QUUX_SCOPE_VALUE_COUNT, sizeof(quux_scope_record));
    QUUX_ROOT_FRAME_END(context);
    if (object == NULL) {
        return false;
    }
    object->flags |= QUUX_OBJECT_FLAG_HAS_IDENTITY;
    object->values[QUUX_SCOPE_PARENT] = parent;
    quux_scope_record *record = (quux_scope_record *)(void *)quux_object_bytes(object);
    quux_atomic_store_relaxed(&record->state, QUUX_SCOPE_RUNNING);
    record->body_task = context->task;
    quux_concurrency_lock(context);
    next_identifier_locked(quux_concurrency_of(context), record->identifier);
    quux_concurrency_unlock(context);
    *result = quux_object_value(object);
    return true;
}

typedef struct join_condition {
    quux_context *context;
    quux_value scope;
} join_condition;

static bool join_ready(void *data) {
    join_condition *condition = data;
    quux_task *task = condition->context->task;
    return quux_scope_record_of(condition->scope)->running_task_count == 0 ||
           (task->shield_depth == 0 && quux_atomic_load_acquire(&task->cancellation_requested) &&
            scope_state(condition->scope) == QUUX_SCOPE_RUNNING);
}

/* Waits until every task of the scope has ended, then releases them. Lock held on entry and
 * on exit. When the joining task itself is cancelled meanwhile, the scope is cancelled. */
static void join_scope_locked(quux_context *context, quux_value scope) {
    quux_scope_record *record = quux_scope_record_of(scope);
    join_condition condition = {context, scope};
    while (record->running_task_count > 0) {
        if (join_ready(&condition) && record->running_task_count > 0) {
            cancel_scope_locked(context, scope, true);
            continue;
        }
        quux_concurrency_park_locked(context, join_ready, &condition);
    }
    quux_task **tasks = record->tasks;
    size_t task_count = record->task_count;
    record->tasks = NULL;
    record->task_count = 0;
    record->task_capacity = 0;
    record->body_task = NULL;
    quux_concurrency_unlock(context);
    quux_executor *executor = executor_of(context);
    for (size_t i = 0; i < task_count; i++) {
        executor->operations->join_task(executor, tasks[i]);
        free(tasks[i]); /* the spawned_task that starts with this task */
    }
    free(tasks);
    quux_concurrency_lock(context);
}

/* The primary error with the suppressed errors attached, in completion order (rule 7). */
static bool primary_with_suppressed(quux_context *context, quux_value scope, quux_value *result) {
    quux_value primary = quux_checked_object(scope)->values[QUUX_SCOPE_PRIMARY_ERROR];
    quux_value suppressed = quux_checked_object(scope)->values[QUUX_SCOPE_SUPPRESSED];
    if (quux_is_nothing(suppressed)) {
        *result = primary;
        return true;
    }
    quux_value vector = quux_nothing;
    quux_value copy = quux_nothing;
    QUUX_ROOT_FRAME(context, &primary, &suppressed, &vector, &copy);
    bool done =
        quux_list_reverse(context, suppressed, &suppressed) && quux_vector_empty(context, &vector);
    for (quux_value list = suppressed; done && !quux_is_nothing(list);
         list = quux_pair_rest(list)) {
        done = quux_vector_add(context, vector, quux_pair_first(list), &vector);
    }
    quux_object *object =
        done ? quux_allocate(context, QUUX_KIND_ERROR, QUUX_ERROR_FIELD_COUNT, 0) : NULL;
    done = done && object != NULL;
    if (done) {
        for (int i = 0; i < QUUX_ERROR_FIELD_COUNT; i++) {
            object->values[i] = quux_error_field(primary, i);
        }
        object->values[QUUX_ERROR_SUPPRESSED] = vector;
        *result = quux_object_value(object);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_concurrency_with_scope(quux_context *context, quux_scope_body body, void *data,
                                 quux_value *result) {
    quux_value parent = context->current_scope;
    quux_value scope = quux_nothing;
    quux_value value = quux_nothing;
    quux_value error = quux_nothing;
    QUUX_ROOT_FRAME(context, &parent, &scope, &value, &error);
    bool done = make_scope(context, parent, &scope);
    if (!done) {
        QUUX_ROOT_FRAME_END(context);
        return false;
    }
    quux_concurrency_lock(context);
    if (quux_is_kind(parent, QUUX_KIND_SCOPE)) {
        if (scope_state(parent) != QUUX_SCOPE_RUNNING) {
            /* a scope opened inside a cancelling scope starts cancelled */
            set_scope_state(scope, QUUX_SCOPE_CANCELLING);
            quux_scope_record_of(scope)->cancelled = true;
        }
        if (!add_child_locked(context, parent, scope)) {
            quux_concurrency_unlock(context);
            QUUX_ROOT_FRAME_END(context);
            return false;
        }
    }
    quux_concurrency_unlock(context);

    context->current_scope = scope;
    bool body_done = body(context, scope, data, &value);
    context->current_scope = parent;

    quux_concurrency_lock(context);
    quux_task *task = context->task;
    if (!body_done) {
        error = context->pending_error;
        context->pending_error = quux_nothing;
        bool task_cancelled =
            task != NULL && quux_atomic_load_acquire(&task->cancellation_requested);
        if (error_has_kind(error, "cancelled") &&
            (scope_state(scope) != QUUX_SCOPE_RUNNING || task_cancelled)) {
            /* the scope's own cancellation, or the cancellation of the task that runs it:
             * dropped, and the scope is cancelled (rule 8) */
            cancel_scope_locked(context, scope, true);
        } else {
            record_failure_locked(context, scope, error);
        }
    }
    join_scope_locked(context, scope);
    quux_object *object = quux_checked_object(scope);
    bool failed = !quux_is_nothing(object->values[QUUX_SCOPE_PRIMARY_ERROR]) ||
                  quux_scope_record_of(scope)->cancelled;
    set_scope_state(scope, failed ? QUUX_SCOPE_JOINED_FAILED : QUUX_SCOPE_JOINED_SUCCESSFULLY);
    quux_concurrency_unlock(context);

    if (!quux_is_nothing(object->values[QUUX_SCOPE_PRIMARY_ERROR])) {
        done = primary_with_suppressed(context, scope, &error) && quux_raise_value(context, error);
    } else if (failed) {
        done = quux_raise(context, "cancelled", "the scope was cancelled");
    } else {
        *result = value;
        done = true;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- Primitives and form functions ---- */

static bool call_with_scope(quux_context *context, quux_value scope, void *data,
                            quux_value *result) {
    quux_value *function = data;
    return quux_call1(context, *function, scope, result);
}

static bool primitive_with_scope(quux_context *context, quux_value *arguments, uint32_t count,
                                 quux_value *result) {
    (void)count;
    quux_value function = arguments[0];
    QUUX_ROOT_FRAME(context, &function);
    bool done = quux_concurrency_with_scope(context, call_with_scope, &function, result);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool primitive_spawn(quux_context *context, quux_value *arguments, uint32_t count,
                            quux_value *result) {
    quux_value scope = count == 2 ? arguments[0] : context->current_scope;
    quux_value function = arguments[count - 1];
    quux_value empty = quux_nothing;
    if (count == 2 && !quux_is_kind(scope, QUUX_KIND_SCOPE)) {
        return quux_raise(context, "wrong-type", "spawn expects a scope, got %s",
                          quux_kind_phrase(quux_kind_of(scope)));
    }
    if (count == 2 && !is_live_scope_of(scope, context->current_scope)) {
        return quux_raise(context, "wrong-type",
                          "spawn expects the current scope or one of its live ancestors");
    }
    QUUX_ROOT_FRAME(context, &scope, &function, &empty);
    bool done = quux_make_arguments(context, 0, &empty) &&
                quux_concurrency_spawn(context, scope, function, empty, result);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool primitive_shield(quux_context *context, quux_value *arguments, uint32_t count,
                             quux_value *result) {
    (void)count;
    quux_value empty = quux_nothing;
    quux_value function = arguments[0];
    QUUX_ROOT_FRAME(context, &empty, &function);
    bool done = quux_make_arguments(context, 0, &empty);
    if (done) {
        if (context->task != NULL) {
            context->task->shield_depth++;
        }
        done = quux_apply(context, function, empty, result);
        if (context->task != NULL) {
            context->task->shield_depth--;
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* Without the harness, (tag name expression) is expression. */
static bool primitive_tag(quux_context *context, quux_value *arguments, uint32_t count,
                          quux_value *result) {
    (void)count;
    quux_value empty = quux_nothing;
    quux_value function = arguments[1];
    QUUX_ROOT_FRAME(context, &empty, &function);
    bool done =
        quux_make_arguments(context, 0, &empty) && quux_apply(context, function, empty, result);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool primitive_wait_for(quux_context *context, quux_value *arguments, uint32_t count,
                               quux_value *result) {
    (void)count;
    return quux_concurrency_wait_for(context, arguments[0], result);
}

static bool primitive_state(quux_context *context, quux_value *arguments, uint32_t count,
                            quux_value *result) {
    (void)count;
    quux_value subject = arguments[0];
    if (quux_is_kind(subject, QUUX_KIND_FUTURE)) {
        static const char *const names[] = {"empty", "filled", "failed"};
        *result = quux_keyword(context, names[quux_future_current_state(subject)]);
        return true;
    }
    if (quux_is_kind(subject, QUUX_KIND_AGENT)) {
        *result = quux_load_shared(quux_checked_object(subject), 0);
        return true;
    }
    return quux_raise(context, "wrong-type", "state expects a future or an agent, got %s",
                      quux_kind_phrase(quux_kind_of(subject)));
}

static bool primitive_sleep(quux_context *context, quux_value *arguments, uint32_t count,
                            quux_value *result) {
    (void)count;
    if (!quux_is_kind(arguments[0], QUUX_KIND_INTEGER) || quux_integer_value(arguments[0]) < 0) {
        return quux_raise(context, "wrong-type",
                          "sleep expects a non-negative integer of milliseconds, got %s",
                          quux_kind_phrase(quux_kind_of(arguments[0])));
    }
    if (!quux_concurrency_sleep(context, (uint64_t)quux_integer_value(arguments[0]))) {
        return false;
    }
    *result = quux_nothing;
    return true;
}

static bool primitive_cancel(quux_context *context, quux_value *arguments, uint32_t count,
                             quux_value *result) {
    (void)count;
    if (!quux_is_kind(arguments[0], QUUX_KIND_SCOPE)) {
        return quux_raise(context, "wrong-type", "cancel! expects a scope, got %s",
                          quux_kind_phrase(quux_kind_of(arguments[0])));
    }
    quux_concurrency_lock(context);
    cancel_scope_locked(context, arguments[0], true);
    quux_concurrency_unlock(context);
    *result = quux_nothing;
    return true;
}

static bool primitive_check_cancellation(quux_context *context, quux_value *arguments,
                                         uint32_t count, quux_value *result) {
    (void)arguments;
    (void)count;
    if (quux_concurrency_cancellation_observed(context)) {
        return quux_concurrency_raise_cancelled(context);
    }
    *result = quux_nothing;
    return true;
}

static bool install_form_function(quux_context *context, quux_form_function which, const char *name,
                                  quux_primitive_function function, uint32_t minimum,
                                  uint32_t maximum) {
    quux_value primitive = quux_nothing;
    QUUX_ROOT_FRAME(context, &primitive);
    bool done = quux_make_primitive(context, name, function, minimum, maximum, &primitive);
    if (done) {
        context->runtime->form_functions[which] = primitive;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static const struct {
    const char *name;
    quux_primitive_function function;
    uint32_t minimum;
    uint32_t maximum;
} future_primitives[] = {
    {"wait-for", primitive_wait_for, 1, 1},
    {"state", primitive_state, 1, 1},
    {"sleep", primitive_sleep, 1, 1},
    {"cancel!", primitive_cancel, 1, 1},
    {"check-cancellation", primitive_check_cancellation, 0, 0},
};

bool quux_concurrency_initialize(quux_context *context, quux_executor *executor) {
    quux_runtime *runtime = context->runtime;
    quux_concurrency_state *state = calloc(1, sizeof *state);
    if (state == NULL) {
        return quux_raise_value(context, runtime->resource_exhausted_error);
    }
    state->executor = executor;
    state->root_scope = quux_nothing;
    quux_memory_register_global_root(runtime->heap, &state->root_scope);
    for (int i = 0; i < QUUX_HELPER_COUNT; i++) {
        state->helpers[i] = quux_nothing;
        quux_memory_register_global_root(runtime->heap, &state->helpers[i]);
    }
    runtime->concurrency = state;
    state->session_task.context = context;
    if (!executor->operations->attach_current_thread(executor, &state->session_task)) {
        return quux_raise_value(context, runtime->resource_exhausted_error);
    }
    context->task = &state->session_task;
    bool done = make_scope(context, quux_nothing, &state->root_scope);
    if (done) {
        context->current_scope = state->root_scope;
    }
    for (size_t i = 0; done && i < sizeof future_primitives / sizeof future_primitives[0]; i++) {
        done =
            quux_define_primitive(context, future_primitives[i].name, future_primitives[i].function,
                                  future_primitives[i].minimum, future_primitives[i].maximum);
    }
    done = done &&
           install_form_function(context, QUUX_FORM_SPAWN, "future", primitive_spawn, 1, 2) &&
           install_form_function(context, QUUX_FORM_WITH_SCOPE, "with-scope", primitive_with_scope,
                                 1, 1) &&
           install_form_function(context, QUUX_FORM_SHIELD, "shield-from-cancellation",
                                 primitive_shield, 1, 1) &&
           install_form_function(context, QUUX_FORM_TAG, "tag", primitive_tag, 2, 2) &&
           quux_combinators_register(context) && quux_parallelism_register(context) &&
           quux_agents_register(context);
    return done;
}

bool quux_concurrency_finish(quux_context *context) {
    quux_concurrency_state *state = quux_concurrency_of(context);
    if (state == NULL) {
        return true;
    }
    bool done = true;
    quux_value error = quux_nothing;
    QUUX_ROOT_FRAME(context, &error);
    if (quux_is_kind(state->root_scope, QUUX_KIND_SCOPE)) {
        quux_concurrency_lock(context);
        join_scope_locked(context, state->root_scope);
        set_scope_state(state->root_scope, QUUX_SCOPE_JOINED_SUCCESSFULLY);
        quux_concurrency_unlock(context);
        if (!quux_is_nothing(
                quux_checked_object(state->root_scope)->values[QUUX_SCOPE_PRIMARY_ERROR])) {
            done = primary_with_suppressed(context, state->root_scope, &error) &&
                   quux_raise_value(context, error);
        }
    }
    QUUX_ROOT_FRAME_END(context);
    state->executor->operations->detach_current_thread(state->executor, &state->session_task);
    state->executor->operations->destroy(state->executor);
    context->task = NULL;
    context->current_scope = quux_nothing;
    state->root_scope = quux_nothing;
    for (int i = 0; i < QUUX_HELPER_COUNT; i++) {
        state->helpers[i] = quux_nothing;
    }
    for (int i = 0; i < QUUX_FORM_FUNCTION_COUNT; i++) {
        context->runtime->form_functions[i] = quux_nothing;
    }
    /* the state block stays allocated (its root slots are registered with the heap) and is
     * released by quux_runtime_destroy */
    state->executor = NULL;
    return done;
}

/* ---- Observation ---- */

quux_future_state quux_future_current_state(quux_value future) {
    return (quux_future_state)quux_atomic_load_acquire(&quux_future_record_of(future)->state);
}

quux_value quux_future_error(quux_value future) {
    if (quux_future_current_state(future) != QUUX_FUTURE_FAILED) {
        return quux_nothing;
    }
    return quux_checked_object(future)->values[QUUX_FUTURE_ERROR];
}

bool quux_concurrency_primary_error_raced(quux_runtime *runtime) {
    return runtime->concurrency != NULL &&
           quux_atomic_load_acquire(&runtime->concurrency->primary_error_raced);
}

void quux_concurrency_set_seed(quux_runtime *runtime, uint64_t seed) {
    if (runtime->concurrency != NULL) {
        runtime->concurrency->random_state = seed;
        runtime->concurrency->seeded = true;
    }
}
