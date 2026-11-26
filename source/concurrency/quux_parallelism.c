/* fork-join (section 11, D-43): the sub-problems of a :split run as futures in a child scope of
 * the current scope, and their results are combined in the order split returned them, so the
 * result is the same as plain recursion. parallel-transform and parallel-reduce (fixed chunks
 * and the fixed reduction tree) belong to task T054. */
#include "quux_concurrency_internal.h"

static bool solve(quux_context *context, quux_value split, quux_value combine, quux_value problem,
                  quux_value *result);

static bool wrong_step(quux_context *context, quux_value step) {
    return quux_raise(context, "wrong-type",
                      "fork-join expects split to return [:leaf value] or [:split sub-problem "
                      "...], got %s",
                      quux_kind_phrase(quux_kind_of(step)));
}

static bool solve_parts(quux_context *context, quux_value split, quux_value combine,
                        quux_value step, uint64_t count, quux_value *result) {
    quux_value futures = quux_nothing;
    quux_value arguments = quux_nothing;
    quux_value future = quux_nothing;
    quux_value accumulated = quux_nothing;
    quux_value value = quux_nothing;
    QUUX_ROOT_FRAME(context, &split, &combine, &step, &futures, &arguments, &future, &accumulated,
                    &value);
    bool done = quux_vector_empty(context, &futures);
    for (uint64_t i = 1; done && i < count; i++) {
        done = quux_make_arguments(context, 3, &arguments);
        if (done) {
            quux_object *object = quux_checked_object(arguments);
            object->values[0] = split;
            object->values[1] = combine;
            object->values[2] = quux_vector_element(step, i);
            done = quux_concurrency_spawn(
                       context, context->current_scope,
                       quux_concurrency_of(context)->helpers[QUUX_HELPER_FORK_JOIN_SOLVE],
                       arguments, &future) &&
                   quux_vector_add(context, futures, future, &futures);
        }
    }
    for (uint64_t i = 0; done && i + 1 < count; i++) {
        done = quux_concurrency_wait_for(context, quux_vector_element(futures, i), &value);
        if (done) {
            done = i == 0 ? (accumulated = value, true)
                          : quux_call2(context, combine, accumulated, value, &accumulated);
        }
    }
    if (done) {
        *result = accumulated;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool solve(quux_context *context, quux_value split, quux_value combine, quux_value problem,
                  quux_value *result) {
    quux_value step = quux_nothing;
    QUUX_ROOT_FRAME(context, &split, &combine, &step);
    bool done = quux_call1(context, split, problem, &step);
    uint64_t count = 0;
    if (done && (!quux_is_kind(step, QUUX_KIND_VECTOR) || !quux_count(context, step, &count) ||
                 count < 2)) {
        done = wrong_step(context, step);
    }
    if (done) {
        quux_value marker = quux_vector_element(step, 0);
        if (quux_is_kind(marker, QUUX_KIND_KEYWORD) && quux_name_equals(marker, "leaf") &&
            count == 2) {
            *result = quux_vector_element(step, 1);
        } else if (quux_is_kind(marker, QUUX_KIND_KEYWORD) && quux_name_equals(marker, "split")) {
            done = solve_parts(context, split, combine, step, count, result);
        } else {
            done = wrong_step(context, step);
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool helper_solve(quux_context *context, quux_value *arguments, uint32_t count,
                         quux_value *result) {
    (void)count;
    return solve(context, arguments[0], arguments[1], arguments[2], result);
}

typedef struct fork_join_call {
    quux_value split;
    quux_value combine;
    quux_value problem;
} fork_join_call;

static bool fork_join_body(quux_context *context, quux_value scope, void *data,
                           quux_value *result) {
    (void)scope;
    fork_join_call *call = data;
    return solve(context, call->split, call->combine, call->problem, result);
}

static bool primitive_fork_join(quux_context *context, quux_value *arguments, uint32_t count,
                                quux_value *result) {
    (void)count;
    fork_join_call call = {arguments[0], arguments[1], arguments[2]};
    QUUX_ROOT_FRAME(context, &call.split, &call.combine, &call.problem);
    bool done = quux_concurrency_with_scope(context, fork_join_body, &call, result);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_parallelism_register(quux_context *context) {
    return quux_define_primitive(context, "fork-join", primitive_fork_join, 3, 3) &&
           quux_make_primitive(context, "fork-join", helper_solve, 3, 3,
                               &quux_concurrency_of(context)->helpers[QUUX_HELPER_FORK_JOIN_SOLVE]);
}
