#include "quux_encoding.h"

#include "quux_collections.h"
#include "quux_expander.h"
#include "quux_namespaces.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ---- Output sink ---- */

typedef struct sink {
    quux_content_hasher hasher;
} sink;

static void write_bytes(sink *output, const void *bytes, size_t length) {
    quux_hashing_content_update(&output->hasher, bytes, length);
}

static void write_byte(sink *output, unsigned char byte) {
    write_bytes(output, &byte, 1);
}

static void write_unsigned(sink *output, uint64_t number) {
    unsigned char bytes[8];
    for (int i = 0; i < 8; i++) {
        bytes[i] = (unsigned char)(number >> (8 * i)); /* little-endian on every target */
    }
    write_bytes(output, bytes, sizeof bytes);
}

enum {
    TAG_NOTHING,
    TAG_FALSE,
    TAG_TRUE,
    TAG_INTEGER,
    TAG_FLOAT,
    TAG_STRING,
    TAG_SYMBOL,
    TAG_KEYWORD,
    TAG_LIST,
    TAG_VECTOR,
    TAG_MAP,
    TAG_SET,
    TAG_ERROR,
    TAG_CLOSURE,
    TAG_PRIMITIVE,
    TAG_IDENTITY,
    TAG_CODE = 0x40,
    TAG_LAMBDA,
    TAG_DEFINITION_REFERENCE,
    TAG_UNRESOLVED_REFERENCE,
    TAG_GROUP_PLACEHOLDER,
    TAG_GROUP_MEMBER,
};

/* While hashing a recursive group: references to its members are placeholders. */
typedef struct group_context {
    quux_value *members; /* in final (sorted) order when indexed is true */
    uint32_t count;
    bool indexed;
} group_context;

typedef struct encoder {
    quux_context *context;
    sink output;
    const group_context *group;
    bool failed;
} encoder;

static void encode_value(encoder *state, quux_value value);
static void encode_code(encoder *state, quux_value code);

typedef struct entry_encoder {
    encoder *state;
    bool is_map;
} entry_encoder;

static bool encode_entry(void *data, quux_value key, quux_value value) {
    entry_encoder *entries = data;
    encode_value(entries->state, key);
    if (entries->is_map) {
        encode_value(entries->state, value);
    }
    return true;
}

static void encode_name(encoder *state, unsigned char tag, quux_value name) {
    write_byte(&state->output, tag);
    write_unsigned(&state->output, quux_name_length(name));
    write_bytes(&state->output, quux_name_bytes(name), quux_name_length(name));
}

static void encode_value(encoder *state, quux_value value) {
    sink *output = &state->output;
    switch (quux_kind_of(value)) {
    case QUUX_KIND_NOTHING:
        write_byte(output, TAG_NOTHING);
        return;
    case QUUX_KIND_BOOLEAN:
        write_byte(output, value == quux_true ? TAG_TRUE : TAG_FALSE);
        return;
    case QUUX_KIND_INTEGER:
        write_byte(output, TAG_INTEGER);
        write_unsigned(output, (uint64_t)quux_integer_value(value));
        return;
    case QUUX_KIND_FLOAT: {
        double number = quux_float_value(value);
        if (isnan(number)) {
            number = NAN;
        } else if (number == 0.0) {
            number = 0.0;
        }
        uint64_t bits;
        memcpy(&bits, &number, sizeof bits);
        write_byte(output, TAG_FLOAT);
        write_unsigned(output, bits);
        return;
    }
    case QUUX_KIND_STRING:
        write_byte(output, TAG_STRING);
        write_unsigned(output, quux_string_length(value));
        write_bytes(output, quux_string_bytes(value), quux_string_length(value));
        return;
    case QUUX_KIND_SYMBOL:
        encode_name(state, TAG_SYMBOL, value);
        return;
    case QUUX_KIND_KEYWORD:
        encode_name(state, TAG_KEYWORD, value);
        return;
    case QUUX_KIND_PAIR:
        write_byte(output, TAG_LIST);
        write_unsigned(output, quux_list_count(value));
        for (quux_value cell = value; !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
            encode_value(state, quux_pair_first(cell));
        }
        return;
    case QUUX_KIND_VECTOR: {
        uint64_t count = quux_vector_count(value);
        write_byte(output, TAG_VECTOR);
        write_unsigned(output, count);
        for (uint64_t i = 0; i < count; i++) {
            encode_value(state, quux_vector_element(value, i));
        }
        return;
    }
    case QUUX_KIND_MAP:
    case QUUX_KIND_SET: {
        bool is_map = quux_kind_of(value) == QUUX_KIND_MAP;
        write_byte(output, is_map ? TAG_MAP : TAG_SET);
        write_unsigned(output, quux_map_count(value));
        entry_encoder entries = {state, is_map};
        quux_map_for_each(value, encode_entry, &entries);
        return;
    }
    case QUUX_KIND_ERROR:
        write_byte(output, TAG_ERROR);
        for (int i = 0; i < QUUX_ERROR_FIELD_COUNT; i++) {
            encode_value(state, quux_error_field(value, i));
        }
        return;
    case QUUX_KIND_CLOSURE:
    case QUUX_KIND_PRIMITIVE: {
        unsigned char digest[QUUX_CONTENT_HASH_BYTES];
        if (!quux_function_content_hash(state->context, value, digest)) {
            state->failed = true;
            return;
        }
        write_byte(output, TAG_CLOSURE);
        write_bytes(output, digest, sizeof digest);
        uint32_t captured = quux_closure_captured_count(value);
        write_unsigned(output, captured);
        for (uint32_t i = 0; i < captured; i++) {
            encode_value(state, quux_closure_captured_value(value, i));
        }
        return;
    }
    default:
        /* Identity kinds: meaningful only in this process (section 13 sends them by remote
         * reference). */
        write_byte(output, TAG_IDENTITY);
        write_unsigned(output, (uint64_t)value);
        return;
    }
}

static int group_index(const group_context *group, quux_value definition) {
    if (group == NULL) {
        return -1;
    }
    for (uint32_t i = 0; i < group->count; i++) {
        if (group->members[i] == definition) {
            return (int)i;
        }
    }
    return -1;
}

/* The definition a GLOBAL node refers to, resolving it now if analysis could not. */
static quux_value global_definition(quux_context *context, quux_object *code) {
    quux_value definition = quux_load_shared(code, 1);
    if (quux_is_nothing(definition)) {
        if (quux_namespace_resolve(context, code->values[2], code->values[0], &definition) &&
            !quux_is_nothing(definition)) {
            quux_store_shared(context, code, 1, definition);
        } else {
            context->pending_error = quux_nothing;
            definition = quux_nothing;
        }
    }
    return definition;
}

static void encode_code(encoder *state, quux_value code_value) {
    sink *output = &state->output;
    quux_object *code = quux_checked_object(code_value);
    if (code->kind == QUUX_KIND_LAMBDA) {
        quux_lambda_shape shape = quux_lambda_read_shape(code);
        write_byte(output, TAG_LAMBDA);
        write_unsigned(output, shape.required_count);
        write_unsigned(output, shape.has_rest);
        write_unsigned(output, shape.frame_size);
        if (shape.has_rest) {
            encode_code(state, code->values[QUUX_LAMBDA_REST]);
        }
        for (uint32_t i = 0; i < shape.required_count; i++) {
            encode_code(state, code->values[QUUX_LAMBDA_PARAMETERS + i]);
        }
        encode_code(state, code->values[QUUX_LAMBDA_BODY]);
        return;
    }
    quux_operation operation = quux_code_operation(code);
    write_byte(output, TAG_CODE);
    write_byte(output, (unsigned char)operation);
    switch (operation) {
    case QUUX_OPERATION_CONSTANT:
        encode_value(state, code->values[0]);
        return;
    case QUUX_OPERATION_LOCAL:
        write_unsigned(output, quux_code_field(code, 0));
        write_unsigned(output, quux_code_field(code, 1));
        return;
    case QUUX_OPERATION_GLOBAL: {
        quux_value definition = global_definition(state->context, code);
        if (quux_is_nothing(definition)) {
            encode_name(state, TAG_UNRESOLVED_REFERENCE, code->values[0]);
            return;
        }
        int member = group_index(state->group, definition);
        if (member >= 0) {
            write_byte(output, state->group->indexed ? TAG_GROUP_MEMBER : TAG_GROUP_PLACEHOLDER);
            if (state->group->indexed) {
                write_unsigned(output, (uint64_t)member);
            }
            return;
        }
        unsigned char digest[QUUX_CONTENT_HASH_BYTES];
        if (!quux_definition_identity(state->context, definition, digest)) {
            state->failed = true;
            return;
        }
        write_byte(output, TAG_DEFINITION_REFERENCE);
        write_bytes(output, digest, sizeof digest);
        return;
    }
    case QUUX_OPERATION_DEFINE:
        /* The name is metadata and never hashed (section 13 rule 2). */
        write_unsigned(output, quux_code_field(code, 0));
        encode_code(state, code->values[1]);
        return;
    case QUUX_OPERATION_NAMESPACE:
        for (uint32_t i = 0; i < code->value_count; i++) {
            encode_value(state, code->values[i]);
        }
        return;
    case QUUX_OPERATION_FUNCTION:
        encode_code(state, code->values[0]);
        return;
    case QUUX_OPERATION_PATTERN_MAP:
        write_unsigned(output, code->value_count);
        for (uint32_t i = 0; i < code->value_count; i += 2) {
            encode_value(state, code->values[i]);
            encode_code(state, code->values[i + 1]);
        }
        return;
    default:
        for (int i = 0; i < 3; i++) {
            write_unsigned(output, quux_code_field(code, i));
        }
        write_unsigned(output, code->value_count);
        for (uint32_t i = 0; i < code->value_count; i++) {
            if (quux_is_nothing(code->values[i])) {
                write_byte(output, TAG_NOTHING);
            } else {
                encode_code(state, code->values[i]);
            }
        }
        return;
    }
}

static void begin(encoder *state, quux_context *context, const group_context *group) {
    state->context = context;
    state->group = group;
    state->failed = false;
    quux_hashing_content_begin(&state->output.hasher);
}

static bool finish(encoder *state, unsigned char digest[QUUX_CONTENT_HASH_BYTES]) {
    quux_hashing_content_finish(&state->output.hasher, digest);
    return !state->failed;
}

/* ---- Cached hashes ---- */

static bool load_content_hash(quux_object *object, unsigned char digest[QUUX_CONTENT_HASH_BYTES]) {
    unsigned char *stored = __atomic_load_n(&object->content_hash, __ATOMIC_ACQUIRE);
    if (stored == NULL) {
        return false;
    }
    memcpy(digest, stored, QUUX_CONTENT_HASH_BYTES);
    return true;
}

static void store_content_hash(quux_object *object,
                               const unsigned char digest[QUUX_CONTENT_HASH_BYTES]) {
    unsigned char *stored = malloc(QUUX_CONTENT_HASH_BYTES);
    if (stored == NULL) {
        return; /* recomputed next time */
    }
    memcpy(stored, digest, QUUX_CONTENT_HASH_BYTES);
    unsigned char *expected = NULL;
    if (!__atomic_compare_exchange_n(&object->content_hash, &expected, stored, false,
                                     __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        free(stored); /* another task stored the same hash first */
    }
}

/* ---- Definition identities (Tarjan's strongly connected components) ---- */

typedef struct graph_node {
    quux_value definition;
    int index;
    int low_link;
    bool on_stack;
} graph_node;

typedef struct tarjan_state {
    quux_context *context;
    graph_node *nodes;
    size_t count;
    size_t capacity;
    size_t *stack;
    size_t stack_count;
    int next_index;
    bool failed;
} tarjan_state;

static size_t find_or_add_node(tarjan_state *state, quux_value definition, bool *added) {
    for (size_t i = 0; i < state->count; i++) {
        if (state->nodes[i].definition == definition) {
            *added = false;
            return i;
        }
    }
    if (state->count == state->capacity) {
        size_t capacity = state->capacity == 0 ? 16 : state->capacity * 2;
        graph_node *grown = realloc(state->nodes, capacity * sizeof *grown);
        if (grown != NULL) {
            state->nodes = grown;
        }
        size_t *stack = realloc(state->stack, capacity * sizeof *stack);
        if (stack != NULL) {
            state->stack = stack;
        }
        if (grown == NULL || stack == NULL) {
            state->failed = true;
            return SIZE_MAX;
        }
        state->capacity = capacity;
    }
    state->nodes[state->count] = (graph_node){definition, -1, -1, false};
    *added = true;
    return state->count++;
}

/* Collects the definitions referenced by a piece of code. */
typedef struct edge_list {
    quux_value *definitions;
    size_t count;
    size_t capacity;
    bool failed;
} edge_list;

static void add_edge(edge_list *edges, quux_value definition) {
    for (size_t i = 0; i < edges->count; i++) {
        if (edges->definitions[i] == definition) {
            return;
        }
    }
    if (edges->count == edges->capacity) {
        size_t capacity = edges->capacity == 0 ? 8 : edges->capacity * 2;
        quux_value *grown = realloc(edges->definitions, capacity * sizeof *grown);
        if (grown == NULL) {
            edges->failed = true;
            return;
        }
        edges->definitions = grown;
        edges->capacity = capacity;
    }
    edges->definitions[edges->count++] = definition;
}

static void collect_edges(quux_context *context, quux_value code_value, edge_list *edges) {
    if (!quux_value_is_object(code_value)) {
        return;
    }
    quux_object *code = quux_checked_object(code_value);
    if (code->kind == QUUX_KIND_LAMBDA) {
        for (uint32_t i = 0; i < code->value_count; i++) {
            if (i != QUUX_LAMBDA_NAME) {
                collect_edges(context, code->values[i], edges);
            }
        }
        return;
    }
    if (code->kind != QUUX_KIND_CODE) {
        return;
    }
    quux_operation operation = quux_code_operation(code);
    if (operation == QUUX_OPERATION_GLOBAL) {
        quux_value definition = global_definition(context, code);
        if (!quux_is_nothing(definition)) {
            add_edge(edges, definition);
        }
        return;
    }
    if (operation == QUUX_OPERATION_CONSTANT || operation == QUUX_OPERATION_NAMESPACE) {
        return;
    }
    for (uint32_t i = 0; i < code->value_count; i++) {
        if (operation == QUUX_OPERATION_PATTERN_MAP && i % 2 == 0) {
            continue; /* keys are constants */
        }
        if (operation == QUUX_OPERATION_DEFINE && i == 0) {
            continue;
        }
        collect_edges(context, code->values[i], edges);
    }
}

static bool hash_primitive(quux_value definition, unsigned char digest[QUUX_CONTENT_HASH_BYTES]) {
    quux_content_hasher hasher;
    quux_hashing_content_begin(&hasher);
    quux_hashing_content_update(&hasher, "primitive", 9);
    quux_value name = quux_definition_field(definition, QUUX_DEFINITION_NAME);
    quux_hashing_content_update(&hasher, quux_name_bytes(name), quux_name_length(name));
    quux_hashing_content_finish(&hasher, digest);
    return true;
}

static int compare_digests(const void *left, const void *right) {
    return memcmp(left, right, QUUX_CONTENT_HASH_BYTES);
}

typedef struct member_order {
    unsigned char body_hash[QUUX_CONTENT_HASH_BYTES];
    quux_value definition;
} member_order;

/* Hashes one strongly connected component whose dependencies are already hashed. */
static bool hash_component(quux_context *context, quux_value *members, size_t count,
                           bool self_reference) {
    if (count == 1 && !self_reference) {
        quux_value code = quux_definition_field(members[0], QUUX_DEFINITION_CODE);
        unsigned char digest[QUUX_CONTENT_HASH_BYTES];
        if (quux_is_nothing(code)) {
            hash_primitive(members[0], digest);
        } else {
            encoder state;
            begin(&state, context, NULL);
            write_byte(&state.output, 1);
            encode_code(&state, code);
            if (!finish(&state, digest)) {
                return false;
            }
        }
        store_content_hash(quux_checked_object(members[0]), digest);
        return true;
    }
    /* A recursive group: order members by the hash of their bodies with member references as
     * placeholders, then hash the group with references as indexes (section 13 rule 3). */
    member_order *order = malloc(count * sizeof *order);
    if (order == NULL) {
        return quux_raise(context, "resource-exhausted", "out of memory while hashing");
    }
    group_context placeholder_group = {members, (uint32_t)count, false};
    bool done = true;
    for (size_t i = 0; i < count && done; i++) {
        encoder state;
        begin(&state, context, &placeholder_group);
        encode_code(&state, quux_definition_field(members[i], QUUX_DEFINITION_CODE));
        done = finish(&state, order[i].body_hash);
        order[i].definition = members[i];
    }
    if (done) {
        qsort(order, count, sizeof *order, compare_digests);
        quux_value *sorted = malloc(count * sizeof *sorted);
        done = sorted != NULL;
        if (done) {
            for (size_t i = 0; i < count; i++) {
                sorted[i] = order[i].definition;
            }
            group_context indexed_group = {sorted, (uint32_t)count, true};
            encoder state;
            begin(&state, context, &indexed_group);
            write_byte(&state.output, 2);
            write_unsigned(&state.output, count);
            for (size_t i = 0; i < count; i++) {
                encode_code(&state, quux_definition_field(sorted[i], QUUX_DEFINITION_CODE));
            }
            unsigned char group_hash[QUUX_CONTENT_HASH_BYTES];
            done = finish(&state, group_hash);
            for (size_t i = 0; i < count && done; i++) {
                quux_content_hasher member_hasher;
                unsigned char member_hash[QUUX_CONTENT_HASH_BYTES];
                quux_hashing_content_begin(&member_hasher);
                quux_hashing_content_update(&member_hasher, group_hash, sizeof group_hash);
                uint64_t index = i;
                quux_hashing_content_update(&member_hasher, &index, sizeof index);
                quux_hashing_content_finish(&member_hasher, member_hash);
                store_content_hash(quux_checked_object(sorted[i]), member_hash);
            }
            free(sorted);
        }
    }
    free(order);
    return done;
}

static void strong_connect(tarjan_state *state, size_t node_index) {
    quux_context *context = state->context;
    graph_node *node = &state->nodes[node_index];
    node->index = node->low_link = state->next_index++;
    state->stack[state->stack_count++] = node_index;
    node->on_stack = true;
    quux_value definition = node->definition;
    edge_list edges = {NULL, 0, 0, false};
    collect_edges(context, quux_definition_field(definition, QUUX_DEFINITION_CODE), &edges);
    bool self_reference = false;
    for (size_t e = 0; e < edges.count && !state->failed; e++) {
        quux_value target = edges.definitions[e];
        if (target == definition) {
            self_reference = true;
        }
        unsigned char ignored[QUUX_CONTENT_HASH_BYTES];
        if (load_content_hash(quux_checked_object(target), ignored)) {
            continue; /* already hashed, outside any component being built */
        }
        bool added = false;
        size_t target_index = find_or_add_node(state, target, &added);
        if (state->failed) {
            break;
        }
        if (added) {
            strong_connect(state, target_index);
            graph_node *current = &state->nodes[node_index];
            if (state->nodes[target_index].low_link < current->low_link) {
                current->low_link = state->nodes[target_index].low_link;
            }
        } else if (state->nodes[target_index].on_stack) {
            graph_node *current = &state->nodes[node_index];
            if (state->nodes[target_index].index < current->low_link) {
                current->low_link = state->nodes[target_index].index;
            }
        }
    }
    free(edges.definitions);
    if (edges.failed) {
        state->failed = true;
    }
    graph_node *current = &state->nodes[node_index];
    if (current->low_link == current->index && !state->failed) {
        size_t count = 0;
        quux_value *members = malloc(state->stack_count * sizeof *members);
        if (members == NULL) {
            state->failed = true;
            return;
        }
        size_t popped;
        do {
            popped = state->stack[--state->stack_count];
            state->nodes[popped].on_stack = false;
            members[count++] = state->nodes[popped].definition;
        } while (popped != node_index);
        if (!hash_component(context, members, count, self_reference)) {
            state->failed = true;
        }
        free(members);
    }
}

bool quux_definition_identity(quux_context *context, quux_value definition,
                              unsigned char digest[QUUX_CONTENT_HASH_BYTES]) {
    quux_object *object = quux_checked_object(definition);
    if (load_content_hash(object, digest)) {
        return true;
    }
    tarjan_state state = {context, NULL, 0, 0, NULL, 0, 0, false};
    bool added = false;
    size_t start = find_or_add_node(&state, definition, &added);
    if (!state.failed) {
        strong_connect(&state, start);
    }
    free(state.nodes);
    free(state.stack);
    if (state.failed) {
        if (quux_is_nothing(context->pending_error)) {
            (void)quux_raise(context, "resource-exhausted", "out of memory while hashing");
        }
        return false;
    }
    return load_content_hash(object, digest);
}

/* ---- Functions ---- */

bool quux_function_content_hash(quux_context *context, quux_value function,
                                unsigned char digest[QUUX_CONTENT_HASH_BYTES]) {
    quux_object *object = quux_checked_object(function);
    if (object->kind == QUUX_KIND_PRIMITIVE) {
        quux_content_hasher hasher;
        quux_hashing_content_begin(&hasher);
        quux_hashing_content_update(&hasher, "primitive", 9);
        quux_value name = object->values[QUUX_PRIMITIVE_NAME];
        quux_hashing_content_update(&hasher, quux_name_bytes(name), quux_name_length(name));
        quux_hashing_content_finish(&hasher, digest);
        return true;
    }
    if (object->kind != QUUX_KIND_CLOSURE) {
        return quux_raise(context, "wrong-type", "function-hash expects a function, got %s",
                          quux_kind_phrase((quux_kind)object->kind));
    }
    quux_object *lambda = quux_checked_object(object->values[QUUX_CLOSURE_LAMBDA]);
    if (load_content_hash(lambda, digest)) {
        return true;
    }
    encoder state;
    begin(&state, context, NULL);
    write_byte(&state.output, 3);
    encode_code(&state, object->values[QUUX_CLOSURE_LAMBDA]);
    if (!finish(&state, digest)) {
        return false;
    }
    store_content_hash(lambda, digest);
    return true;
}

uint32_t quux_closure_captured_count(quux_value closure) {
    quux_object *object = quux_checked_object(closure);
    if (object->kind != QUUX_KIND_CLOSURE) {
        return 0;
    }
    return quux_lambda_read_shape(quux_checked_object(object->values[QUUX_CLOSURE_LAMBDA]))
        .free_count;
}

quux_value quux_closure_captured_value(quux_value closure, uint32_t index) {
    quux_object *object = quux_checked_object(closure);
    uint32_t depth = 0;
    uint32_t slot = 0;
    quux_lambda_free_reference(quux_checked_object(object->values[QUUX_CLOSURE_LAMBDA]), index,
                               &depth, &slot);
    quux_value environment = object->values[QUUX_CLOSURE_ENVIRONMENT];
    for (uint32_t i = 0; i < depth; i++) {
        environment = quux_checked_object(environment)->values[QUUX_ENVIRONMENT_PARENT];
    }
    return quux_checked_object(environment)->values[QUUX_ENVIRONMENT_FIRST_SLOT + slot];
}

static bool functions_equal(quux_context *context, quux_value left, quux_value right) {
    unsigned char left_hash[QUUX_CONTENT_HASH_BYTES];
    unsigned char right_hash[QUUX_CONTENT_HASH_BYTES];
    if (!quux_function_content_hash(context, left, left_hash) ||
        !quux_function_content_hash(context, right, right_hash)) {
        context->pending_error = quux_nothing;
        return false;
    }
    if (memcmp(left_hash, right_hash, sizeof left_hash) != 0) {
        return false;
    }
    uint32_t count = quux_closure_captured_count(left);
    if (count != quux_closure_captured_count(right)) {
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        if (!quux_equal(context, quux_closure_captured_value(left, i),
                        quux_closure_captured_value(right, i))) {
            return false;
        }
    }
    return true;
}

static uint64_t function_hash(quux_context *context, quux_value function) {
    unsigned char digest[QUUX_CONTENT_HASH_BYTES];
    if (!quux_function_content_hash(context, function, digest)) {
        context->pending_error = quux_nothing;
        return 0;
    }
    uint64_t hash;
    memcpy(&hash, digest, sizeof hash);
    uint32_t count = quux_closure_captured_count(function);
    for (uint32_t i = 0; i < count; i++) {
        hash = quux_hashing_combine(
            hash, quux_value_hash(context, quux_closure_captured_value(function, i)));
    }
    return hash;
}

void quux_encoding_install_function_hooks(void) {
    static const quux_function_hooks hooks = {functions_equal, function_hash};
    quux_collections_set_function_hooks(&hooks);
}
