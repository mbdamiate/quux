/* Reuse: keep. Macro expansion and analysis (D-06, D-38, D-39, section 04): turns a read form into
 * analyzed code, expanding macros completely from the outside in. Locals become (depth, index)
 * pairs, so parameter names never reach the content hash (D-19); patterns and `:rest` are
 * resolved here, without helper functions. The Phase 3 VM compiles from this code. */
#ifndef QUUX_EXPANDER_H
#define QUUX_EXPANDER_H

#include "quux_values.h"

/* ---- Analyzed code (KIND_CODE objects) ----
 * bytes: operation, then three uint32 fields (a, b, c). The values depend on the operation: */
typedef enum quux_operation {
    QUUX_OPERATION_CONSTANT,      /* v[0] value */
    QUUX_OPERATION_LOCAL,         /* a depth, b slot */
    QUUX_OPERATION_GLOBAL,        /* v[0] symbol, v[1] definition or nothing, v[2] namespace */
    QUUX_OPERATION_IF,            /* v[0] test, v[1] then, v[2] else */
    QUUX_OPERATION_SEQUENCE,      /* v[...] expressions (at least one) */
    QUUX_OPERATION_LET,           /* a frame size; v [pattern, expression]..., body */
    QUUX_OPERATION_LET_FUNCTIONS, /* a frame size; v [function]..., body */
    QUUX_OPERATION_FUNCTION,      /* v[0] lambda */
    QUUX_OPERATION_DEFINE,        /* a flags; v[0] name, v[1] expression */
    QUUX_OPERATION_CALL,          /* v[0] operator, v[1...] arguments */
    QUUX_OPERATION_TRY,           /* v[0] body, v[1] finally or nothing, v[2...] matcher, handler */
    QUUX_OPERATION_RAISE,         /* v[0] expression */
    QUUX_OPERATION_BUILD_LIST,    /* v parts (code or SPLICE) */
    QUUX_OPERATION_BUILD_VECTOR,
    QUUX_OPERATION_BUILD_SET,
    QUUX_OPERATION_BUILD_MAP,      /* v key, value, key, value ... */
    QUUX_OPERATION_SPLICE,         /* v[0] expression whose elements are inserted */
    QUUX_OPERATION_NAMESPACE,      /* v[0] name, v[1...] imported namespace names */
    QUUX_OPERATION_PATTERN_NAME,   /* a slot */
    QUUX_OPERATION_PATTERN_VECTOR, /* a 1 when the last part is a rest pattern; v sub-patterns */
    QUUX_OPERATION_PATTERN_MAP,    /* v key, sub-pattern, key, sub-pattern ... */
} quux_operation;

/* Lambda (KIND_LAMBDA): the code shared by every closure of one `function` form.
 * values: [body, name, rest pattern or nothing, parameter patterns...]
 * bytes: required count, has rest, frame size, free reference count, then pairs (depth, slot)
 * of the locals it captures (relative to the closure's environment). */
enum { QUUX_LAMBDA_BODY, QUUX_LAMBDA_NAME, QUUX_LAMBDA_REST, QUUX_LAMBDA_PARAMETERS };

typedef struct quux_lambda_shape {
    uint32_t required_count;
    uint32_t has_rest;
    uint32_t frame_size;
    uint32_t free_count;
} quux_lambda_shape;

static inline quux_operation quux_code_operation(quux_object *code) {
    uint32_t operation;
    __builtin_memcpy(&operation, quux_object_bytes(code), sizeof operation);
    return (quux_operation)operation;
}

static inline uint32_t quux_code_field(quux_object *code, int index) {
    uint32_t field;
    __builtin_memcpy(&field, quux_object_bytes(code) + 4 * (1 + index), sizeof field);
    return field;
}

quux_lambda_shape quux_lambda_read_shape(quux_object *lambda);
/* The i-th captured reference: depth and slot relative to the closure's environment. */
void quux_lambda_free_reference(quux_object *lambda, uint32_t index, uint32_t *depth,
                                uint32_t *slot);

/* Applies a macro function to the unevaluated argument forms (a list). Installed by the
 * evaluator. */
typedef bool (*quux_macro_caller)(quux_context *context, quux_value macro_function,
                                  quux_value arguments, quux_value *result);
void quux_expander_set_macro_caller(quux_macro_caller caller);

/* Expands and analyzes a top-level form in context->current_namespace. */
bool quux_analyze(quux_context *context, quux_value form, quux_value *code);

/* Expands macro calls at the head of `form` until it is not a macro call (for tools). */
bool quux_macro_expand(quux_context *context, quux_value form, quux_value *result);

#endif
