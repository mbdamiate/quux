/* Reuse: keep. Value representation (D-35): tagged words, heap kinds, numbers, strings, interned
 * symbols and keywords, error values, the runtime and the per-task context (data-model.md
 * "Context", "Heap object header"). */
#ifndef QUUX_VALUES_H
#define QUUX_VALUES_H

#include "quux_memory.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* ---- Constants ---- */

#define QUUX_CONSTANT(index) ((quux_value)(((uint64_t)(index) << 3) | QUUX_TAG_CONSTANT))
#define quux_nothing QUUX_CONSTANT(0)
#define quux_false QUUX_CONSTANT(1)
#define quux_true QUUX_CONSTANT(2)

static inline quux_value quux_boolean(bool condition) {
    return condition ? quux_true : quux_false;
}
static inline bool quux_is_nothing(quux_value value) {
    return value == quux_nothing;
}
/* Only `false` and `nothing` are false in a test (section 03 rule 4). */
static inline bool quux_is_truthy(quux_value value) {
    return value != quux_nothing && value != quux_false;
}

/* ---- Kinds ---- */

typedef enum quux_kind {
    QUUX_KIND_NOTHING,
    QUUX_KIND_BOOLEAN,
    QUUX_KIND_INTEGER, /* small (tagged) or boxed */
    QUUX_KIND_FLOAT,
    QUUX_KIND_STRING,
    QUUX_KIND_SYMBOL,
    QUUX_KIND_KEYWORD,
    QUUX_KIND_PAIR, /* list cell; the empty list is `nothing` */
    QUUX_KIND_VECTOR,
    QUUX_KIND_VECTOR_NODE,
    QUUX_KIND_MAP,
    QUUX_KIND_SET,
    QUUX_KIND_CHAMP_NODE,
    QUUX_KIND_COLLISION_NODE,
    QUUX_KIND_ERROR,
    QUUX_KIND_CLOSURE,
    QUUX_KIND_PRIMITIVE,
    QUUX_KIND_MACRO,
    QUUX_KIND_LAMBDA,      /* the code of a `function` form, shared by its closures */
    QUUX_KIND_CODE,        /* an analyzed expression node */
    QUUX_KIND_ENVIRONMENT, /* a frame of local bindings */
    QUUX_KIND_NAMESPACE,
    QUUX_KIND_DEFINITION, /* an immutable definition record (D-23) */
    QUUX_KIND_ARGUMENTS,  /* evaluated arguments of a primitive call */
    QUUX_KIND_FUTURE,
    QUUX_KIND_SCOPE,
    QUUX_KIND_AGENT,
    QUUX_KIND_STREAM,
    QUUX_KIND_NODE,
    QUUX_KIND_RESOURCE,
    QUUX_KIND_INTERNAL, /* runtime bookkeeping with no quux meaning */
    QUUX_KIND_COUNT
} quux_kind;

static inline quux_object *quux_checked_object(quux_value value) {
    quux_object *object = quux_value_object(value);
    quux_memory_check_object(object);
    return object;
}

static inline quux_kind quux_kind_of(quux_value value) {
    switch (value & QUUX_TAG_MASK) {
    case QUUX_TAG_SMALL_INTEGER:
        return QUUX_KIND_INTEGER;
    case QUUX_TAG_CONSTANT:
        return value == quux_nothing ? QUUX_KIND_NOTHING : QUUX_KIND_BOOLEAN;
    default:
        return (quux_kind)quux_checked_object(value)->kind;
    }
}

static inline bool quux_is_kind(quux_value value, quux_kind kind) {
    return quux_kind_of(value) == kind;
}

/* The kind's name for messages ("integer", "vector", ...). */
const char *quux_kind_name(quux_kind kind);

/* The kind's name as a noun phrase for messages: "an integer", "a vector", and "nothing" for the
 * nothing value (friction P10-F1). */
const char *quux_kind_phrase(quux_kind kind);

/* ---- Runtime and context ---- */

typedef struct quux_runtime quux_runtime;
struct quux_task;
struct quux_namespace_registry;
struct quux_evaluator_state;
struct quux_concurrency_state;

/* Per-task state, passed explicitly to every runtime function (D-33). */
typedef struct quux_context {
    quux_runtime *runtime;
    quux_root_stack roots;
    struct quux_task *task;
    /* Fixed root slots: registered with the root stack. Keep them together and first. */
    quux_value current_scope;
    quux_value current_namespace;
    quux_value current_node;
    quux_value pending_error;
    /* A spawned task's work: the function, its ARGUMENTS object and the future it fills
     * (nothing for the session's own task). */
    quux_value task_function;
    quux_value task_arguments;
    quux_value task_future;
    uint32_t recursion_depth;
} quux_context;

enum { QUUX_CONTEXT_FIXED_SLOT_COUNT = 7 };

/* The functions that the concurrency forms (`future`, `spawn`, `with-scope`,
 * `shield-from-cancellation`, `tag`) call after the expander wraps their bodies in functions.
 * Installed by the concurrency module and the harness; `nothing` until then. */
typedef enum quux_form_function {
    QUUX_FORM_SPAWN,      /* (f function) or (f scope function) */
    QUUX_FORM_WITH_SCOPE, /* (f (function [scope] body ...)) */
    QUUX_FORM_SHIELD,     /* (f function) */
    QUUX_FORM_TAG,        /* (f name function) */
    QUUX_FORM_FUNCTION_COUNT
} quux_form_function;

struct quux_runtime {
    quux_heap *heap;
    quux_mutex intern_mutex;
    quux_value *symbol_table;
    size_t symbol_table_capacity;
    size_t symbol_count;
    quux_value *keyword_table;
    size_t keyword_table_capacity;
    size_t keyword_count;
    quux_atomic_unsigned unique_symbol_counter;
    struct quux_namespace_registry *namespaces;
    struct quux_evaluator_state *evaluator;
    struct quux_concurrency_state *concurrency;
    quux_value form_functions[QUUX_FORM_FUNCTION_COUNT];
    FILE *output;
    uint32_t maximum_recursion_depth;
    /* Preallocated, because raising it must not need memory. */
    quux_value resource_exhausted_error;
};

quux_runtime *quux_runtime_create(void);
/* Releases every object of the runtime (quux_memory_shutdown) and the runtime itself. */
void quux_runtime_destroy(quux_runtime *runtime);

/* Registers the context's roots; must be paired with quux_context_finish. */
void quux_context_initialize(quux_context *context, quux_runtime *runtime);
void quux_context_finish(quux_context *context);

/* Allocates through the heap. On failure, stores :resource-exhausted in the context and returns
 * NULL. */
quux_object *quux_allocate(quux_context *context, quux_kind kind, uint32_t value_count,
                           uint32_t byte_count);

/* Stores a value into a mutable cell of `owner`, through the write barrier (rule 5). */
void quux_store(quux_context *context, quux_object *owner, uint32_t index, quux_value value);

/* For cells that several tasks may read and write at the same time (the resolution cache of a
 * global reference): the store releases and the load acquires. */
void quux_store_shared(quux_context *context, quux_object *owner, uint32_t index, quux_value value);
static inline quux_value quux_load_shared(quux_object *owner, uint32_t index) {
    return __atomic_load_n(&owner->values[index], __ATOMIC_ACQUIRE);
}

/* ---- Numbers ---- */

enum { QUUX_SMALL_INTEGER_BITS = 61 };
#define QUUX_SMALL_INTEGER_MAXIMUM ((int64_t)((UINT64_C(1) << (QUUX_SMALL_INTEGER_BITS - 1)) - 1))
#define QUUX_SMALL_INTEGER_MINIMUM (-QUUX_SMALL_INTEGER_MAXIMUM - 1)

static inline bool quux_is_small_integer(quux_value value) {
    return (value & QUUX_TAG_MASK) == QUUX_TAG_SMALL_INTEGER;
}
static inline quux_value quux_small_integer(int64_t number) {
    return ((uint64_t)number << 3) | QUUX_TAG_SMALL_INTEGER;
}
bool quux_make_integer(quux_context *context, int64_t number, quux_value *result);
int64_t quux_integer_value(quux_value value);
bool quux_make_float(quux_context *context, double number, quux_value *result);
double quux_float_value(quux_value value);

/* ---- Strings, symbols, keywords ---- */

bool quux_make_string(quux_context *context, const char *bytes, size_t length, quux_value *result);
bool quux_make_string_from_c(quux_context *context, const char *text, quux_value *result);
const char *quux_string_bytes(quux_value string);
size_t quux_string_length(quux_value string);

/* Symbols are interned: two symbols with the same text are the same object. The text may be
 * qualified ("namespace/name"). */
bool quux_intern_symbol(quux_context *context, const char *bytes, size_t length,
                        quux_value *result);
bool quux_intern_keyword(quux_context *context, const char *bytes, size_t length,
                         quux_value *result);
/* Interns with a NUL-terminated name; aborts on allocation failure (startup only). */
quux_value quux_symbol(quux_context *context, const char *name);
quux_value quux_keyword(quux_context *context, const char *name);
/* The full text (without ':' for keywords). */
const char *quux_name_bytes(quux_value symbol_or_keyword);
size_t quux_name_length(quux_value symbol_or_keyword);
/* For a qualified symbol, the length of the namespace part (0 when unqualified). */
size_t quux_symbol_namespace_length(quux_value symbol);
bool quux_name_equals(quux_value symbol_or_keyword, const char *text);

/* A fresh symbol that is not interned and cannot clash with any other (section 04 rule 5). */
bool quux_make_unique_symbol(quux_context *context, const char *prefix, size_t prefix_length,
                             quux_value *result);

/* ---- Errors (section 12) ---- */

enum {
    QUUX_ERROR_KIND,
    QUUX_ERROR_MESSAGE,
    QUUX_ERROR_DATA,
    QUUX_ERROR_ORIGIN,
    QUUX_ERROR_CAUSE,
    QUUX_ERROR_SUPPRESSED,
    QUUX_ERROR_FIELD_COUNT
};

bool quux_make_error(quux_context *context, quux_value kind, quux_value message, quux_value data,
                     quux_value *result);
quux_value quux_error_field(quux_value error, int field);

/* Creates an error of the given kind (keyword name without ':'), stores it as the pending error
 * and returns false, so that `return quux_raise(...)` propagates it. */
bool quux_raise(quux_context *context, const char *kind_name, const char *format, ...)
    QUUX_PRINTF_FORMAT(3, 4);
/* Stores an existing error value as the pending error and returns false. */
bool quux_raise_value(quux_context *context, quux_value error);
/* Whether the pending error has the given kind. */
bool quux_pending_error_is(quux_context *context, const char *kind_name);

#endif
