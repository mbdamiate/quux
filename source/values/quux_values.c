#include "quux_values.h"

#include <stdlib.h>
#include <string.h>

/* Symbol and keyword objects: no values; bytes = [namespace length (4 bytes)] [text] [NUL]. */
enum { QUUX_NAME_HEADER_BYTES = 4 };

static const char *const quux_kind_names[QUUX_KIND_COUNT] = {
    [QUUX_KIND_NOTHING] = "nothing",
    [QUUX_KIND_BOOLEAN] = "boolean",
    [QUUX_KIND_INTEGER] = "integer",
    [QUUX_KIND_FLOAT] = "floating-point number",
    [QUUX_KIND_STRING] = "string",
    [QUUX_KIND_SYMBOL] = "symbol",
    [QUUX_KIND_KEYWORD] = "keyword",
    [QUUX_KIND_PAIR] = "list",
    [QUUX_KIND_VECTOR] = "vector",
    [QUUX_KIND_VECTOR_NODE] = "vector node",
    [QUUX_KIND_MAP] = "map",
    [QUUX_KIND_SET] = "set",
    [QUUX_KIND_CHAMP_NODE] = "map node",
    [QUUX_KIND_COLLISION_NODE] = "map collision node",
    [QUUX_KIND_ERROR] = "error",
    [QUUX_KIND_CLOSURE] = "function",
    [QUUX_KIND_PRIMITIVE] = "function",
    [QUUX_KIND_MACRO] = "macro",
    [QUUX_KIND_LAMBDA] = "function code",
    [QUUX_KIND_CODE] = "code",
    [QUUX_KIND_ENVIRONMENT] = "environment",
    [QUUX_KIND_NAMESPACE] = "namespace",
    [QUUX_KIND_DEFINITION] = "definition",
    [QUUX_KIND_ARGUMENTS] = "arguments",
    [QUUX_KIND_FUTURE] = "future",
    [QUUX_KIND_SCOPE] = "scope",
    [QUUX_KIND_AGENT] = "agent",
    [QUUX_KIND_STREAM] = "stream",
    [QUUX_KIND_NODE] = "node",
    [QUUX_KIND_RESOURCE] = "resource",
    [QUUX_KIND_INTERNAL] = "internal",
};

const char *quux_kind_name(quux_kind kind) {
    return kind < QUUX_KIND_COUNT ? quux_kind_names[kind] : "unknown";
}

/* ---- Interning tables ---- */

static uint64_t name_hash(const char *bytes, size_t length) {
    uint64_t hash = 1469598103934665603u; /* FNV-1a, used only to place names in the tables */
    for (size_t i = 0; i < length; i++) {
        hash ^= (unsigned char)bytes[i];
        hash *= 1099511628211u;
    }
    return hash;
}

static void mark_table(quux_value *table, size_t capacity,
                       void (*mark)(void *mark_state, quux_value), void *mark_state) {
    for (size_t i = 0; i < capacity; i++) {
        if (table[i] != 0) {
            mark(mark_state, table[i]);
        }
    }
}

static void enumerate_intern_tables(void *data, void (*mark)(void *mark_state, quux_value),
                                    void *mark_state) {
    quux_runtime *runtime = data;
    mark_table(runtime->symbol_table, runtime->symbol_table_capacity, mark, mark_state);
    mark_table(runtime->keyword_table, runtime->keyword_table_capacity, mark, mark_state);
}

quux_runtime *quux_runtime_create(void) {
    quux_runtime *runtime = calloc(1, sizeof *runtime);
    if (runtime == NULL) {
        return NULL;
    }
    runtime->heap = quux_memory_create_heap();
    if (runtime->heap == NULL) {
        free(runtime);
        return NULL;
    }
    quux_mutex_initialize(&runtime->intern_mutex);
    runtime->output = stdout;
    runtime->maximum_recursion_depth = 10000;
    quux_memory_register_root_enumerator(runtime->heap, enumerate_intern_tables, runtime);
    return runtime;
}

void quux_runtime_destroy(quux_runtime *runtime) {
    if (runtime == NULL) {
        return;
    }
    quux_memory_shutdown(runtime->heap);
    /* Module state blocks are plain allocations owned by the runtime. */
    free(runtime->namespaces);
    free(runtime->evaluator);
    free(runtime->symbol_table);
    free(runtime->keyword_table);
    quux_mutex_destroy(&runtime->intern_mutex);
    free(runtime);
}

void quux_context_initialize(quux_context *context, quux_runtime *runtime) {
    memset(context, 0, sizeof *context);
    context->runtime = runtime;
    context->current_scope = quux_nothing;
    context->current_namespace = quux_nothing;
    context->current_node = quux_nothing;
    context->pending_error = quux_nothing;
    context->roots.fixed_slots = &context->current_scope;
    context->roots.fixed_count = QUUX_CONTEXT_FIXED_SLOT_COUNT;
    quux_memory_register_root_stack(runtime->heap, &context->roots);
    if (runtime->resource_exhausted_error == 0) {
        runtime->resource_exhausted_error = quux_nothing;
        quux_memory_register_global_root(runtime->heap, &runtime->resource_exhausted_error);
        quux_value error = quux_nothing;
        if (!quux_raise(context, "resource-exhausted", "a thread or memory allocation failed")) {
            error = context->pending_error;
        }
        runtime->resource_exhausted_error = error;
        context->pending_error = quux_nothing;
    }
}

void quux_context_finish(quux_context *context) {
    quux_memory_unregister_root_stack(context->runtime->heap, &context->roots);
}

quux_object *quux_allocate(quux_context *context, quux_kind kind, uint32_t value_count,
                           uint32_t byte_count) {
    quux_object *object = quux_memory_allocate(context->runtime->heap, &context->roots,
                                               (uint32_t)kind, value_count, byte_count);
    if (object == NULL) {
        context->pending_error = context->runtime->resource_exhausted_error;
    }
    return object;
}

void quux_store(quux_context *context, quux_object *owner, uint32_t index, quux_value value) {
    quux_memory_write_barrier(context->runtime->heap, owner, value);
    owner->values[index] = value;
}

/* ---- Numbers ---- */

bool quux_make_integer(quux_context *context, int64_t number, quux_value *result) {
    if (number >= QUUX_SMALL_INTEGER_MINIMUM && number <= QUUX_SMALL_INTEGER_MAXIMUM) {
        *result = quux_small_integer(number);
        return true;
    }
    quux_object *object = quux_allocate(context, QUUX_KIND_INTEGER, 0, sizeof(int64_t));
    if (object == NULL) {
        return false;
    }
    memcpy(quux_object_bytes(object), &number, sizeof number);
    *result = quux_object_value(object);
    return true;
}

int64_t quux_integer_value(quux_value value) {
    if (quux_is_small_integer(value)) {
        return (int64_t)value >> 3;
    }
    int64_t number;
    memcpy(&number, quux_object_bytes(quux_checked_object(value)), sizeof number);
    return number;
}

bool quux_make_float(quux_context *context, double number, quux_value *result) {
    quux_object *object = quux_allocate(context, QUUX_KIND_FLOAT, 0, sizeof(double));
    if (object == NULL) {
        return false;
    }
    memcpy(quux_object_bytes(object), &number, sizeof number);
    *result = quux_object_value(object);
    return true;
}

double quux_float_value(quux_value value) {
    double number;
    memcpy(&number, quux_object_bytes(quux_checked_object(value)), sizeof number);
    return number;
}

/* ---- Strings ---- */

bool quux_make_string(quux_context *context, const char *bytes, size_t length, quux_value *result) {
    if (length > UINT32_MAX - 1) {
        return quux_raise(context, "resource-exhausted", "string too long");
    }
    quux_object *object = quux_allocate(context, QUUX_KIND_STRING, 0, (uint32_t)length + 1);
    if (object == NULL) {
        return false;
    }
    if (length > 0) {
        memcpy(quux_object_bytes(object), bytes, length);
    }
    quux_object_bytes(object)[length] = '\0';
    *result = quux_object_value(object);
    return true;
}

bool quux_make_string_from_c(quux_context *context, const char *text, quux_value *result) {
    return quux_make_string(context, text, strlen(text), result);
}

const char *quux_string_bytes(quux_value string) {
    return (const char *)quux_object_bytes(quux_checked_object(string));
}

size_t quux_string_length(quux_value string) {
    return quux_checked_object(string)->byte_count - 1;
}

/* ---- Symbols and keywords ---- */

const char *quux_name_bytes(quux_value name) {
    return (const char *)quux_object_bytes(quux_checked_object(name)) + QUUX_NAME_HEADER_BYTES;
}

size_t quux_name_length(quux_value name) {
    return quux_checked_object(name)->byte_count - QUUX_NAME_HEADER_BYTES - 1;
}

size_t quux_symbol_namespace_length(quux_value symbol) {
    uint32_t length;
    memcpy(&length, quux_object_bytes(quux_checked_object(symbol)), sizeof length);
    return length;
}

bool quux_name_equals(quux_value name, const char *text) {
    size_t length = strlen(text);
    return quux_name_length(name) == length && memcmp(quux_name_bytes(name), text, length) == 0;
}

static quux_object *make_name_object(quux_context *context, quux_kind kind, const char *bytes,
                                     size_t length) {
    if (length > UINT32_MAX - QUUX_NAME_HEADER_BYTES - 1) {
        (void)quux_raise(context, "resource-exhausted", "name too long");
        return NULL;
    }
    quux_object *object =
        quux_allocate(context, kind, 0, (uint32_t)(length + QUUX_NAME_HEADER_BYTES + 1));
    if (object == NULL) {
        return NULL;
    }
    uint32_t namespace_length = 0;
    /* A qualified symbol has a '/' that is neither first nor last ("core/print-line"); the
     * division operator "/" itself is unqualified. */
    if (kind == QUUX_KIND_SYMBOL && length > 2) {
        const char *slash = memchr(bytes + 1, '/', length - 2);
        if (slash != NULL) {
            namespace_length = (uint32_t)(slash - bytes);
        }
    }
    unsigned char *storage = quux_object_bytes(object);
    memcpy(storage, &namespace_length, sizeof namespace_length);
    memcpy(storage + QUUX_NAME_HEADER_BYTES, bytes, length);
    storage[QUUX_NAME_HEADER_BYTES + length] = '\0';
    return object;
}

static bool intern(quux_context *context, quux_kind kind, const char *bytes, size_t length,
                   quux_value *result) {
    quux_runtime *runtime = context->runtime;
    quux_value **table =
        kind == QUUX_KIND_SYMBOL ? &runtime->symbol_table : &runtime->keyword_table;
    size_t *capacity = kind == QUUX_KIND_SYMBOL ? &runtime->symbol_table_capacity
                                                : &runtime->keyword_table_capacity;
    size_t *count = kind == QUUX_KIND_SYMBOL ? &runtime->symbol_count : &runtime->keyword_count;
    quux_mutex_lock(&runtime->intern_mutex);
    if ((*count + 1) * 2 > *capacity) {
        size_t new_capacity = *capacity == 0 ? 256 : *capacity * 2;
        quux_value *grown = calloc(new_capacity, sizeof(quux_value));
        if (grown == NULL) {
            quux_mutex_unlock(&runtime->intern_mutex);
            return quux_raise(context, "resource-exhausted", "symbol table full");
        }
        for (size_t i = 0; i < *capacity; i++) {
            quux_value existing = (*table)[i];
            if (existing != 0) {
                size_t slot = name_hash(quux_name_bytes(existing), quux_name_length(existing)) &
                              (new_capacity - 1);
                while (grown[slot] != 0) {
                    slot = (slot + 1) & (new_capacity - 1);
                }
                grown[slot] = existing;
            }
        }
        free(*table);
        *table = grown;
        *capacity = new_capacity;
    }
    size_t slot = name_hash(bytes, length) & (*capacity - 1);
    while ((*table)[slot] != 0) {
        quux_value candidate = (*table)[slot];
        if (quux_name_length(candidate) == length &&
            memcmp(quux_name_bytes(candidate), bytes, length) == 0) {
            quux_mutex_unlock(&runtime->intern_mutex);
            *result = candidate;
            return true;
        }
        slot = (slot + 1) & (*capacity - 1);
    }
    /* The intern mutex is held across this allocation; the verifier does not take it. */
    quux_object *object = make_name_object(context, kind, bytes, length);
    if (object == NULL) {
        quux_mutex_unlock(&runtime->intern_mutex);
        return false;
    }
    (*table)[slot] = quux_object_value(object);
    (*count)++;
    quux_mutex_unlock(&runtime->intern_mutex);
    *result = quux_object_value(object);
    return true;
}

bool quux_intern_symbol(quux_context *context, const char *bytes, size_t length,
                        quux_value *result) {
    return intern(context, QUUX_KIND_SYMBOL, bytes, length, result);
}

bool quux_intern_keyword(quux_context *context, const char *bytes, size_t length,
                         quux_value *result) {
    return intern(context, QUUX_KIND_KEYWORD, bytes, length, result);
}

quux_value quux_symbol(quux_context *context, const char *name) {
    quux_value result;
    if (!quux_intern_symbol(context, name, strlen(name), &result)) {
        fprintf(stderr, "quux: cannot intern symbol %s\n", name);
        abort();
    }
    return result;
}

quux_value quux_keyword(quux_context *context, const char *name) {
    quux_value result;
    if (!quux_intern_keyword(context, name, strlen(name), &result)) {
        fprintf(stderr, "quux: cannot intern keyword %s\n", name);
        abort();
    }
    return result;
}

bool quux_make_unique_symbol(quux_context *context, const char *prefix, size_t prefix_length,
                             quux_value *result) {
    uint64_t number = quux_atomic_add_relaxed(&context->runtime->unique_symbol_counter, 1);
    char buffer[512];
    if (prefix_length > 400) {
        prefix_length = 400;
    }
    /* The '#' cannot appear in a symbol written in source, so the name cannot clash; the
     * symbol is also not interned, so it is distinct even from another with the same text. */
    int length = snprintf(buffer, sizeof buffer, "%.*s#%llu", (int)prefix_length, prefix,
                          (unsigned long long)number);
    quux_object *object = make_name_object(context, QUUX_KIND_SYMBOL, buffer, (size_t)length);
    if (object == NULL) {
        return false;
    }
    *result = quux_object_value(object);
    return true;
}

/* ---- Errors ---- */

bool quux_make_error(quux_context *context, quux_value kind, quux_value message, quux_value data,
                     quux_value *result) {
    QUUX_ROOT_FRAME(context, &kind, &message, &data);
    quux_object *object = quux_allocate(context, QUUX_KIND_ERROR, QUUX_ERROR_FIELD_COUNT, 0);
    QUUX_ROOT_FRAME_END(context);
    if (object == NULL) {
        return false;
    }
    object->values[QUUX_ERROR_KIND] = kind;
    object->values[QUUX_ERROR_MESSAGE] = message;
    object->values[QUUX_ERROR_DATA] = data;
    *result = quux_object_value(object);
    return true;
}

quux_value quux_error_field(quux_value error, int field) {
    return quux_checked_object(error)->values[field];
}

bool quux_raise_value(quux_context *context, quux_value error) {
    context->pending_error = error;
    return false;
}

bool quux_raise(quux_context *context, const char *kind_name, const char *format, ...) {
    char text[1024];
    va_list arguments;
    va_start(arguments, format);
    int length = vsnprintf(text, sizeof text, format, arguments);
    va_end(arguments);
    if (length < 0) {
        length = 0;
    }
    if ((size_t)length >= sizeof text) {
        length = (int)sizeof text - 1;
    }
    quux_value kind = quux_nothing;
    quux_value message = quux_nothing;
    quux_value error = quux_nothing;
    QUUX_ROOT_FRAME(context, &kind, &message, &error);
    if (quux_intern_keyword(context, kind_name, strlen(kind_name), &kind) &&
        quux_make_string(context, text, (size_t)length, &message) &&
        quux_make_error(context, kind, message, quux_nothing, &error)) {
        context->pending_error = error;
    }
    QUUX_ROOT_FRAME_END(context);
    return false;
}

bool quux_pending_error_is(quux_context *context, const char *kind_name) {
    quux_value error = context->pending_error;
    if (!quux_is_kind(error, QUUX_KIND_ERROR)) {
        return false;
    }
    quux_value kind = quux_error_field(error, QUUX_ERROR_KIND);
    return quux_is_kind(kind, QUUX_KIND_KEYWORD) && quux_name_equals(kind, kind_name);
}
