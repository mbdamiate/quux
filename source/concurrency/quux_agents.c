/* Agents (section 09), the part that reference program 09 needs: `agent`, `state`, `update` and
 * `transact`. Updates run one at a time, in the order they were sent: each send takes a ticket,
 * and the future of the send waits for its turn. A send whose task is cancelled while it waits
 * is withdrawn when its turn comes, without being applied (D-16), unless it was sent from
 * shield-from-cancellation: cleanup code must be able to finish (section 07 rule 14).
 *
 * Task T051 replaces this with D-41 (a queue drained by one worker per agent, the agent's own
 * scope for update functions, update-once, and the deadlock of section 07 rule 18). */
#include "quux_concurrency_internal.h"

#include <stdlib.h>

enum { QUUX_AGENT_STATE, QUUX_AGENT_VALUE_COUNT };

typedef struct agent_record {
    uint64_t next_ticket;    /* under the lock */
    uint64_t serving_ticket; /* under the lock */
    quux_task **waiters;
    uint32_t waiter_count;
    uint32_t waiter_capacity;
} agent_record;

static agent_record *agent_record_of(quux_value agent) {
    return (agent_record *)(void *)quux_object_bytes(quux_checked_object(agent));
}

static bool primitive_agent(quux_context *context, quux_value *arguments, uint32_t count,
                            quux_value *result) {
    (void)count;
    quux_object *object =
        quux_allocate(context, QUUX_KIND_AGENT, QUUX_AGENT_VALUE_COUNT, sizeof(agent_record));
    if (object == NULL) {
        return false;
    }
    object->flags |= QUUX_OBJECT_FLAG_HAS_IDENTITY;
    object->values[QUUX_AGENT_STATE] = arguments[0];
    *result = quux_object_value(object);
    return true;
}

typedef struct turn_condition {
    quux_value agent;
    uint64_t ticket;
} turn_condition;

static bool is_my_turn(void *data) {
    turn_condition *condition = data;
    return agent_record_of(condition->agent)->serving_ticket == condition->ticket;
}

static bool add_waiter_locked(agent_record *record, quux_task *task) {
    if (record->waiter_count == record->waiter_capacity) {
        uint32_t capacity = record->waiter_capacity == 0 ? 4 : record->waiter_capacity * 2;
        quux_task **grown = realloc(record->waiters, capacity * sizeof *grown);
        if (grown == NULL) {
            return false;
        }
        record->waiters = grown;
        record->waiter_capacity = capacity;
    }
    record->waiters[record->waiter_count++] = task;
    return true;
}

static void remove_waiter_locked(agent_record *record, quux_task *task) {
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

static void finish_turn_locked(quux_context *context, agent_record *record) {
    record->serving_ticket++;
    for (uint32_t i = 0; i < record->waiter_count; i++) {
        quux_concurrency_wake_locked(context, record->waiters[i]);
    }
}

/* arguments: agent, ticket, function, extra arguments (an ARGUMENTS object), transact?,
 * shielded? (sent from shield-from-cancellation: cleanup, never withdrawn) */
static bool helper_update(quux_context *context, quux_value *arguments, uint32_t count,
                          quux_value *result) {
    (void)count;
    quux_value agent = arguments[0];
    uint64_t ticket = (uint64_t)quux_integer_value(arguments[1]);
    quux_value function = arguments[2];
    quux_value extra = arguments[3];
    bool transact = quux_is_truthy(arguments[4]);
    bool shielded = quux_is_truthy(arguments[5]);
    quux_value call = quux_nothing;
    quux_value value = quux_nothing;
    QUUX_ROOT_FRAME(context, &agent, &function, &extra, &call, &value);
    agent_record *record = agent_record_of(agent);
    quux_concurrency_lock(context);
    turn_condition condition = {agent, ticket};
    if (!is_my_turn(&condition)) {
        while (!add_waiter_locked(record, context->task)) {
            /* out of memory for the waiter list: poll */
            quux_concurrency_unlock(context);
            quux_sleep_nanoseconds(1000000u);
            quux_concurrency_lock(context);
        }
        quux_concurrency_park_locked(context, is_my_turn, &condition);
        remove_waiter_locked(record, context->task);
    }
    quux_concurrency_unlock(context);

    bool done;
    if (!shielded && quux_concurrency_cancellation_observed(context)) {
        /* withdrawn: the sender's scope was cancelled while the update was queued */
        done = quux_concurrency_raise_cancelled(context);
    } else {
        quux_object *extra_object = quux_checked_object(extra);
        done = quux_make_arguments(context, extra_object->value_count + 1, &call);
        if (done) {
            quux_object *call_object = quux_checked_object(call);
            call_object->values[0] = quux_load_shared(quux_checked_object(agent), QUUX_AGENT_STATE);
            for (uint32_t i = 0; i < extra_object->value_count; i++) {
                call_object->values[i + 1] = quux_checked_object(extra)->values[i];
            }
            done = quux_apply(context, function, call, &value);
        }
        uint64_t length = 0;
        if (done && transact &&
            (!quux_is_kind(value, QUUX_KIND_VECTOR) || !quux_count(context, value, &length) ||
             length != 2)) {
            done = quux_raise(context, "wrong-type",
                              "transact expects the function to return [new-state reply], got %s",
                              quux_kind_phrase(quux_kind_of(value)));
        }
        if (done) {
            quux_value new_state = transact ? quux_vector_element(value, 0) : value;
            quux_store_shared(context, quux_checked_object(agent), QUUX_AGENT_STATE, new_state);
            *result = transact ? quux_vector_element(value, 1) : value;
        }
    }
    quux_concurrency_lock(context);
    finish_turn_locked(context, record);
    quux_concurrency_unlock(context);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool send(quux_context *context, quux_value *arguments, uint32_t count, bool transact,
                 quux_value *result) {
    const char *name = transact ? "transact" : "update";
    if (!quux_is_kind(arguments[0], QUUX_KIND_AGENT)) {
        return quux_raise(context, "wrong-type", "%s expects an agent, got %s", name,
                          quux_kind_phrase(quux_kind_of(arguments[0])));
    }
    quux_kind function_kind = quux_kind_of(arguments[1]);
    if (function_kind != QUUX_KIND_CLOSURE && function_kind != QUUX_KIND_PRIMITIVE) {
        return quux_raise(context, "wrong-type", "%s expects a function, got %s", name,
                          quux_kind_phrase(function_kind));
    }
    quux_value extra = quux_nothing;
    quux_value helper_arguments = quux_nothing;
    quux_value ticket = quux_nothing;
    QUUX_ROOT_FRAME(context, &extra, &helper_arguments, &ticket);
    bool done = quux_make_arguments(context, count - 2, &extra) &&
                quux_make_arguments(context, 6, &helper_arguments);
    if (done) {
        for (uint32_t i = 2; i < count; i++) {
            quux_checked_object(extra)->values[i - 2] = arguments[i];
        }
        quux_concurrency_lock(context);
        uint64_t number = agent_record_of(arguments[0])->next_ticket++;
        quux_concurrency_unlock(context);
        done = quux_make_integer(context, (int64_t)number, &ticket);
    }
    if (done) {
        quux_object *object = quux_checked_object(helper_arguments);
        object->values[0] = arguments[0];
        object->values[1] = ticket;
        object->values[2] = arguments[1];
        object->values[3] = extra;
        object->values[4] = quux_boolean(transact);
        object->values[5] = quux_boolean(context->task != NULL && context->task->shield_depth > 0);
        done =
            quux_concurrency_spawn(context, context->current_scope,
                                   quux_concurrency_of(context)->helpers[QUUX_HELPER_AGENT_UPDATE],
                                   helper_arguments, result);
        if (!done) {
            /* the ticket was taken: give the turn back so later sends are not stuck */
            quux_concurrency_lock(context);
            finish_turn_locked(context, agent_record_of(arguments[0]));
            quux_concurrency_unlock(context);
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool primitive_update(quux_context *context, quux_value *arguments, uint32_t count,
                             quux_value *result) {
    return send(context, arguments, count, false, result);
}

static bool primitive_transact(quux_context *context, quux_value *arguments, uint32_t count,
                               quux_value *result) {
    return send(context, arguments, count, true, result);
}

bool quux_agents_register(quux_context *context) {
    return quux_define_primitive(context, "agent", primitive_agent, 1, 1) &&
           quux_define_primitive(context, "update", primitive_update, 2, QUUX_VARIADIC) &&
           quux_define_primitive(context, "transact", primitive_transact, 2, QUUX_VARIADIC) &&
           quux_make_primitive(context, "update", helper_update, 6, 6,
                               &quux_concurrency_of(context)->helpers[QUUX_HELPER_AGENT_UPDATE]);
}
