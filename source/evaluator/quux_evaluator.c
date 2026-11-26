#include "quux_evaluator.h"

#include "quux_collections.h"
#include "quux_encoding.h"
#include "quux_expander.h"
#include "quux_namespaces.h"
#include "quux_printer.h"
#include "quux_reader.h"

#include <string.h>

#include "quux_core_prelude.h" /* generated from quux_core_prelude.qx */

typedef struct primitive_shape {
    quux_primitive_function function;
    uint32_t minimum;
    uint32_t maximum;
} primitive_shape;

static primitive_shape read_primitive(quux_object *primitive) {
    primitive_shape shape;
    memcpy(&shape, quux_object_bytes(primitive), sizeof shape);
    return shape;
}

/* ---- Environments ---- */

static bool make_frame(quux_context *context, quux_value parent, uint32_t size,
                       quux_value *result) {
    QUUX_ROOT_FRAME(context, &parent);
    quux_object *frame =
        quux_allocate(context, QUUX_KIND_ENVIRONMENT, QUUX_ENVIRONMENT_FIRST_SLOT + size, 0);
    QUUX_ROOT_FRAME_END(context);
    if (frame == NULL) {
        return false;
    }
    frame->values[QUUX_ENVIRONMENT_PARENT] = parent;
    *result = quux_object_value(frame);
    return true;
}

static void store_slot(quux_context *context, quux_value frame, uint32_t slot, quux_value value) {
    quux_store(context, quux_checked_object(frame), QUUX_ENVIRONMENT_FIRST_SLOT + slot, value);
}

static bool make_closure(quux_context *context, quux_value lambda, quux_value environment,
                         quux_value *result) {
    QUUX_ROOT_FRAME(context, &lambda, &environment);
    quux_object *closure = quux_allocate(context, QUUX_KIND_CLOSURE, 2, 0);
    QUUX_ROOT_FRAME_END(context);
    if (closure == NULL) {
        return false;
    }
    closure->values[QUUX_CLOSURE_LAMBDA] = lambda;
    closure->values[QUUX_CLOSURE_ENVIRONMENT] = environment;
    *result = quux_object_value(closure);
    return true;
}

/* ---- Patterns (D-39, section 03 rule 6) ---- */

static bool bind_pattern(quux_context *context, quux_value pattern, quux_value value,
                         quux_value frame) {
    quux_object *code = quux_checked_object(pattern);
    switch (quux_code_operation(code)) {
    case QUUX_OPERATION_PATTERN_NAME:
        store_slot(context, frame, quux_code_field(code, 0), value);
        return true;
    case QUUX_OPERATION_PATTERN_VECTOR: {
        if (!quux_is_sequential(value)) {
            return quux_raise(context, "wrong-type",
                              "a vector pattern needs a list or a vector, got %s",
                              quux_kind_phrase(quux_kind_of(value)));
        }
        bool has_rest = quux_code_field(code, 0) != 0;
        uint32_t positional = code->value_count - (has_rest ? 1 : 0);
        quux_value rest = quux_nothing;
        bool done = true;
        QUUX_ROOT_FRAME(context, &pattern, &value, &frame, &rest);
        bool is_vector = quux_is_kind(value, QUUX_KIND_VECTOR);
        uint64_t count = is_vector ? quux_vector_count(value) : quux_list_count(value);
        quux_value cell = value;
        for (uint32_t i = 0; i < positional && done; i++) {
            quux_value element = quux_nothing;
            if (i < count) {
                element = is_vector ? quux_vector_element(value, i) : quux_pair_first(cell);
            }
            if (!is_vector && !quux_is_nothing(cell)) {
                cell = quux_pair_rest(cell);
            }
            done = bind_pattern(context, quux_checked_object(pattern)->values[i], element, frame);
        }
        if (done && has_rest) {
            if (is_vector) {
                for (uint64_t i = count; i > positional && done; i--) {
                    done =
                        quux_list_prepend(context, quux_vector_element(value, i - 1), rest, &rest);
                }
            } else {
                rest = cell;
            }
            done = done && bind_pattern(context, quux_checked_object(pattern)->values[positional],
                                        rest, frame);
        }
        QUUX_ROOT_FRAME_END(context);
        return done;
    }
    case QUUX_OPERATION_PATTERN_MAP: {
        if (!quux_is_kind(value, QUUX_KIND_MAP)) {
            return quux_raise(context, "wrong-type", "a map pattern needs a map, got %s",
                              quux_kind_phrase(quux_kind_of(value)));
        }
        bool done = true;
        quux_value element = quux_nothing;
        QUUX_ROOT_FRAME(context, &pattern, &value, &frame, &element);
        for (uint32_t i = 0; i < quux_checked_object(pattern)->value_count && done; i += 2) {
            done =
                quux_get(context, value, quux_checked_object(pattern)->values[i], quux_nothing,
                         &element) &&
                bind_pattern(context, quux_checked_object(pattern)->values[i + 1], element, frame);
        }
        QUUX_ROOT_FRAME_END(context);
        return done;
    }
    default:
        return quux_raise(context, "wrong-type", "not a pattern");
    }
}

/* Builds the parameter frame of a closure call from an ARGUMENTS object. */
static bool bind_parameters(quux_context *context, quux_value closure, quux_value arguments,
                            quux_value *frame) {
    quux_object *lambda =
        quux_checked_object(quux_checked_object(closure)->values[QUUX_CLOSURE_LAMBDA]);
    quux_lambda_shape shape = quux_lambda_read_shape(lambda);
    uint32_t count = quux_checked_object(arguments)->value_count;
    if (count < shape.required_count || (!shape.has_rest && count > shape.required_count)) {
        quux_value name = lambda->values[QUUX_LAMBDA_NAME];
        return quux_raise(context, "wrong-argument-count", "%.*s expects %s%u argument%s, got %u",
                          quux_is_nothing(name) ? 8 : (int)quux_name_length(name),
                          quux_is_nothing(name) ? "function" : quux_name_bytes(name),
                          shape.has_rest ? "at least " : "", shape.required_count,
                          shape.required_count == 1 ? "" : "s", count);
    }
    quux_value rest = quux_nothing;
    bool done = true;
    QUUX_ROOT_FRAME(context, &closure, &arguments, &rest);
    /* *frame is a slot that the caller roots. */
    done = make_frame(context, quux_checked_object(closure)->values[QUUX_CLOSURE_ENVIRONMENT],
                      shape.frame_size, frame);
    for (uint32_t i = 0; i < shape.required_count && done; i++) {
        quux_object *current_lambda =
            quux_checked_object(quux_checked_object(closure)->values[QUUX_CLOSURE_LAMBDA]);
        done = bind_pattern(context, current_lambda->values[QUUX_LAMBDA_PARAMETERS + i],
                            quux_checked_object(arguments)->values[i], *frame);
    }
    if (done && shape.has_rest) {
        done =
            quux_list_from_object_values(context, quux_checked_object(arguments),
                                         shape.required_count, count - shape.required_count, &rest);
        quux_object *current_lambda =
            quux_checked_object(quux_checked_object(closure)->values[QUUX_CLOSURE_LAMBDA]);
        done =
            done && bind_pattern(context, current_lambda->values[QUUX_LAMBDA_REST], rest, *frame);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- Calls ---- */

bool quux_make_arguments(quux_context *context, uint32_t count, quux_value *result) {
    quux_object *arguments = quux_allocate(context, QUUX_KIND_ARGUMENTS, count, 0);
    if (arguments == NULL) {
        return false;
    }
    *result = quux_object_value(arguments);
    return true;
}

static bool call_primitive(quux_context *context, quux_value primitive, quux_value arguments,
                           quux_value *result) {
    quux_object *object = quux_checked_object(primitive);
    primitive_shape shape = read_primitive(object);
    uint32_t count = quux_checked_object(arguments)->value_count;
    if (count < shape.minimum || count > shape.maximum) {
        /* the same form as a program function's message (friction P10-F2) */
        quux_value name = object->values[QUUX_PRIMITIVE_NAME];
        char expected[64];
        if (shape.maximum == QUUX_VARIADIC) {
            snprintf(expected, sizeof expected, "at least %u argument%s", shape.minimum,
                     shape.minimum == 1 ? "" : "s");
        } else if (shape.minimum == shape.maximum) {
            snprintf(expected, sizeof expected, "%u argument%s", shape.minimum,
                     shape.minimum == 1 ? "" : "s");
        } else if (shape.maximum == shape.minimum + 1) {
            snprintf(expected, sizeof expected, "%u or %u arguments", shape.minimum, shape.maximum);
        } else {
            snprintf(expected, sizeof expected, "%u to %u arguments", shape.minimum, shape.maximum);
        }
        return quux_raise(context, "wrong-argument-count", "%.*s expects %s, got %u",
                          (int)quux_name_length(name), quux_name_bytes(name), expected, count);
    }
    return shape.function(context, quux_checked_object(arguments)->values, count, result);
}

static bool enter(quux_context *context) {
    if (context->recursion_depth >= context->runtime->maximum_recursion_depth) {
        return quux_raise(context, "stack-exhausted",
                          "calls not in tail position are nested deeper than %u",
                          context->runtime->maximum_recursion_depth);
    }
    context->recursion_depth++;
    return true;
}

bool quux_apply(quux_context *context, quux_value function, quux_value arguments,
                quux_value *result) {
    quux_kind kind = quux_kind_of(function);
    if (kind == QUUX_KIND_PRIMITIVE) {
        return call_primitive(context, function, arguments, result);
    }
    if (kind != QUUX_KIND_CLOSURE) {
        return quux_raise(context, "wrong-type", "%s is not a function", quux_kind_phrase(kind));
    }
    quux_value frame = quux_nothing;
    QUUX_ROOT_FRAME(context, &function, &arguments, &frame);
    bool done = bind_parameters(context, function, arguments, &frame) &&
                quux_evaluate(
                    context,
                    quux_checked_object(quux_checked_object(function)->values[QUUX_CLOSURE_LAMBDA])
                        ->values[QUUX_LAMBDA_BODY],
                    frame, result);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_call1(quux_context *context, quux_value function, quux_value first, quux_value *result) {
    quux_value arguments = quux_nothing;
    QUUX_ROOT_FRAME(context, &function, &first, &arguments);
    bool done = quux_make_arguments(context, 1, &arguments);
    if (done) {
        quux_checked_object(arguments)->values[0] = first;
        done = quux_apply(context, function, arguments, result);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_call2(quux_context *context, quux_value function, quux_value first, quux_value second,
                quux_value *result) {
    quux_value arguments = quux_nothing;
    QUUX_ROOT_FRAME(context, &function, &first, &second, &arguments);
    bool done = quux_make_arguments(context, 2, &arguments);
    if (done) {
        quux_checked_object(arguments)->values[0] = first;
        quux_checked_object(arguments)->values[1] = second;
        done = quux_apply(context, function, arguments, result);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- Building collections (literals with expressions, quasi-quote) ---- */

static bool build_collection(quux_context *context, quux_value code, quux_value environment,
                             quux_value *result) {
    quux_operation operation = quux_code_operation(quux_checked_object(code));
    quux_value collected = quux_nothing; /* reversed list of elements */
    quux_value element = quux_nothing;
    quux_value collection = quux_nothing;
    quux_value key = quux_nothing;
    bool done = true;
    QUUX_ROOT_FRAME(context, &code, &environment, &collected, &element, &collection, &key);
    uint32_t count = quux_checked_object(code)->value_count;
    for (uint32_t i = 0; i < count && done; i++) {
        quux_value part = quux_checked_object(code)->values[i];
        quux_object *part_code = quux_checked_object(part);
        if (quux_code_operation(part_code) == QUUX_OPERATION_SPLICE) {
            done = quux_evaluate(context, part_code->values[0], environment, &element) &&
                   quux_as_list(context, element, &element);
            for (quux_value cell = element; done && !quux_is_nothing(cell);
                 cell = quux_pair_rest(cell)) {
                done = quux_list_prepend(context, quux_pair_first(cell), collected, &collected);
            }
        } else {
            done = quux_evaluate(context, part, environment, &element) &&
                   quux_list_prepend(context, element, collected, &collected);
        }
    }
    done = done && quux_list_reverse(context, collected, &collected);
    if (done) {
        switch (operation) {
        case QUUX_OPERATION_BUILD_LIST:
            collection = collected;
            break;
        case QUUX_OPERATION_BUILD_VECTOR:
            done = quux_vector_empty(context, &collection);
            for (quux_value cell = collected; done && !quux_is_nothing(cell);
                 cell = quux_pair_rest(cell)) {
                done = quux_vector_add(context, collection, quux_pair_first(cell), &collection);
            }
            break;
        case QUUX_OPERATION_BUILD_SET:
            done = quux_set_empty(context, &collection);
            for (quux_value cell = collected; done && !quux_is_nothing(cell);
                 cell = quux_pair_rest(cell)) {
                done = quux_map_put(context, collection, quux_pair_first(cell),
                                    quux_pair_first(cell), &collection);
            }
            break;
        default:
            done = quux_map_empty(context, &collection);
            if (quux_list_count(collected) % 2 != 0) {
                done = quux_raise(context, "wrong-type", "a map needs keys and values in pairs");
            }
            for (quux_value cell = collected; done && !quux_is_nothing(cell);) {
                key = quux_pair_first(cell);
                cell = quux_pair_rest(cell);
                done = quux_map_put(context, collection, key, quux_pair_first(cell), &collection);
                cell = quux_pair_rest(cell);
            }
            break;
        }
    }
    if (done) {
        *result = collection;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- try / catch / finally (section 12) ---- */

static bool error_matches(quux_context *context, quux_value matcher, quux_value error,
                          bool *matches) {
    if (quux_is_kind(matcher, QUUX_KIND_KEYWORD)) {
        *matches = quux_is_kind(error, QUUX_KIND_ERROR) &&
                   quux_error_field(error, QUUX_ERROR_KIND) == matcher;
        return true;
    }
    quux_value verdict = quux_nothing;
    QUUX_ROOT_FRAME(context, &verdict);
    bool done = quux_call1(context, matcher, error, &verdict);
    QUUX_ROOT_FRAME_END(context);
    *matches = done && quux_is_truthy(verdict);
    return done;
}

static bool evaluate_try(quux_context *context, quux_value code, quux_value environment,
                         quux_value *result) {
    quux_value value = quux_nothing;
    quux_value error = quux_nothing;
    quux_value matcher = quux_nothing;
    quux_value frame = quux_nothing;
    quux_value ignored = quux_nothing;
    quux_value saved_error = quux_nothing;
    QUUX_ROOT_FRAME(context, &code, &environment, &value, &error, &matcher, &frame, &ignored,
                    &saved_error);
    bool done = quux_evaluate(context, quux_checked_object(code)->values[0], environment, &value);
    if (!done) {
        error = context->pending_error;
        uint32_t count = quux_checked_object(code)->value_count;
        for (uint32_t i = 2; i + 1 < count; i += 2) {
            bool matches = false;
            if (!quux_evaluate(context, quux_checked_object(code)->values[i], environment,
                               &matcher) ||
                !error_matches(context, matcher, error, &matches)) {
                error = context->pending_error; /* the matcher itself failed */
                break;
            }
            if (matches) {
                context->pending_error = quux_nothing;
                done = make_frame(context, environment, 1, &frame);
                if (done) {
                    store_slot(context, frame, 0, error);
                    done = quux_evaluate(context, quux_checked_object(code)->values[i + 1], frame,
                                         &value);
                }
                if (!done) {
                    error = context->pending_error;
                }
                break;
            }
        }
        if (!done) {
            context->pending_error = error;
        }
    }
    quux_value finally_code = quux_checked_object(code)->values[1];
    if (!quux_is_nothing(finally_code)) {
        saved_error = context->pending_error;
        bool cleaned = quux_evaluate(context, finally_code, environment, &ignored);
        if (cleaned) {
            context->pending_error = done ? quux_nothing : saved_error;
        } else {
            done = false; /* an error in finally replaces the original outcome */
        }
    }
    if (done) {
        *result = value;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- Globals, definitions, namespaces ---- */

static bool global_value(quux_context *context, quux_object *code, quux_value *result) {
    quux_value definition = quux_load_shared(code, 1);
    if (quux_is_nothing(definition)) {
        /* Not defined when analyzed (a forward reference): resolve now and keep the record
         * (section 14 rules 3 and 5). */
        if (!quux_namespace_resolve(context, code->values[2], code->values[0], &definition)) {
            return false;
        }
        if (quux_is_nothing(definition)) {
            quux_value name = code->values[0];
            return quux_raise(context, "undefined-name", "%.*s is not defined",
                              (int)quux_name_length(name), quux_name_bytes(name));
        }
        quux_store_shared(context, code, 1, definition);
    }
    if (quux_definition_flags(definition) & QUUX_DEFINITION_FLAG_MACRO) {
        quux_value name = code->values[0];
        return quux_raise(context, "wrong-type", "%.*s is a macro, not a value",
                          (int)quux_name_length(name), quux_name_bytes(name));
    }
    *result = quux_definition_field(definition, QUUX_DEFINITION_VALUE);
    return true;
}

static bool evaluate_define(quux_context *context, quux_value code, quux_value environment,
                            quux_value *result) {
    quux_value value = quux_nothing;
    quux_value definition = quux_nothing;
    QUUX_ROOT_FRAME(context, &code, &environment, &value, &definition);
    quux_object *node = quux_checked_object(code);
    bool done = quux_evaluate(context, node->values[1], environment, &value);
    if (done) {
        node = quux_checked_object(code);
        done = quux_namespace_define(context, context->current_namespace, node->values[0], value,
                                     node->values[1], quux_code_field(node, 0), &definition);
    }
    if (done) {
        *result = quux_checked_object(code)->values[0];
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool evaluate_namespace(quux_context *context, quux_value code, quux_value *result) {
    quux_value target = quux_nothing;
    quux_value imported = quux_nothing;
    QUUX_ROOT_FRAME(context, &code, &target, &imported);
    bool done =
        quux_namespace_find_or_create(context, quux_checked_object(code)->values[0], &target);
    for (uint32_t i = 1; done && i < quux_checked_object(code)->value_count; i++) {
        quux_value name = quux_checked_object(code)->values[i];
        imported = quux_namespace_find(context, name);
        if (quux_is_nothing(imported)) {
            done = quux_raise(context, "undefined-name", "namespace %.*s does not exist",
                              (int)quux_name_length(name), quux_name_bytes(name));
        } else {
            done = quux_namespace_import(context, target, imported);
        }
    }
    if (done) {
        context->current_namespace = target;
        *result = quux_nothing;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- The evaluator ---- */

bool quux_evaluate(quux_context *context, quux_value code, quux_value environment,
                   quux_value *result) {
    if (!enter(context)) {
        return false;
    }
    quux_value value = quux_nothing;
    quux_value operator_value = quux_nothing;
    quux_value arguments = quux_nothing;
    quux_value frame = quux_nothing;
    QUUX_ROOT_FRAME(context, &code, &environment, &value, &operator_value, &arguments, &frame);
    bool done = true;
    for (;;) {
        quux_object *node = quux_checked_object(code);
        switch (quux_code_operation(node)) {
        case QUUX_OPERATION_CONSTANT:
            value = node->values[0];
            goto finished;
        case QUUX_OPERATION_LOCAL: {
            quux_value scope = environment;
            for (uint32_t i = quux_code_field(node, 0); i > 0; i--) {
                scope = quux_checked_object(scope)->values[QUUX_ENVIRONMENT_PARENT];
            }
            value = quux_checked_object(scope)
                        ->values[QUUX_ENVIRONMENT_FIRST_SLOT + quux_code_field(node, 1)];
            goto finished;
        }
        case QUUX_OPERATION_GLOBAL:
            done = global_value(context, node, &value);
            goto finished;
        case QUUX_OPERATION_IF:
            done = quux_evaluate(context, node->values[0], environment, &value);
            if (!done) {
                goto finished;
            }
            code = quux_checked_object(code)->values[quux_is_truthy(value) ? 1 : 2];
            continue;
        case QUUX_OPERATION_SEQUENCE: {
            uint32_t count = node->value_count;
            for (uint32_t i = 0; i + 1 < count && done; i++) {
                done = quux_evaluate(context, quux_checked_object(code)->values[i], environment,
                                     &value);
            }
            if (!done) {
                goto finished;
            }
            code = quux_checked_object(code)->values[count - 1];
            continue;
        }
        case QUUX_OPERATION_LET: {
            done = make_frame(context, environment, quux_code_field(node, 0), &frame);
            uint32_t pairs = (quux_checked_object(code)->value_count - 1) / 2;
            for (uint32_t i = 0; i < pairs && done; i++) {
                done =
                    quux_evaluate(context, quux_checked_object(code)->values[2 * i + 1], frame,
                                  &value) &&
                    bind_pattern(context, quux_checked_object(code)->values[2 * i], value, frame);
            }
            if (!done) {
                goto finished;
            }
            environment = frame;
            code = quux_checked_object(code)->values[2 * pairs];
            continue;
        }
        case QUUX_OPERATION_LET_FUNCTIONS: {
            uint32_t count = quux_code_field(node, 0);
            done = make_frame(context, environment, count, &frame);
            for (uint32_t i = 0; i < count && done; i++) {
                quux_value function_code = quux_checked_object(code)->values[i];
                done = make_closure(context, quux_checked_object(function_code)->values[0], frame,
                                    &value);
                if (done) {
                    store_slot(context, frame, i, value);
                }
            }
            if (!done) {
                goto finished;
            }
            environment = frame;
            code = quux_checked_object(code)->values[count];
            continue;
        }
        case QUUX_OPERATION_FUNCTION:
            done = make_closure(context, node->values[0], environment, &value);
            goto finished;
        case QUUX_OPERATION_DEFINE:
            done = evaluate_define(context, code, environment, &value);
            goto finished;
        case QUUX_OPERATION_CALL: {
            done = quux_evaluate(context, node->values[0], environment, &operator_value);
            uint32_t count = quux_checked_object(code)->value_count - 1;
            done = done && quux_make_arguments(context, count, &arguments);
            for (uint32_t i = 0; i < count && done; i++) {
                done = quux_evaluate(context, quux_checked_object(code)->values[i + 1], environment,
                                     &value);
                if (done) {
                    quux_checked_object(arguments)->values[i] = value;
                }
            }
            if (!done) {
                goto finished;
            }
            quux_kind kind = quux_kind_of(operator_value);
            if (kind == QUUX_KIND_PRIMITIVE) {
                done = call_primitive(context, operator_value, arguments, &value);
                goto finished;
            }
            if (kind != QUUX_KIND_CLOSURE) {
                done = quux_raise(context, "wrong-type", "%s is not a function",
                                  quux_kind_phrase(kind));
                goto finished;
            }
            /* A proper tail call: replace the code and environment, keep the C frame. */
            done = bind_parameters(context, operator_value, arguments, &frame);
            if (!done) {
                goto finished;
            }
            environment = frame;
            code = quux_checked_object(
                       quux_checked_object(operator_value)->values[QUUX_CLOSURE_LAMBDA])
                       ->values[QUUX_LAMBDA_BODY];
            continue;
        }
        case QUUX_OPERATION_TRY:
            done = evaluate_try(context, code, environment, &value);
            goto finished;
        case QUUX_OPERATION_RAISE:
            done = quux_evaluate(context, node->values[0], environment, &value);
            if (done) {
                done = quux_is_kind(value, QUUX_KIND_ERROR)
                           ? quux_raise_value(context, value)
                           : quux_raise(context, "wrong-type", "raise expects an error, got %s",
                                        quux_kind_phrase(quux_kind_of(value)));
            }
            goto finished;
        case QUUX_OPERATION_BUILD_LIST:
        case QUUX_OPERATION_BUILD_VECTOR:
        case QUUX_OPERATION_BUILD_SET:
        case QUUX_OPERATION_BUILD_MAP:
            done = build_collection(context, code, environment, &value);
            goto finished;
        case QUUX_OPERATION_NAMESPACE:
            done = evaluate_namespace(context, code, &value);
            goto finished;
        default:
            done = quux_raise(context, "wrong-type", "cannot evaluate this code");
            goto finished;
        }
    }
finished:
    if (done) {
        *result = value;
    }
    QUUX_ROOT_FRAME_END(context);
    context->recursion_depth--;
    return done;
}

bool quux_evaluate_form(quux_context *context, quux_value form, quux_value *result) {
    quux_value code = quux_nothing;
    QUUX_ROOT_FRAME(context, &form, &code);
    bool done =
        quux_analyze(context, form, &code) && quux_evaluate(context, code, quux_nothing, result);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_evaluate_text(quux_context *context, const char *text, size_t length,
                        quux_value *result) {
    quux_reader reader;
    quux_reader_initialize(&reader, text, length);
    quux_value form = quux_nothing;
    quux_value value = quux_nothing;
    QUUX_ROOT_FRAME(context, &form, &value);
    bool done = true;
    for (;;) {
        quux_read_status status = quux_reader_read(context, &reader, &form);
        if (status == QUUX_READ_END) {
            break;
        }
        if (status == QUUX_READ_INCOMPLETE) {
            done = quux_raise(context, "syntax", "line %zu: unexpected end of text", reader.line);
            break;
        }
        if (status == QUUX_READ_ERROR || !quux_evaluate_form(context, form, &value)) {
            done = false;
            break;
        }
    }
    if (done) {
        *result = value;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- Set-up ---- */

static bool apply_macro(quux_context *context, quux_value macro_function, quux_value forms,
                        quux_value *result) {
    quux_value arguments = quux_nothing;
    QUUX_ROOT_FRAME(context, &macro_function, &forms, &arguments);
    uint64_t count = quux_list_count(forms);
    bool done = quux_make_arguments(context, (uint32_t)count, &arguments);
    uint32_t index = 0;
    for (quux_value cell = forms; done && !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
        quux_checked_object(arguments)->values[index++] = quux_pair_first(cell);
    }
    done = done && quux_apply(context, macro_function, arguments, result);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool name_function(quux_value function, quux_text *text) {
    quux_object *object = quux_checked_object(function);
    quux_value name = quux_nothing;
    if (object->kind == QUUX_KIND_PRIMITIVE) {
        name = object->values[QUUX_PRIMITIVE_NAME];
    } else if (object->kind == QUUX_KIND_CLOSURE) {
        name = quux_checked_object(object->values[QUUX_CLOSURE_LAMBDA])->values[QUUX_LAMBDA_NAME];
    }
    if (quux_is_nothing(name)) {
        return false;
    }
    quux_text_append(text, quux_name_bytes(name), quux_name_length(name));
    return true;
}

bool quux_make_primitive(quux_context *context, const char *name, quux_primitive_function function,
                         uint32_t minimum, uint32_t maximum, quux_value *result) {
    quux_value symbol = quux_nothing;
    QUUX_ROOT_FRAME(context, &symbol);
    bool done = quux_intern_symbol(context, name, strlen(name), &symbol);
    quux_object *object =
        done ? quux_allocate(context, QUUX_KIND_PRIMITIVE, 1, sizeof(primitive_shape)) : NULL;
    done = done && object != NULL;
    if (done) {
        object->values[QUUX_PRIMITIVE_NAME] = symbol;
        primitive_shape shape = {function, minimum, maximum};
        memcpy(quux_object_bytes(object), &shape, sizeof shape);
        *result = quux_object_value(object);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_define_primitive(quux_context *context, const char *name,
                           quux_primitive_function function, uint32_t minimum, uint32_t maximum) {
    quux_value primitive = quux_nothing;
    quux_value definition = quux_nothing;
    QUUX_ROOT_FRAME(context, &primitive, &definition);
    bool done = quux_make_primitive(context, name, function, minimum, maximum, &primitive) &&
                quux_namespace_define(context, quux_namespace_core(context),
                                      quux_checked_object(primitive)->values[QUUX_PRIMITIVE_NAME],
                                      primitive, quux_nothing, 0, &definition);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_evaluator_initialize(quux_context *context) {
    quux_expander_set_macro_caller(apply_macro);
    quux_printer_set_function_namer(name_function);
    quux_encoding_install_function_hooks();
    if (!quux_namespaces_initialize(context) || !quux_core_functions_register(context)) {
        return false;
    }
    quux_value ignored = quux_nothing;
    quux_value user = quux_nothing;
    QUUX_ROOT_FRAME(context, &ignored, &user);
    context->current_namespace = quux_namespace_core(context);
    bool done = quux_evaluate_text(context, (const char *)quux_core_prelude_text,
                                   sizeof quux_core_prelude_text, &ignored) &&
                quux_namespace_find_or_create(context, quux_symbol(context, "user"), &user);
    if (done) {
        context->current_namespace = user;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}
