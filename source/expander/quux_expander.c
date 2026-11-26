#include "quux_expander.h"

#include "quux_collections.h"
#include "quux_namespaces.h"

#include <stdlib.h>
#include <string.h>

static quux_macro_caller macro_caller;

void quux_expander_set_macro_caller(quux_macro_caller caller) {
    macro_caller = caller;
}

quux_lambda_shape quux_lambda_read_shape(quux_object *lambda) {
    quux_lambda_shape shape;
    memcpy(&shape, quux_object_bytes(lambda), sizeof shape);
    return shape;
}

void quux_lambda_free_reference(quux_object *lambda, uint32_t index, uint32_t *depth,
                                uint32_t *slot) {
    const unsigned char *pairs = quux_object_bytes(lambda) + sizeof(quux_lambda_shape);
    memcpy(depth, pairs + 8 * index, 4);
    memcpy(slot, pairs + 8 * index + 4, 4);
}

/* ---- Compile-time scopes ---- */

typedef struct lambda_builder {
    uint32_t *free_pairs; /* depth, slot, depth, slot ... */
    uint32_t free_count;
    uint32_t capacity;
    bool failed;
} lambda_builder;

/* One scope frame per run-time environment frame. The names are symbols that stay reachable
 * through the form being analyzed, which the caller roots. */
typedef struct scope_frame {
    struct scope_frame *parent;
    quux_value *names;
    uint32_t count;
    uint32_t capacity;
    lambda_builder *lambda; /* set on a function's parameter frame */
} scope_frame;

typedef struct analyzer {
    quux_context *context;
    scope_frame *scope;
} analyzer;

static void open_frame(analyzer *state, scope_frame *frame, lambda_builder *lambda) {
    frame->parent = state->scope;
    frame->names = NULL;
    frame->count = 0;
    frame->capacity = 0;
    frame->lambda = lambda;
    state->scope = frame;
}

static void close_frame(analyzer *state, scope_frame *frame) {
    free(frame->names);
    state->scope = frame->parent;
}

static bool add_name(analyzer *state, quux_value name, uint32_t *slot) {
    scope_frame *frame = state->scope;
    if (frame->count == frame->capacity) {
        uint32_t capacity = frame->capacity == 0 ? 8 : frame->capacity * 2;
        quux_value *grown = realloc(frame->names, capacity * sizeof(quux_value));
        if (grown == NULL) {
            return quux_raise(state->context, "resource-exhausted", "out of memory in analysis");
        }
        frame->names = grown;
        frame->capacity = capacity;
    }
    frame->names[frame->count] = name;
    *slot = frame->count++;
    return true;
}

static void record_free_reference(lambda_builder *lambda, uint32_t depth, uint32_t slot) {
    for (uint32_t i = 0; i < lambda->free_count; i++) {
        if (lambda->free_pairs[2 * i] == depth && lambda->free_pairs[2 * i + 1] == slot) {
            return;
        }
    }
    if (lambda->free_count == lambda->capacity) {
        uint32_t capacity = lambda->capacity == 0 ? 8 : lambda->capacity * 2;
        uint32_t *grown = realloc(lambda->free_pairs, 2 * capacity * sizeof(uint32_t));
        if (grown == NULL) {
            lambda->failed = true;
            return;
        }
        lambda->free_pairs = grown;
        lambda->capacity = capacity;
    }
    lambda->free_pairs[2 * lambda->free_count] = depth;
    lambda->free_pairs[2 * lambda->free_count + 1] = slot;
    lambda->free_count++;
}

enum { QUUX_MAXIMUM_LAMBDA_NESTING = 256 };

static bool lookup_local(analyzer *state, quux_value symbol, uint32_t *depth, uint32_t *slot) {
    lambda_builder *passed[QUUX_MAXIMUM_LAMBDA_NESTING];
    uint32_t passed_depth[QUUX_MAXIMUM_LAMBDA_NESTING];
    uint32_t passed_count = 0;
    uint32_t current = 0;
    for (scope_frame *frame = state->scope; frame != NULL; frame = frame->parent, current++) {
        for (uint32_t i = frame->count; i > 0; i--) {
            if (frame->names[i - 1] == symbol) {
                *depth = current;
                *slot = i - 1;
                for (uint32_t j = 0; j < passed_count; j++) {
                    record_free_reference(passed[j], current - passed_depth[j] - 1, i - 1);
                }
                return true;
            }
        }
        if (frame->lambda != NULL && passed_count < QUUX_MAXIMUM_LAMBDA_NESTING) {
            passed[passed_count] = frame->lambda;
            passed_depth[passed_count] = current;
            passed_count++;
        }
    }
    return false;
}

/* ---- Code nodes ---- */

static quux_object *new_code(quux_context *context, quux_operation operation,
                             uint32_t value_count) {
    quux_object *code = quux_allocate(context, QUUX_KIND_CODE, value_count, 16);
    if (code != NULL) {
        uint32_t fields[4] = {(uint32_t)operation, 0, 0, 0};
        memcpy(quux_object_bytes(code), fields, sizeof fields);
    }
    return code;
}

static void set_code_field(quux_object *code, int index, uint32_t value) {
    memcpy(quux_object_bytes(code) + 4 * (1 + index), &value, sizeof value);
}

static bool make_constant(quux_context *context, quux_value value, quux_value *result) {
    QUUX_ROOT_FRAME(context, &value);
    quux_object *code = new_code(context, QUUX_OPERATION_CONSTANT, 1);
    QUUX_ROOT_FRAME_END(context);
    if (code == NULL) {
        return false;
    }
    code->values[0] = value;
    *result = quux_object_value(code);
    return true;
}

static bool is_constant_code(quux_value code) {
    return quux_code_operation(quux_checked_object(code)) == QUUX_OPERATION_CONSTANT;
}

static bool malformed(quux_context *context, const char *form_name, const char *problem) {
    return quux_raise(context, "wrong-type", "malformed %s: %s", form_name, problem);
}

/* ---- Special forms ---- */

typedef enum special_form {
    SPECIAL_NONE,
    SPECIAL_QUOTE,
    SPECIAL_IF,
    SPECIAL_SEQUENCE,
    SPECIAL_LET,
    SPECIAL_LET_FUNCTIONS,
    SPECIAL_FUNCTION,
    SPECIAL_DEFINE,
    SPECIAL_DEFINE_MACRO,
    SPECIAL_TRY,
    SPECIAL_RAISE,
    SPECIAL_QUASI_QUOTE,
    SPECIAL_UNQUOTE,
    SPECIAL_UNQUOTE_SPLICING,
    SPECIAL_NAMESPACE,
} special_form;

static const struct {
    const char *name;
    special_form form;
} special_forms[] = {
    {"quote", SPECIAL_QUOTE},
    {"if", SPECIAL_IF},
    {"sequence", SPECIAL_SEQUENCE},
    {"let", SPECIAL_LET},
    {"let-functions", SPECIAL_LET_FUNCTIONS},
    {"function", SPECIAL_FUNCTION},
    {"define", SPECIAL_DEFINE},
    {"define-macro", SPECIAL_DEFINE_MACRO},
    {"try", SPECIAL_TRY},
    {"raise", SPECIAL_RAISE},
    {"quasi-quote", SPECIAL_QUASI_QUOTE},
    {"unquote", SPECIAL_UNQUOTE},
    {"unquote-splicing", SPECIAL_UNQUOTE_SPLICING},
    {"namespace", SPECIAL_NAMESPACE},
};

static special_form special_form_of(quux_value symbol) {
    if (!quux_is_kind(symbol, QUUX_KIND_SYMBOL)) {
        return SPECIAL_NONE;
    }
    const char *text = quux_name_bytes(symbol);
    size_t length = quux_name_length(symbol);
    size_t namespace_length = quux_symbol_namespace_length(symbol);
    if (namespace_length > 0) {
        if (namespace_length != 4 || memcmp(text, "core", 4) != 0) {
            return SPECIAL_NONE;
        }
        text += 5;
        length -= 5;
    }
    for (size_t i = 0; i < sizeof special_forms / sizeof special_forms[0]; i++) {
        if (strlen(special_forms[i].name) == length &&
            memcmp(special_forms[i].name, text, length) == 0) {
            return special_forms[i].form;
        }
    }
    return SPECIAL_NONE;
}

static bool analyze(analyzer *state, quux_value form, quux_value *result);

/* Analyzes a body (zero or more forms in a list) into one expression. */
static bool analyze_body(analyzer *state, quux_value forms, quux_value *result) {
    quux_context *context = state->context;
    uint64_t count = quux_list_count(forms);
    if (count == 0) {
        return make_constant(context, quux_nothing, result);
    }
    if (count == 1) {
        return analyze(state, quux_pair_first(forms), result);
    }
    quux_value node = quux_nothing;
    QUUX_ROOT_FRAME(context, &forms, &node);
    quux_object *code = new_code(context, QUUX_OPERATION_SEQUENCE, (uint32_t)count);
    bool done = code != NULL;
    if (done) {
        node = quux_object_value(code);
    }
    uint32_t index = 0;
    for (quux_value cell = forms; done && !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
        quux_value element = quux_nothing;
        done = analyze(state, quux_pair_first(cell), &element);
        if (done) {
            quux_checked_object(node)->values[index++] = element;
        }
    }
    if (done) {
        *result = node;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* Binds the names of a pattern in the current frame and returns the pattern code. */
static bool bind_pattern(analyzer *state, quux_value pattern, quux_value *result) {
    quux_context *context = state->context;
    quux_kind kind = quux_kind_of(pattern);
    if (kind == QUUX_KIND_SYMBOL) {
        if (quux_symbol_namespace_length(pattern) > 0) {
            return quux_raise(context, "wrong-type",
                              "malformed binding: %.*s is qualified; a macro binds a local name "
                              "made by unique-symbol (section 04 rule 5)",
                              (int)quux_name_length(pattern), quux_name_bytes(pattern));
        }
        uint32_t slot = 0;
        if (!add_name(state, pattern, &slot)) {
            return false;
        }
        quux_object *code = new_code(context, QUUX_OPERATION_PATTERN_NAME, 0);
        if (code == NULL) {
            return false;
        }
        set_code_field(code, 0, slot);
        *result = quux_object_value(code);
        return true;
    }
    quux_value node = quux_nothing;
    quux_value element = quux_nothing;
    quux_value list = quux_nothing;
    bool done = true;
    QUUX_ROOT_FRAME(context, &pattern, &node, &element, &list);
    if (kind == QUUX_KIND_VECTOR) {
        uint64_t count = quux_vector_count(pattern);
        bool has_rest = false;
        uint32_t parts = 0;
        for (uint64_t i = 0; i < count && done; i++) {
            quux_value item = quux_vector_element(pattern, i);
            if (quux_is_kind(item, QUUX_KIND_KEYWORD) && quux_name_equals(item, "rest")) {
                if (has_rest || i + 2 != count) {
                    done = malformed(context, "pattern", ":rest must precede exactly one pattern");
                }
                has_rest = true;
            } else {
                parts++;
            }
        }
        quux_object *code = done ? new_code(context, QUUX_OPERATION_PATTERN_VECTOR, parts) : NULL;
        done = done && code != NULL;
        if (done) {
            set_code_field(code, 0, has_rest ? 1 : 0);
            node = quux_object_value(code);
        }
        uint32_t index = 0;
        for (uint64_t i = 0; i < count && done; i++) {
            quux_value item = quux_vector_element(pattern, i);
            if (quux_is_kind(item, QUUX_KIND_KEYWORD) && quux_name_equals(item, "rest")) {
                continue;
            }
            done = bind_pattern(state, item, &element);
            if (done) {
                quux_checked_object(node)->values[index++] = element;
            }
        }
    } else if (kind == QUUX_KIND_MAP) {
        /* {target key ...}: canonical iteration order fixes the slot order. */
        done = quux_as_list(context, pattern, &list);
        uint64_t count = quux_map_count(pattern);
        quux_object *code =
            done ? new_code(context, QUUX_OPERATION_PATTERN_MAP, (uint32_t)(2 * count)) : NULL;
        done = done && code != NULL;
        if (done) {
            node = quux_object_value(code);
        }
        uint32_t index = 0;
        for (quux_value cell = list; done && !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
            quux_value entry = quux_pair_first(cell);
            quux_checked_object(node)->values[index] = quux_vector_element(entry, 1);
            done = bind_pattern(state, quux_vector_element(entry, 0), &element);
            if (done) {
                quux_checked_object(node)->values[index + 1] = element;
                index += 2;
            }
        }
    } else {
        done = malformed(context, "binding", "expected a name, a vector pattern or a map pattern");
    }
    if (done) {
        *result = node;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* (function [parameters] body...) -> FUNCTION code holding a new lambda. */
static bool analyze_lambda(analyzer *state, quux_value parameters, quux_value body, quux_value name,
                           quux_value *result) {
    quux_context *context = state->context;
    if (!quux_is_kind(parameters, QUUX_KIND_VECTOR)) {
        return malformed(context, "function", "parameters must be a vector");
    }
    uint64_t count = quux_vector_count(parameters);
    uint32_t required = 0;
    bool has_rest = false;
    for (uint64_t i = 0; i < count; i++) {
        quux_value item = quux_vector_element(parameters, i);
        if (quux_is_kind(item, QUUX_KIND_KEYWORD) && quux_name_equals(item, "rest")) {
            if (has_rest || i + 2 != count) {
                return malformed(context, "function", ":rest must precede the last parameter");
            }
            has_rest = true;
        } else if (!has_rest) {
            required++;
        }
    }
    lambda_builder builder = {NULL, 0, 0, false};
    scope_frame frame;
    open_frame(state, &frame, &builder);
    quux_value holder_value = quux_nothing;
    quux_value element = quux_nothing;
    quux_value body_code = quux_nothing;
    quux_value lambda_value = quux_nothing;
    QUUX_ROOT_FRAME(context, &parameters, &body, &name, &holder_value, &element, &body_code,
                    &lambda_value);
    /* holder: [rest pattern, parameter patterns...] while the body is analyzed */
    quux_object *holder = quux_allocate(context, QUUX_KIND_INTERNAL, 1 + required, 0);
    bool done = holder != NULL;
    if (done) {
        holder_value = quux_object_value(holder);
    }
    uint32_t index = 0;
    for (uint64_t i = 0; i < count && done; i++) {
        quux_value item = quux_vector_element(parameters, i);
        if (quux_is_kind(item, QUUX_KIND_KEYWORD) && quux_name_equals(item, "rest")) {
            continue;
        }
        done = bind_pattern(state, item, &element);
        if (done) {
            if (index < required) {
                quux_checked_object(holder_value)->values[1 + index] = element;
            } else {
                quux_checked_object(holder_value)->values[0] = element;
            }
            index++;
        }
    }
    done = done && analyze_body(state, body, &body_code);
    uint32_t frame_size = frame.count;
    close_frame(state, &frame);
    if (done && builder.failed) {
        done = quux_raise(context, "resource-exhausted", "out of memory in analysis");
    }
    quux_object *lambda = NULL;
    if (done) {
        uint32_t byte_count = (uint32_t)(sizeof(quux_lambda_shape) + 8 * builder.free_count);
        lambda =
            quux_allocate(context, QUUX_KIND_LAMBDA, QUUX_LAMBDA_PARAMETERS + required, byte_count);
        done = lambda != NULL;
    }
    if (done) {
        quux_object *filled = quux_checked_object(holder_value);
        lambda->values[QUUX_LAMBDA_BODY] = body_code;
        lambda->values[QUUX_LAMBDA_NAME] = name;
        lambda->values[QUUX_LAMBDA_REST] = filled->values[0];
        for (uint32_t i = 0; i < required; i++) {
            lambda->values[QUUX_LAMBDA_PARAMETERS + i] = filled->values[1 + i];
        }
        quux_lambda_shape shape = {required, has_rest ? 1u : 0u, frame_size, builder.free_count};
        memcpy(quux_object_bytes(lambda), &shape, sizeof shape);
        if (builder.free_count > 0) {
            memcpy(quux_object_bytes(lambda) + sizeof shape, builder.free_pairs,
                   8 * builder.free_count);
        }
        lambda_value = quux_object_value(lambda);
        quux_object *code = new_code(context, QUUX_OPERATION_FUNCTION, 1);
        done = code != NULL;
        if (done) {
            code->values[0] = lambda_value;
            *result = quux_object_value(code);
        }
    }
    free(builder.free_pairs);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static uint64_t argument_count(quux_value form) {
    return quux_list_count(form) - 1;
}

static quux_value argument(quux_value form, uint64_t index) {
    quux_value cell = quux_pair_rest(form);
    for (uint64_t i = 0; i < index; i++) {
        cell = quux_pair_rest(cell);
    }
    return quux_pair_first(cell);
}

static quux_value arguments_after(quux_value form, uint64_t index) {
    quux_value cell = quux_pair_rest(form);
    for (uint64_t i = 0; i < index; i++) {
        cell = quux_pair_rest(cell);
    }
    return cell;
}

/* A node with `count` values, each the analysis of one form of `forms`. */
static bool analyze_into(analyzer *state, quux_operation operation, quux_value forms,
                         uint32_t extra_before, quux_value *node_value) {
    quux_context *context = state->context;
    uint64_t count = quux_list_count(forms);
    quux_value element = quux_nothing;
    quux_value node = quux_nothing; /* the caller's result slot may not be rooted */
    QUUX_ROOT_FRAME(context, &forms, &element, &node);
    quux_object *code = new_code(context, operation, (uint32_t)count + extra_before);
    bool done = code != NULL;
    if (done) {
        node = quux_object_value(code);
    }
    uint32_t index = extra_before;
    for (quux_value cell = forms; done && !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
        done = analyze(state, quux_pair_first(cell), &element);
        if (done) {
            quux_checked_object(node)->values[index++] = element;
        }
    }
    if (done) {
        *node_value = node;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- quasi-quote ---- */

static bool is_form_named(quux_value form, special_form special) {
    return quux_is_kind(form, QUUX_KIND_PAIR) && special_form_of(quux_pair_first(form)) == special;
}

/* A free symbol in a template is qualified (section 04 rule 4): with the namespace where it
 * resolves, or, when it does not resolve yet, with the current namespace, where the macro is being
 * defined. Special forms stay as written. Before feature 003 an unresolved symbol stayed as
 * written, so a macro that bound a literal name worked or failed depending on what was defined
 * before it (friction P07-F3). */
static bool qualify_symbol(analyzer *state, quux_value symbol, quux_value *result) {
    quux_context *context = state->context;
    *result = symbol;
    if (quux_symbol_namespace_length(symbol) > 0 || special_form_of(symbol) != SPECIAL_NONE) {
        return true;
    }
    quux_value definition = quux_nothing;
    QUUX_ROOT_FRAME(context, &symbol, &definition);
    bool done = quux_namespace_resolve(context, context->current_namespace, symbol, &definition);
    quux_value namespace_value = context->current_namespace;
    if (done && !quux_is_nothing(definition)) {
        namespace_value = quux_definition_field(definition, QUUX_DEFINITION_NAMESPACE);
    }
    if (done && !quux_is_nothing(namespace_value)) {
        quux_value namespace_name =
            quux_checked_object(namespace_value)->values[QUUX_NAMESPACE_NAME];
        size_t prefix = quux_name_length(namespace_name);
        size_t length = quux_name_length(symbol);
        char *buffer = malloc(prefix + 1 + length);
        if (buffer == NULL) {
            done = quux_raise(context, "resource-exhausted", "out of memory in quasi-quote");
        } else {
            memcpy(buffer, quux_name_bytes(namespace_name), prefix);
            buffer[prefix] = '/';
            memcpy(buffer + prefix + 1, quux_name_bytes(symbol), length);
            done = quux_intern_symbol(context, buffer, prefix + 1 + length, result);
            free(buffer);
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool analyze_template(analyzer *state, quux_value template_form, quux_value *result);

static bool template_parts(analyzer *state, quux_operation operation, quux_value elements,
                           quux_value *result) {
    quux_context *context = state->context;
    uint64_t count = quux_list_count(elements);
    quux_value node = quux_nothing;
    quux_value element = quux_nothing;
    quux_value inner = quux_nothing;
    QUUX_ROOT_FRAME(context, &elements, &node, &element, &inner);
    quux_object *code = new_code(context, operation, (uint32_t)count);
    bool done = code != NULL;
    if (done) {
        node = quux_object_value(code);
    }
    uint32_t index = 0;
    for (quux_value cell = elements; done && !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
        quux_value item = quux_pair_first(cell);
        if (is_form_named(item, SPECIAL_UNQUOTE_SPLICING)) {
            if (argument_count(item) != 1) {
                done = malformed(context, "unquote-splicing", "expects one expression");
                break;
            }
            done = analyze(state, argument(item, 0), &inner);
            quux_object *splice = done ? new_code(context, QUUX_OPERATION_SPLICE, 1) : NULL;
            done = done && splice != NULL;
            if (done) {
                splice->values[0] = inner;
                element = quux_object_value(splice);
            }
        } else {
            done = analyze_template(state, item, &element);
        }
        if (done) {
            quux_checked_object(node)->values[index++] = element;
        }
    }
    if (done) {
        *result = node;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool analyze_template(analyzer *state, quux_value template_form, quux_value *result) {
    quux_context *context = state->context;
    quux_kind kind = quux_kind_of(template_form);
    quux_value list = quux_nothing;
    quux_value qualified = quux_nothing;
    quux_value entries = quux_nothing;
    bool done = true;
    QUUX_ROOT_FRAME(context, &template_form, &list, &qualified, &entries);
    switch (kind) {
    case QUUX_KIND_SYMBOL:
        done = qualify_symbol(state, template_form, &qualified) &&
               make_constant(context, qualified, result);
        break;
    case QUUX_KIND_PAIR:
        if (is_form_named(template_form, SPECIAL_UNQUOTE)) {
            done = argument_count(template_form) == 1
                       ? analyze(state, argument(template_form, 0), result)
                       : malformed(context, "unquote", "expects one expression");
        } else {
            done = template_parts(state, QUUX_OPERATION_BUILD_LIST, template_form, result);
        }
        break;
    case QUUX_KIND_VECTOR:
        done = quux_as_list(context, template_form, &list) &&
               template_parts(state, QUUX_OPERATION_BUILD_VECTOR, list, result);
        break;
    case QUUX_KIND_SET:
        done = quux_as_list(context, template_form, &list) &&
               template_parts(state, QUUX_OPERATION_BUILD_SET, list, result);
        break;
    case QUUX_KIND_MAP: {
        /* flatten [k v] entries into k v k v */
        done = quux_as_list(context, template_form, &entries);
        for (quux_value cell = entries; done && !quux_is_nothing(cell);
             cell = quux_pair_rest(cell)) {
            quux_value entry = quux_pair_first(cell);
            done = quux_list_prepend(context, quux_vector_element(entry, 0), list, &list) &&
                   quux_list_prepend(context, quux_vector_element(entry, 1), list, &list);
        }
        done = done && quux_list_reverse(context, list, &list) &&
               template_parts(state, QUUX_OPERATION_BUILD_MAP, list, result);
        break;
    }
    default:
        done = make_constant(context, template_form, result);
        break;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- Literals ---- */

/* A vector, map or set literal: constant when every part is constant, otherwise built at run
 * time. */
static bool analyze_collection_literal(analyzer *state, quux_value form, quux_value *result) {
    quux_context *context = state->context;
    quux_kind kind = quux_kind_of(form);
    quux_value list = quux_nothing;
    quux_value entries = quux_nothing;
    quux_value node = quux_nothing;
    bool done = true;
    QUUX_ROOT_FRAME(context, &form, &list, &entries, &node);
    if (kind == QUUX_KIND_MAP) {
        done = quux_as_list(context, form, &entries);
        for (quux_value cell = entries; done && !quux_is_nothing(cell);
             cell = quux_pair_rest(cell)) {
            quux_value entry = quux_pair_first(cell);
            done = quux_list_prepend(context, quux_vector_element(entry, 0), list, &list) &&
                   quux_list_prepend(context, quux_vector_element(entry, 1), list, &list);
        }
        done = done && quux_list_reverse(context, list, &list);
    } else {
        done = quux_as_list(context, form, &list);
    }
    quux_operation operation = kind == QUUX_KIND_MAP   ? QUUX_OPERATION_BUILD_MAP
                               : kind == QUUX_KIND_SET ? QUUX_OPERATION_BUILD_SET
                                                       : QUUX_OPERATION_BUILD_VECTOR;
    done = done && analyze_into(state, operation, list, 0, &node);
    if (done) {
        quux_object *code = quux_checked_object(node);
        bool all_constant = true;
        for (uint32_t i = 0; i < code->value_count && all_constant; i++) {
            all_constant = is_constant_code(code->values[i]);
        }
        if (all_constant) {
            done = make_constant(context, form, result);
        } else {
            *result = node;
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- Special form analysis ---- */

static bool analyze_let(analyzer *state, quux_value form, quux_value *result) {
    quux_context *context = state->context;
    if (argument_count(form) < 1 || !quux_is_kind(argument(form, 0), QUUX_KIND_VECTOR) ||
        quux_vector_count(argument(form, 0)) % 2 != 0) {
        return malformed(context, "let", "expected (let [name value ...] body ...)");
    }
    quux_value bindings = argument(form, 0);
    uint32_t pairs = (uint32_t)(quux_vector_count(bindings) / 2);
    scope_frame frame;
    open_frame(state, &frame, NULL);
    quux_value node = quux_nothing;
    quux_value element = quux_nothing;
    QUUX_ROOT_FRAME(context, &form, &bindings, &node, &element);
    quux_object *code = new_code(context, QUUX_OPERATION_LET, 2 * pairs + 1);
    bool done = code != NULL;
    if (done) {
        node = quux_object_value(code);
    }
    for (uint32_t i = 0; i < pairs && done; i++) {
        done = analyze(state, quux_vector_element(bindings, 2 * i + 1), &element);
        if (done) {
            quux_checked_object(node)->values[2 * i + 1] = element;
            done = bind_pattern(state, quux_vector_element(bindings, 2 * i), &element);
        }
        if (done) {
            quux_checked_object(node)->values[2 * i] = element;
        }
    }
    done = done && analyze_body(state, arguments_after(form, 1), &element);
    if (done) {
        quux_checked_object(node)->values[2 * pairs] = element;
        set_code_field(quux_checked_object(node), 0, frame.count);
        *result = node;
    }
    close_frame(state, &frame);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool analyze_let_functions(analyzer *state, quux_value form, quux_value *result) {
    quux_context *context = state->context;
    if (argument_count(form) < 1 || !quux_is_kind(argument(form, 0), QUUX_KIND_VECTOR)) {
        return malformed(context, "let-functions",
                         "expected (let-functions [(name [parameters] body ...) ...] body ...)");
    }
    quux_value specifications = argument(form, 0);
    uint32_t count = (uint32_t)quux_vector_count(specifications);
    scope_frame frame;
    open_frame(state, &frame, NULL);
    quux_value node = quux_nothing;
    quux_value element = quux_nothing;
    QUUX_ROOT_FRAME(context, &form, &specifications, &node, &element);
    bool done = true;
    for (uint32_t i = 0; i < count && done; i++) {
        quux_value specification = quux_vector_element(specifications, i);
        uint32_t slot = 0;
        if (!quux_is_kind(specification, QUUX_KIND_PAIR) || quux_list_count(specification) < 2 ||
            !quux_is_kind(quux_pair_first(specification), QUUX_KIND_SYMBOL)) {
            done = malformed(context, "let-functions",
                             "each function is (name [parameters] body ...)");
        } else {
            done = add_name(state, quux_pair_first(specification), &slot);
        }
    }
    quux_object *code = done ? new_code(context, QUUX_OPERATION_LET_FUNCTIONS, count + 1) : NULL;
    done = done && code != NULL;
    if (done) {
        node = quux_object_value(code);
        set_code_field(code, 0, count);
    }
    for (uint32_t i = 0; i < count && done; i++) {
        quux_value specification = quux_vector_element(specifications, i);
        quux_value rest = quux_pair_rest(specification);
        done = analyze_lambda(state, quux_pair_first(rest), quux_pair_rest(rest),
                              quux_pair_first(specification), &element);
        if (done) {
            quux_checked_object(node)->values[i] = element;
        }
    }
    done = done && analyze_body(state, arguments_after(form, 1), &element);
    if (done) {
        quux_checked_object(node)->values[count] = element;
        *result = node;
    }
    close_frame(state, &frame);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool analyze_define(analyzer *state, quux_value form, bool is_macro, quux_value *result) {
    quux_context *context = state->context;
    const char *form_name = is_macro ? "define-macro" : "define";
    uint64_t count = argument_count(form);
    if (count < 1 || !quux_is_kind(argument(form, 0), QUUX_KIND_SYMBOL) ||
        (!is_macro && count != 2) || (is_macro && count < 2)) {
        return malformed(context, form_name,
                         is_macro ? "expected (define-macro name [parameters] body ...)"
                                  : "expected (define name value)");
    }
    quux_value name = argument(form, 0);
    quux_value expression = quux_nothing;
    QUUX_ROOT_FRAME(context, &form, &name, &expression);
    bool done;
    if (is_macro) {
        done =
            analyze_lambda(state, argument(form, 1), arguments_after(form, 2), name, &expression);
    } else {
        done = analyze(state, argument(form, 1), &expression);
        if (done) {
            quux_object *code = quux_checked_object(expression);
            if (quux_code_operation(code) == QUUX_OPERATION_FUNCTION) {
                quux_object *lambda = quux_checked_object(code->values[0]);
                if (quux_is_nothing(lambda->values[QUUX_LAMBDA_NAME])) {
                    lambda->values[QUUX_LAMBDA_NAME] = name;
                }
            }
        }
    }
    quux_object *node = done ? new_code(context, QUUX_OPERATION_DEFINE, 2) : NULL;
    done = done && node != NULL;
    if (done) {
        node->values[0] = name;
        node->values[1] = expression;
        set_code_field(node, 0, is_macro ? QUUX_DEFINITION_FLAG_MACRO : 0);
        *result = quux_object_value(node);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool analyze_try(analyzer *state, quux_value form, quux_value *result) {
    quux_context *context = state->context;
    quux_value body_forms = quux_nothing;
    quux_value clauses = quux_nothing;
    quux_value finally_forms = quux_nothing;
    quux_value node = quux_nothing;
    quux_value element = quux_nothing;
    bool has_finally = false;
    bool done = true;
    QUUX_ROOT_FRAME(context, &form, &body_forms, &clauses, &finally_forms, &node, &element);
    uint32_t catch_count = 0;
    for (quux_value cell = quux_pair_rest(form); done && !quux_is_nothing(cell);
         cell = quux_pair_rest(cell)) {
        quux_value item = quux_pair_first(cell);
        bool is_catch = quux_is_kind(item, QUUX_KIND_PAIR) &&
                        quux_is_kind(quux_pair_first(item), QUUX_KIND_SYMBOL) &&
                        quux_name_equals(quux_pair_first(item), "catch");
        bool is_finally = quux_is_kind(item, QUUX_KIND_PAIR) &&
                          quux_is_kind(quux_pair_first(item), QUUX_KIND_SYMBOL) &&
                          quux_name_equals(quux_pair_first(item), "finally");
        if (is_catch) {
            if (has_finally || quux_list_count(item) < 3 ||
                !quux_is_kind(argument(item, 1), QUUX_KIND_SYMBOL)) {
                done = malformed(context, "try",
                                 "expected (catch kind-or-predicate name handler ...)");
            } else {
                done = quux_list_prepend(context, item, clauses, &clauses);
                catch_count++;
            }
        } else if (is_finally) {
            if (has_finally) {
                done = malformed(context, "try", "more than one finally");
            }
            has_finally = true;
            finally_forms = quux_pair_rest(item);
        } else if (catch_count > 0 || has_finally) {
            done = malformed(context, "try", "body forms must come before catch and finally");
        } else {
            done = quux_list_prepend(context, item, body_forms, &body_forms);
        }
    }
    done = done && quux_list_reverse(context, body_forms, &body_forms) &&
           quux_list_reverse(context, clauses, &clauses);
    quux_object *code = done ? new_code(context, QUUX_OPERATION_TRY, 2 + 2 * catch_count) : NULL;
    done = done && code != NULL;
    if (done) {
        node = quux_object_value(code);
        done = analyze_body(state, body_forms, &element);
    }
    if (done) {
        quux_checked_object(node)->values[0] = element;
        if (has_finally) {
            done = analyze_body(state, finally_forms, &element);
            if (done) {
                quux_checked_object(node)->values[1] = element;
            }
        }
    }
    uint32_t index = 2;
    for (quux_value cell = clauses; done && !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
        quux_value clause = quux_pair_first(cell);
        done = analyze(state, argument(clause, 0), &element);
        if (done) {
            quux_checked_object(node)->values[index] = element;
            scope_frame frame;
            open_frame(state, &frame, NULL);
            uint32_t slot = 0;
            done = add_name(state, argument(clause, 1), &slot) &&
                   analyze_body(state, arguments_after(clause, 2), &element);
            close_frame(state, &frame);
        }
        if (done) {
            quux_checked_object(node)->values[index + 1] = element;
            index += 2;
        }
    }
    if (done) {
        *result = node;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool analyze_namespace(analyzer *state, quux_value form, quux_value *result) {
    quux_context *context = state->context;
    if (argument_count(form) < 1 || !quux_is_kind(argument(form, 0), QUUX_KIND_SYMBOL)) {
        return malformed(context, "namespace", "expected (namespace name (:import other ...))");
    }
    quux_value imports = quux_nothing;
    quux_value node = quux_nothing;
    bool done = true;
    QUUX_ROOT_FRAME(context, &form, &imports, &node);
    for (quux_value cell = arguments_after(form, 1); done && !quux_is_nothing(cell);
         cell = quux_pair_rest(cell)) {
        quux_value clause = quux_pair_first(cell);
        if (!quux_is_kind(clause, QUUX_KIND_PAIR) ||
            !quux_is_kind(quux_pair_first(clause), QUUX_KIND_KEYWORD) ||
            !quux_name_equals(quux_pair_first(clause), "import")) {
            done = malformed(context, "namespace", "expected an (:import ...) clause");
            break;
        }
        for (quux_value name = quux_pair_rest(clause); done && !quux_is_nothing(name);
             name = quux_pair_rest(name)) {
            done = quux_list_prepend(context, quux_pair_first(name), imports, &imports);
        }
    }
    done = done && quux_list_reverse(context, imports, &imports);
    uint64_t count = quux_list_count(imports);
    quux_object *code =
        done ? new_code(context, QUUX_OPERATION_NAMESPACE, 1 + (uint32_t)count) : NULL;
    done = done && code != NULL;
    if (done) {
        code->values[0] = argument(form, 0);
        uint32_t index = 1;
        for (quux_value cell = imports; !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
            code->values[index++] = quux_pair_first(cell);
        }
        *result = quux_object_value(code);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool analyze_symbol(analyzer *state, quux_value symbol, quux_value *result) {
    quux_context *context = state->context;
    uint32_t depth = 0;
    uint32_t slot = 0;
    if (quux_symbol_namespace_length(symbol) == 0 && lookup_local(state, symbol, &depth, &slot)) {
        quux_object *code = new_code(context, QUUX_OPERATION_LOCAL, 0);
        if (code == NULL) {
            return false;
        }
        set_code_field(code, 0, depth);
        set_code_field(code, 1, slot);
        *result = quux_object_value(code);
        return true;
    }
    quux_value definition = quux_nothing;
    QUUX_ROOT_FRAME(context, &symbol, &definition);
    bool done = quux_namespace_resolve(context, context->current_namespace, symbol, &definition);
    quux_object *code = done ? new_code(context, QUUX_OPERATION_GLOBAL, 3) : NULL;
    done = done && code != NULL;
    if (done) {
        code->values[0] = symbol;
        code->values[1] = definition;
        code->values[2] = context->current_namespace;
        *result = quux_object_value(code);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool analyze_special(analyzer *state, special_form special, quux_value form,
                            quux_value *result) {
    quux_context *context = state->context;
    uint64_t count = argument_count(form);
    switch (special) {
    case SPECIAL_QUOTE:
        if (count != 1) {
            return malformed(context, "quote", "expects one form");
        }
        return make_constant(context, argument(form, 0), result);
    case SPECIAL_IF: {
        if (count != 2 && count != 3) {
            return malformed(context, "if", "expected (if test then else)");
        }
        quux_value node = quux_nothing;
        bool done = analyze_into(state, QUUX_OPERATION_IF, quux_pair_rest(form), 0, &node);
        if (done && count == 2) {
            /* a missing else is nothing */
            quux_value padded = quux_nothing;
            quux_value nothing_code = quux_nothing;
            QUUX_ROOT_FRAME(context, &node, &padded, &nothing_code);
            quux_object *code = new_code(context, QUUX_OPERATION_IF, 3);
            done = code != NULL && make_constant(context, quux_nothing, &nothing_code);
            if (done) {
                code->values[0] = quux_checked_object(node)->values[0];
                code->values[1] = quux_checked_object(node)->values[1];
                code->values[2] = nothing_code;
                node = quux_object_value(code);
            }
            QUUX_ROOT_FRAME_END(context);
        }
        if (done) {
            *result = node;
        }
        return done;
    }
    case SPECIAL_SEQUENCE:
        return analyze_body(state, quux_pair_rest(form), result);
    case SPECIAL_LET:
        return analyze_let(state, form, result);
    case SPECIAL_LET_FUNCTIONS:
        return analyze_let_functions(state, form, result);
    case SPECIAL_FUNCTION:
        if (count < 1) {
            return malformed(context, "function", "expected (function [parameters] body ...)");
        }
        return analyze_lambda(state, argument(form, 0), arguments_after(form, 1), quux_nothing,
                              result);
    case SPECIAL_DEFINE:
        return analyze_define(state, form, false, result);
    case SPECIAL_DEFINE_MACRO:
        return analyze_define(state, form, true, result);
    case SPECIAL_TRY:
        return analyze_try(state, form, result);
    case SPECIAL_RAISE:
        if (count != 1) {
            return malformed(context, "raise", "expects one error");
        }
        return analyze_into(state, QUUX_OPERATION_RAISE, quux_pair_rest(form), 0, result);
    case SPECIAL_QUASI_QUOTE:
        if (count != 1) {
            return malformed(context, "quasi-quote", "expects one template");
        }
        return analyze_template(state, argument(form, 0), result);
    case SPECIAL_UNQUOTE:
    case SPECIAL_UNQUOTE_SPLICING:
        return quux_raise(context, "wrong-type", "unquote outside quasi-quote");
    case SPECIAL_NAMESPACE:
        return analyze_namespace(state, form, result);
    default:
        return malformed(context, "form", "unknown special form");
    }
}

enum { MAXIMUM_EXPANSION_DEPTH = 1000 };

static bool analyze(analyzer *state, quux_value form, quux_value *result) {
    quux_context *context = state->context;
    quux_kind kind = quux_kind_of(form);
    if (kind == QUUX_KIND_SYMBOL) {
        return analyze_symbol(state, form, result);
    }
    if (kind == QUUX_KIND_VECTOR || kind == QUUX_KIND_MAP || kind == QUUX_KIND_SET) {
        return analyze_collection_literal(state, form, result);
    }
    if (kind != QUUX_KIND_PAIR) {
        return make_constant(context, form, result);
    }
    quux_value expanded = form;
    quux_value definition = quux_nothing;
    bool done = true;
    QUUX_ROOT_FRAME(context, &form, &expanded, &definition);
    for (int expansions = 0;; expansions++) {
        if (expansions > (int)MAXIMUM_EXPANSION_DEPTH) {
            done = quux_raise(context, "stack-exhausted", "macro expansion does not terminate");
            break;
        }
        if (!quux_is_kind(expanded, QUUX_KIND_PAIR)) {
            done = analyze(state, expanded, result);
            break;
        }
        quux_value head = quux_pair_first(expanded);
        uint32_t depth = 0;
        uint32_t slot = 0;
        bool head_is_local = quux_is_kind(head, QUUX_KIND_SYMBOL) &&
                             quux_symbol_namespace_length(head) == 0 &&
                             lookup_local(state, head, &depth, &slot);
        special_form special = head_is_local ? SPECIAL_NONE : special_form_of(head);
        if (special != SPECIAL_NONE) {
            done = analyze_special(state, special, expanded, result);
            break;
        }
        if (!head_is_local && quux_is_kind(head, QUUX_KIND_SYMBOL)) {
            done = quux_namespace_resolve(context, context->current_namespace, head, &definition);
            if (done && !quux_is_nothing(definition) &&
                (quux_definition_flags(definition) & QUUX_DEFINITION_FLAG_MACRO)) {
                done =
                    macro_caller != NULL &&
                    macro_caller(context, quux_definition_field(definition, QUUX_DEFINITION_VALUE),
                                 quux_pair_rest(expanded), &expanded);
                if (!done) {
                    break;
                }
                continue;
            }
            if (!done) {
                break;
            }
        }
        done = analyze_into(state, QUUX_OPERATION_CALL, expanded, 0, result);
        break;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_analyze(quux_context *context, quux_value form, quux_value *code) {
    analyzer state = {context, NULL};
    QUUX_ROOT_FRAME(context, &form);
    bool done = analyze(&state, form, code);
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_macro_expand(quux_context *context, quux_value form, quux_value *result) {
    quux_value definition = quux_nothing;
    QUUX_ROOT_FRAME(context, &form, &definition);
    bool done = true;
    while (done && quux_is_kind(form, QUUX_KIND_PAIR) &&
           quux_is_kind(quux_pair_first(form), QUUX_KIND_SYMBOL)) {
        done = quux_namespace_resolve(context, context->current_namespace, quux_pair_first(form),
                                      &definition);
        if (!done || quux_is_nothing(definition) ||
            !(quux_definition_flags(definition) & QUUX_DEFINITION_FLAG_MACRO)) {
            break;
        }
        done = macro_caller(context, quux_definition_field(definition, QUUX_DEFINITION_VALUE),
                            quux_pair_rest(form), &form);
    }
    if (done) {
        *result = form;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}
