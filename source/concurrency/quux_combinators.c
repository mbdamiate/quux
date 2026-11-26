/* The combinators of section 08 (D-12, D-11 amendment). Each one returns a future that a helper
 * task fills; the helper is a child of the current scope, so cancelling the scope cancels it.
 * Futures a combinator no longer needs are cancelled without failing their scope (section 07
 * rule 8). */
#include "quux_concurrency_internal.h"

static bool require_futures(quux_context *context, const char *name, quux_value futures) {
    if (!quux_is_kind(futures, QUUX_KIND_VECTOR)) {
        return quux_raise(context, "wrong-type", "%s expects a vector of futures, got %s", name,
                          quux_kind_phrase(quux_kind_of(futures)));
    }
    uint64_t count = 0;
    quux_count(context, futures, &count);
    for (uint64_t i = 0; i < count; i++) {
        quux_value element = quux_vector_element(futures, i);
        if (!quux_is_kind(element, QUUX_KIND_FUTURE)) {
            return quux_raise(context, "wrong-type", "%s expects a vector of futures, got %s", name,
                              quux_kind_phrase(quux_kind_of(element)));
        }
    }
    return true;
}

/* Starts helper(arguments...) as a future in the current scope. */
static bool start_helper(quux_context *context, int helper, const quux_value *values,
                         uint32_t count, quux_value *result) {
    quux_value arguments = quux_nothing;
    QUUX_ROOT_FRAME(context, &arguments);
    bool done = quux_make_arguments(context, count, &arguments);
    if (done) {
        for (uint32_t i = 0; i < count; i++) {
            quux_checked_object(arguments)->values[i] = values[i];
        }
        done = quux_concurrency_spawn(context, context->current_scope,
                                      quux_concurrency_of(context)->helpers[helper], arguments,
                                      result);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* The futures of `futures` that are still empty. */
static bool still_empty(quux_context *context, quux_value futures, quux_value *result) {
    quux_value pending = quux_nothing;
    QUUX_ROOT_FRAME(context, &futures, &pending);
    uint64_t count = 0;
    quux_count(context, futures, &count);
    bool done = quux_vector_empty(context, &pending);
    for (uint64_t i = 0; done && i < count; i++) {
        quux_value future = quux_vector_element(futures, i);
        if (quux_future_current_state(future) == QUUX_FUTURE_EMPTY) {
            done = quux_vector_add(context, pending, future, &pending);
        }
    }
    if (done) {
        *result = pending;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static void cancel_unneeded(quux_context *context, quux_value futures) {
    uint64_t count = 0;
    quux_count(context, futures, &count);
    quux_concurrency_lock(context);
    for (uint64_t i = 0; i < count; i++) {
        quux_concurrency_cancel_unneeded_locked(context, quux_vector_element(futures, i));
    }
    quux_concurrency_unlock(context);
}

/* Parks until one of the still-empty futures settles. When none is empty any more (they
 * settled after the caller looked), returns at once so the caller looks again. */
static bool wait_for_change(quux_context *context, quux_value futures) {
    quux_value pending = quux_nothing;
    QUUX_ROOT_FRAME(context, &futures, &pending);
    bool done = still_empty(context, futures, &pending);
    uint64_t count = 0;
    if (done && quux_count(context, pending, &count) && count > 0 &&
        quux_concurrency_wait_for_any(context, pending, QUUX_NO_DEADLINE) == QUUX_PARK_CANCELLED) {
        done = quux_concurrency_raise_cancelled(context);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- all ---- */

static bool helper_all(quux_context *context, quux_value *arguments, uint32_t count,
                       quux_value *result) {
    (void)count;
    quux_value futures = arguments[0];
    quux_value values = quux_nothing;
    QUUX_ROOT_FRAME(context, &futures, &values);
    uint64_t total = 0;
    quux_count(context, futures, &total);
    bool done = true;
    for (;;) {
        bool all_filled = true;
        quux_value failed = quux_nothing;
        for (uint64_t i = 0; i < total; i++) {
            quux_value future = quux_vector_element(futures, i);
            quux_future_state state = quux_future_current_state(future);
            all_filled = all_filled && state == QUUX_FUTURE_FILLED;
            if (state == QUUX_FUTURE_FAILED && quux_is_nothing(failed)) {
                failed = future;
            }
        }
        if (!quux_is_nothing(failed)) {
            /* the first failure: the futures still empty are no longer needed */
            cancel_unneeded(context, futures);
            done = quux_raise_value(context, quux_future_error(failed));
            break;
        }
        if (all_filled) {
            done = quux_vector_empty(context, &values);
            for (uint64_t i = 0; done && i < total; i++) {
                quux_value value = quux_nothing;
                done =
                    quux_concurrency_wait_for(context, quux_vector_element(futures, i), &value) &&
                    quux_vector_add(context, values, value, &values);
            }
            if (done) {
                *result = values;
            }
            break;
        }
        if (!wait_for_change(context, futures)) {
            done = false;
            break;
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool primitive_all(quux_context *context, quux_value *arguments, uint32_t count,
                          quux_value *result) {
    (void)count;
    return require_futures(context, "all", arguments[0]) &&
           start_helper(context, QUUX_HELPER_ALL, arguments, 1, result);
}

/* ---- first-success ---- */

static bool all_failed_error(quux_context *context, quux_value futures) {
    quux_value errors = quux_nothing;
    quux_value data = quux_nothing;
    quux_value message = quux_nothing;
    quux_value error = quux_nothing;
    QUUX_ROOT_FRAME(context, &futures, &errors, &data, &message, &error);
    uint64_t total = 0;
    quux_count(context, futures, &total);
    bool done = quux_vector_empty(context, &errors);
    for (uint64_t i = 0; done && i < total; i++) {
        done = quux_vector_add(context, errors, quux_future_error(quux_vector_element(futures, i)),
                               &errors);
    }
    done =
        done && quux_map_empty(context, &data) &&
        quux_map_put(context, data, quux_keyword(context, "errors"), errors, &data) &&
        quux_make_string_from_c(context, "every alternative of first-success failed", &message) &&
        quux_make_error(context, quux_keyword(context, "all-failed"), message, data, &error);
    QUUX_ROOT_FRAME_END(context);
    return done && quux_raise_value(context, error);
}

static bool helper_first_success(quux_context *context, quux_value *arguments, uint32_t count,
                                 quux_value *result) {
    (void)count;
    quux_value futures = arguments[0];
    QUUX_ROOT_FRAME(context, &futures);
    uint64_t total = 0;
    quux_count(context, futures, &total);
    bool done = true;
    for (;;) {
        bool all_failed = true;
        quux_value winner = quux_nothing;
        for (uint64_t i = 0; i < total; i++) {
            quux_value future = quux_vector_element(futures, i);
            quux_future_state state = quux_future_current_state(future);
            all_failed = all_failed && state == QUUX_FUTURE_FAILED;
            if (state == QUUX_FUTURE_FILLED && quux_is_nothing(winner)) {
                winner = future;
            }
        }
        if (!quux_is_nothing(winner)) {
            cancel_unneeded(context, futures);
            done = quux_concurrency_wait_for(context, winner, result);
            break;
        }
        if (all_failed) {
            done = all_failed_error(context, futures);
            break;
        }
        if (!wait_for_change(context, futures)) {
            done = false;
            break;
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool primitive_first_success(quux_context *context, quux_value *arguments, uint32_t count,
                                    quux_value *result) {
    (void)count;
    if (!require_futures(context, "first-success", arguments[0])) {
        return false;
    }
    quux_concurrency_handle_failures(context, arguments[0]);
    return start_helper(context, QUUX_HELPER_FIRST_SUCCESS, arguments, 1, result);
}

/* ---- then ---- */

static bool helper_then(quux_context *context, quux_value *arguments, uint32_t count,
                        quux_value *result) {
    (void)count;
    quux_value function = arguments[1];
    quux_value value = quux_nothing;
    QUUX_ROOT_FRAME(context, &function, &value);
    bool done = quux_concurrency_wait_for(context, arguments[0], &value) &&
                quux_call1(context, function, value, result);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool is_function(quux_value value) {
    quux_kind kind = quux_kind_of(value);
    return kind == QUUX_KIND_CLOSURE || kind == QUUX_KIND_PRIMITIVE;
}

static bool primitive_then(quux_context *context, quux_value *arguments, uint32_t count,
                           quux_value *result) {
    (void)count;
    if (!quux_is_kind(arguments[0], QUUX_KIND_FUTURE)) {
        return quux_raise(context, "wrong-type", "then expects a future, got %s",
                          quux_kind_phrase(quux_kind_of(arguments[0])));
    }
    if (!is_function(arguments[1])) {
        return quux_raise(context, "wrong-type", "then expects a function, got %s",
                          quux_kind_phrase(quux_kind_of(arguments[1])));
    }
    return start_helper(context, QUUX_HELPER_THEN, arguments, 2, result);
}

/* ---- with-timeout ---- */

static bool helper_with_timeout(quux_context *context, quux_value *arguments, uint32_t count,
                                quux_value *result) {
    (void)count;
    quux_value inner = arguments[0];
    uint64_t deadline = (uint64_t)quux_integer_value(arguments[1]);
    int64_t milliseconds = quux_integer_value(arguments[2]);
    quux_value pending = quux_nothing;
    QUUX_ROOT_FRAME(context, &inner, &pending);
    bool done =
        quux_vector_empty(context, &pending) && quux_vector_add(context, pending, inner, &pending);
    if (done) {
        quux_park_outcome outcome = quux_future_current_state(inner) == QUUX_FUTURE_EMPTY
                                        ? quux_concurrency_wait_for_any(context, pending, deadline)
                                        : QUUX_PARK_SATISFIED;
        if (quux_future_current_state(inner) != QUUX_FUTURE_EMPTY) {
            /* completion won the race (section 08 rule 1) */
            done = quux_concurrency_wait_for(context, inner, result);
        } else if (outcome == QUUX_PARK_DEADLINE) {
            cancel_unneeded(context, pending);
            done = quux_raise(context, "timeout", "with-timeout ran out after %lld ms",
                              (long long)milliseconds);
        } else {
            done = quux_concurrency_raise_cancelled(context);
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool primitive_with_timeout(quux_context *context, quux_value *arguments, uint32_t count,
                                   quux_value *result) {
    (void)count;
    quux_value milliseconds = arguments[0];
    quux_value subject = arguments[1];
    if (!quux_is_kind(milliseconds, QUUX_KIND_INTEGER) || quux_integer_value(milliseconds) < 0) {
        return quux_raise(context, "wrong-type",
                          "with-timeout expects a non-negative integer of milliseconds, got %s",
                          quux_kind_phrase(quux_kind_of(milliseconds)));
    }
    if (!quux_is_kind(subject, QUUX_KIND_FUTURE) && !is_function(subject)) {
        return quux_raise(context, "wrong-type",
                          "with-timeout expects a future or a function, got %s",
                          quux_kind_phrase(quux_kind_of(subject)));
    }
    uint64_t deadline =
        quux_concurrency_now(context) + (uint64_t)quux_integer_value(milliseconds) * 1000000u;
    quux_value values[3] = {subject, quux_nothing, milliseconds};
    quux_value empty = quux_nothing;
    QUUX_ROOT_FRAME(context, &values[0], &values[1], &values[2], &empty);
    bool done = quux_make_integer(context, (int64_t)deadline, &values[1]);
    if (done && !quux_is_kind(subject, QUUX_KIND_FUTURE)) {
        /* a function is started as the inner future, in the current scope */
        done = quux_make_arguments(context, 0, &empty) &&
               quux_concurrency_spawn(context, context->current_scope, subject, empty, &values[0]);
    }
    done = done && start_helper(context, QUUX_HELPER_WITH_TIMEOUT, values, 3, result);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static const struct {
    const char *name;
    quux_primitive_function function;
    uint32_t minimum;
    uint32_t maximum;
} combinator_primitives[] = {
    {"all", primitive_all, 1, 1},
    {"first-success", primitive_first_success, 1, 1},
    {"then", primitive_then, 2, 2},
    {"with-timeout", primitive_with_timeout, 2, 2},
};

static const struct {
    int helper;
    const char *name;
    quux_primitive_function function;
    uint32_t count;
} combinator_helpers[] = {
    {QUUX_HELPER_ALL, "all", helper_all, 1},
    {QUUX_HELPER_FIRST_SUCCESS, "first-success", helper_first_success, 1},
    {QUUX_HELPER_THEN, "then", helper_then, 2},
    {QUUX_HELPER_WITH_TIMEOUT, "with-timeout", helper_with_timeout, 3},
};

bool quux_combinators_register(quux_context *context) {
    quux_concurrency_state *state = quux_concurrency_of(context);
    bool done = true;
    for (size_t i = 0; done && i < sizeof combinator_primitives / sizeof combinator_primitives[0];
         i++) {
        done = quux_define_primitive(
            context, combinator_primitives[i].name, combinator_primitives[i].function,
            combinator_primitives[i].minimum, combinator_primitives[i].maximum);
    }
    for (size_t i = 0; done && i < sizeof combinator_helpers / sizeof combinator_helpers[0]; i++) {
        done =
            quux_make_primitive(context, combinator_helpers[i].name, combinator_helpers[i].function,
                                combinator_helpers[i].count, combinator_helpers[i].count,
                                &state->helpers[combinator_helpers[i].helper]);
    }
    return done;
}
