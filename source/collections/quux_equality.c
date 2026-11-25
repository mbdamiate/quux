/* Structural equality, the value hash and the canonical order (section 01), and the generic
 * collection operations shared by the core functions. */
#include "quux_collections.h"

#include "quux_hashing.h"

#include <math.h>
#include <string.h>

static quux_function_hooks function_hooks;

void quux_collections_set_function_hooks(const quux_function_hooks *hooks) {
    function_hooks = *hooks;
}

/* Seeds separate kinds, so a string and a symbol with the same text hash differently. */
enum {
    SEED_NOTHING = 11,
    SEED_BOOLEAN = 13,
    SEED_INTEGER = 17,
    SEED_FLOAT = 19,
    SEED_STRING = 23,
    SEED_SYMBOL = 29,
    SEED_KEYWORD = 31,
    SEED_LIST = 37,
    SEED_VECTOR = 41,
    SEED_MAP = 43,
    SEED_SET = 47,
    SEED_NODE = 53,
    SEED_ERROR = 59,
    SEED_IDENTITY = 61,
};

static bool load_cached_hash(quux_object *object, uint64_t *hash) {
    uint32_t flags = __atomic_load_n(&object->flags, __ATOMIC_ACQUIRE);
    if (flags & QUUX_OBJECT_FLAG_HASHED) {
        *hash = __atomic_load_n(&object->value_hash, __ATOMIC_RELAXED);
        return true;
    }
    return false;
}

static uint64_t store_cached_hash(quux_object *object, uint64_t hash) {
    __atomic_store_n(&object->value_hash, hash, __ATOMIC_RELAXED);
    __atomic_fetch_or(&object->flags, (uint32_t)QUUX_OBJECT_FLAG_HASHED, __ATOMIC_RELEASE);
    return hash;
}

static uint64_t hash_integer(int64_t number) {
    return quux_hashing_value(&number, sizeof number, SEED_INTEGER);
}

static uint64_t hash_float(double number) {
    if (isnan(number)) {
        number = NAN; /* every NaN is equal? to every other (section 01 rule 3) */
    } else if (number == 0.0) {
        number = 0.0; /* 0.0 and -0.0 are equal? */
    }
    return quux_hashing_value(&number, sizeof number, SEED_FLOAT);
}

static uint64_t hash_node(quux_context *context, quux_object *node);

uint64_t quux_value_hash(quux_context *context, quux_value value) {
    switch (quux_kind_of(value)) {
    case QUUX_KIND_NOTHING:
        return quux_hashing_value("", 0, SEED_NOTHING);
    case QUUX_KIND_BOOLEAN:
        return quux_hashing_value(value == quux_true ? "t" : "f", 1, SEED_BOOLEAN);
    case QUUX_KIND_INTEGER:
        return hash_integer(quux_integer_value(value));
    case QUUX_KIND_FLOAT:
        return hash_float(quux_float_value(value));
    default:
        break;
    }
    quux_object *object = quux_checked_object(value);
    uint64_t hash;
    if (load_cached_hash(object, &hash)) {
        return hash;
    }
    switch ((quux_kind)object->kind) {
    case QUUX_KIND_STRING:
        hash = quux_hashing_value(quux_string_bytes(value), quux_string_length(value), SEED_STRING);
        break;
    case QUUX_KIND_SYMBOL:
        hash = quux_hashing_value(quux_name_bytes(value), quux_name_length(value), SEED_SYMBOL);
        break;
    case QUUX_KIND_KEYWORD:
        hash = quux_hashing_value(quux_name_bytes(value), quux_name_length(value), SEED_KEYWORD);
        break;
    case QUUX_KIND_PAIR: {
        hash = SEED_LIST;
        for (quux_value cell = value; !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
            hash = quux_hashing_combine(hash, quux_value_hash(context, quux_pair_first(cell)));
        }
        break;
    }
    case QUUX_KIND_VECTOR: {
        hash = SEED_VECTOR;
        uint64_t count = quux_vector_count(value);
        for (uint64_t i = 0; i < count; i++) {
            hash =
                quux_hashing_combine(hash, quux_value_hash(context, quux_vector_element(value, i)));
        }
        break;
    }
    case QUUX_KIND_MAP:
    case QUUX_KIND_SET:
        /* The structure is canonical, so a structural hash does not depend on construction
         * order (D-03). */
        hash = quux_hashing_combine(object->kind == QUUX_KIND_MAP ? SEED_MAP : SEED_SET,
                                    hash_node(context, quux_checked_object(object->values[0])));
        break;
    case QUUX_KIND_ERROR: {
        hash = SEED_ERROR;
        for (uint32_t i = 0; i < QUUX_ERROR_FIELD_COUNT; i++) {
            hash = quux_hashing_combine(hash, quux_value_hash(context, object->values[i]));
        }
        break;
    }
    case QUUX_KIND_CLOSURE:
    case QUUX_KIND_PRIMITIVE:
        if (function_hooks.hash != NULL) {
            return function_hooks.hash(context, value);
        }
        hash = quux_hashing_combine(SEED_IDENTITY, (uint64_t)(uintptr_t)object);
        return hash;
    default:
        /* Identity kinds (futures, agents, scopes, nodes, resources): never cached, because
         * the hash of an address is meaningful only in this process. */
        return quux_hashing_combine(SEED_IDENTITY, (uint64_t)(uintptr_t)object);
    }
    return store_cached_hash(object, hash);
}

static uint64_t hash_node(quux_context *context, quux_object *node) {
    uint64_t hash;
    if (load_cached_hash(node, &hash)) {
        return hash;
    }
    hash = SEED_NODE;
    uint32_t width;
    uint32_t payload_values;
    if (node->kind == QUUX_KIND_COLLISION_NODE) {
        memcpy(&width, quux_object_bytes(node), sizeof width);
        payload_values = node->value_count;
    } else {
        uint32_t data_bitmap;
        uint32_t node_bitmap;
        memcpy(&data_bitmap, quux_object_bytes(node), 4);
        memcpy(&node_bitmap, quux_object_bytes(node) + 4, 4);
        memcpy(&width, quux_object_bytes(node) + 8, 4);
        payload_values = (uint32_t)__builtin_popcount(data_bitmap) * width;
        hash = quux_hashing_combine(hash, ((uint64_t)data_bitmap << 32) | node_bitmap);
    }
    for (uint32_t i = 0; i < payload_values; i++) {
        hash = quux_hashing_combine(hash, quux_value_hash(context, node->values[i]));
    }
    for (uint32_t i = payload_values; i < node->value_count; i++) {
        hash = quux_hashing_combine(hash, hash_node(context, quux_checked_object(node->values[i])));
    }
    return store_cached_hash(node, hash);
}

/* ---- Equality ---- */

static bool equal_floats(double left, double right) {
    if (isnan(left) || isnan(right)) {
        return isnan(left) && isnan(right);
    }
    return left == right;
}

static bool equal_nodes(quux_context *context, quux_object *left, quux_object *right) {
    if (left == right) {
        return true;
    }
    if (left->kind != right->kind || left->value_count != right->value_count ||
        left->byte_count != right->byte_count ||
        memcmp(quux_object_bytes(left), quux_object_bytes(right), left->byte_count) != 0) {
        return false;
    }
    uint32_t payload_values;
    if (left->kind == QUUX_KIND_COLLISION_NODE) {
        payload_values = left->value_count;
    } else {
        uint32_t data_bitmap;
        uint32_t width;
        memcpy(&data_bitmap, quux_object_bytes(left), 4);
        memcpy(&width, quux_object_bytes(left) + 8, 4);
        payload_values = (uint32_t)__builtin_popcount(data_bitmap) * width;
    }
    for (uint32_t i = 0; i < payload_values; i++) {
        if (!quux_equal(context, left->values[i], right->values[i])) {
            return false;
        }
    }
    for (uint32_t i = payload_values; i < left->value_count; i++) {
        if (!equal_nodes(context, quux_checked_object(left->values[i]),
                         quux_checked_object(right->values[i]))) {
            return false;
        }
    }
    return true;
}

bool quux_equal(quux_context *context, quux_value left, quux_value right) {
    if (left == right) {
        return true;
    }
    quux_kind kind = quux_kind_of(left);
    if (kind != quux_kind_of(right)) {
        /* Closures and primitives are both functions. */
        bool both_functions = (kind == QUUX_KIND_CLOSURE || kind == QUUX_KIND_PRIMITIVE) &&
                              (quux_kind_of(right) == QUUX_KIND_CLOSURE ||
                               quux_kind_of(right) == QUUX_KIND_PRIMITIVE);
        if (!both_functions) {
            return false;
        }
    }
    switch (kind) {
    case QUUX_KIND_NOTHING:
    case QUUX_KIND_BOOLEAN:
    case QUUX_KIND_SYMBOL:  /* interned, or unique */
    case QUUX_KIND_KEYWORD: /* interned */
        return false;
    case QUUX_KIND_INTEGER:
        return quux_integer_value(left) == quux_integer_value(right);
    case QUUX_KIND_FLOAT:
        return equal_floats(quux_float_value(left), quux_float_value(right));
    case QUUX_KIND_STRING:
        return quux_string_length(left) == quux_string_length(right) &&
               memcmp(quux_string_bytes(left), quux_string_bytes(right),
                      quux_string_length(left)) == 0;
    case QUUX_KIND_PAIR: {
        if (quux_list_count(left) != quux_list_count(right)) {
            return false;
        }
        while (!quux_is_nothing(left)) {
            if (!quux_equal(context, quux_pair_first(left), quux_pair_first(right))) {
                return false;
            }
            left = quux_pair_rest(left);
            right = quux_pair_rest(right);
        }
        return true;
    }
    case QUUX_KIND_VECTOR: {
        uint64_t count = quux_vector_count(left);
        if (count != quux_vector_count(right)) {
            return false;
        }
        for (uint64_t i = 0; i < count; i++) {
            if (!quux_equal(context, quux_vector_element(left, i), quux_vector_element(right, i))) {
                return false;
            }
        }
        return true;
    }
    case QUUX_KIND_MAP:
    case QUUX_KIND_SET:
        return quux_map_count(left) == quux_map_count(right) &&
               quux_value_hash(context, left) == quux_value_hash(context, right) &&
               equal_nodes(context, quux_checked_object(quux_checked_object(left)->values[0]),
                           quux_checked_object(quux_checked_object(right)->values[0]));
    case QUUX_KIND_ERROR:
        for (int i = 0; i < QUUX_ERROR_FIELD_COUNT; i++) {
            if (!quux_equal(context, quux_error_field(left, i), quux_error_field(right, i))) {
                return false;
            }
        }
        return true;
    case QUUX_KIND_CLOSURE:
    case QUUX_KIND_PRIMITIVE:
        return function_hooks.equal != NULL && function_hooks.equal(context, left, right);
    default:
        return false; /* identity kinds: equal only to themselves */
    }
}

/* ---- Canonical order ---- */

static int compare_numbers(int64_t left, int64_t right) {
    return left < right ? -1 : left > right ? 1 : 0;
}

static int compare_bytes(const char *left, size_t left_length, const char *right,
                         size_t right_length) {
    size_t shorter = left_length < right_length ? left_length : right_length;
    int order = memcmp(left, right, shorter);
    if (order != 0) {
        return order < 0 ? -1 : 1;
    }
    return compare_numbers((int64_t)left_length, (int64_t)right_length);
}

typedef struct entry_collector {
    quux_value *entries;
    size_t count;
    size_t capacity;
} entry_collector;

static int compare_addresses(quux_value left, quux_value right) {
    return left < right ? -1 : left > right ? 1 : 0;
}

int quux_canonical_compare(quux_context *context, quux_value left, quux_value right) {
    if (quux_equal(context, left, right)) {
        return 0;
    }
    quux_kind left_kind = quux_kind_of(left);
    quux_kind right_kind = quux_kind_of(right);
    if (left_kind != right_kind) {
        return left_kind < right_kind ? -1 : 1;
    }
    switch (left_kind) {
    case QUUX_KIND_BOOLEAN:
        return left == quux_false ? -1 : 1;
    case QUUX_KIND_INTEGER:
        return compare_numbers(quux_integer_value(left), quux_integer_value(right));
    case QUUX_KIND_FLOAT: {
        double a = quux_float_value(left);
        double b = quux_float_value(right);
        if (isnan(a)) {
            return 1;
        }
        if (isnan(b)) {
            return -1;
        }
        return a < b ? -1 : 1;
    }
    case QUUX_KIND_STRING:
        return compare_bytes(quux_string_bytes(left), quux_string_length(left),
                             quux_string_bytes(right), quux_string_length(right));
    case QUUX_KIND_SYMBOL:
    case QUUX_KIND_KEYWORD: {
        int order = compare_bytes(quux_name_bytes(left), quux_name_length(left),
                                  quux_name_bytes(right), quux_name_length(right));
        return order != 0 ? order : compare_addresses(left, right);
    }
    case QUUX_KIND_PAIR: {
        while (!quux_is_nothing(left) && !quux_is_nothing(right)) {
            int order =
                quux_canonical_compare(context, quux_pair_first(left), quux_pair_first(right));
            if (order != 0) {
                return order;
            }
            left = quux_pair_rest(left);
            right = quux_pair_rest(right);
        }
        return quux_is_nothing(left) ? -1 : 1;
    }
    case QUUX_KIND_VECTOR: {
        uint64_t left_count = quux_vector_count(left);
        uint64_t right_count = quux_vector_count(right);
        uint64_t shorter = left_count < right_count ? left_count : right_count;
        for (uint64_t i = 0; i < shorter; i++) {
            int order = quux_canonical_compare(context, quux_vector_element(left, i),
                                               quux_vector_element(right, i));
            if (order != 0) {
                return order;
            }
        }
        return left_count < right_count ? -1 : 1;
    }
    case QUUX_KIND_MAP:
    case QUUX_KIND_SET: {
        uint64_t left_count = quux_map_count(left);
        uint64_t right_count = quux_map_count(right);
        if (left_count != right_count) {
            return left_count < right_count ? -1 : 1;
        }
        uint64_t left_hash = quux_value_hash(context, left);
        uint64_t right_hash = quux_value_hash(context, right);
        if (left_hash != right_hash) {
            return left_hash < right_hash ? -1 : 1;
        }
        return compare_addresses(left, right); /* unequal maps with equal hashes: rare */
    }
    case QUUX_KIND_ERROR:
        for (int i = 0; i < QUUX_ERROR_FIELD_COUNT; i++) {
            int order = quux_canonical_compare(context, quux_error_field(left, i),
                                               quux_error_field(right, i));
            if (order != 0) {
                return order;
            }
        }
        return 0;
    default: {
        uint64_t left_hash = quux_value_hash(context, left);
        uint64_t right_hash = quux_value_hash(context, right);
        if (left_hash != right_hash) {
            return left_hash < right_hash ? -1 : 1;
        }
        return compare_addresses(left, right);
    }
    }
}

/* ---- Generic operations ---- */

bool quux_is_sequential(quux_value value) {
    quux_kind kind = quux_kind_of(value);
    return kind == QUUX_KIND_NOTHING || kind == QUUX_KIND_PAIR || kind == QUUX_KIND_VECTOR;
}

bool quux_count(quux_context *context, quux_value collection, uint64_t *count) {
    switch (quux_kind_of(collection)) {
    case QUUX_KIND_NOTHING:
        *count = 0;
        return true;
    case QUUX_KIND_PAIR:
        *count = quux_list_count(collection);
        return true;
    case QUUX_KIND_VECTOR:
        *count = quux_vector_count(collection);
        return true;
    case QUUX_KIND_MAP:
    case QUUX_KIND_SET:
        *count = quux_map_count(collection);
        return true;
    case QUUX_KIND_STRING:
        *count = quux_string_length(collection);
        return true;
    default:
        return quux_raise(context, "wrong-type", "count expects a collection, got a %s",
                          quux_kind_name(quux_kind_of(collection)));
    }
}

static const char *const error_field_names[QUUX_ERROR_FIELD_COUNT] = {
    "kind", "message", "data", "origin", "cause", "suppressed"};

bool quux_get(quux_context *context, quux_value collection, quux_value key,
              quux_value default_value, quux_value *result) {
    switch (quux_kind_of(collection)) {
    case QUUX_KIND_NOTHING:
        *result = default_value;
        return true;
    case QUUX_KIND_MAP:
    case QUUX_KIND_SET: {
        quux_value found;
        *result = quux_map_lookup(context, collection, key, &found) ? found : default_value;
        return true;
    }
    case QUUX_KIND_VECTOR: {
        if (quux_kind_of(key) != QUUX_KIND_INTEGER) {
            *result = default_value;
            return true;
        }
        int64_t index = quux_integer_value(key);
        *result = index >= 0 && (uint64_t)index < quux_vector_count(collection)
                      ? quux_vector_element(collection, (uint64_t)index)
                      : default_value;
        return true;
    }
    case QUUX_KIND_ERROR:
        if (quux_kind_of(key) == QUUX_KIND_KEYWORD) {
            for (int i = 0; i < QUUX_ERROR_FIELD_COUNT; i++) {
                if (quux_name_equals(key, error_field_names[i])) {
                    quux_value field = quux_error_field(collection, i);
                    if (quux_is_nothing(field) && i == QUUX_ERROR_SUPPRESSED) {
                        return quux_vector_empty(context, result);
                    }
                    *result = quux_is_nothing(field) ? default_value : field;
                    return true;
                }
            }
        }
        *result = default_value;
        return true;
    default:
        return quux_raise(context, "wrong-type", "get expects a map, set or vector, got a %s",
                          quux_kind_name(quux_kind_of(collection)));
    }
}

typedef struct list_builder {
    quux_context *context;
    quux_value *list; /* rooted by the caller */
    bool map_entries;
    bool failed;
} list_builder;

static bool prepend_entry(void *data, quux_value key, quux_value value) {
    list_builder *builder = data;
    quux_context *context = builder->context;
    quux_value element = key;
    quux_value entry = quux_nothing;
    QUUX_ROOT_FRAME(context, &key, &value, &element, &entry);
    bool done = true;
    if (builder->map_entries) {
        done = quux_vector_empty(context, &entry) && quux_vector_add(context, entry, key, &entry) &&
               quux_vector_add(context, entry, value, &entry);
        element = entry;
    }
    done = done && quux_list_prepend(context, element, *builder->list, builder->list);
    QUUX_ROOT_FRAME_END(context);
    builder->failed = !done;
    return done;
}

bool quux_as_list(quux_context *context, quux_value collection, quux_value *result) {
    quux_kind kind = quux_kind_of(collection);
    quux_value list = quux_nothing;
    bool done = true;
    QUUX_ROOT_FRAME(context, &collection, &list);
    switch (kind) {
    case QUUX_KIND_NOTHING:
    case QUUX_KIND_PAIR:
        list = collection;
        break;
    case QUUX_KIND_VECTOR:
        for (uint64_t i = quux_vector_count(collection); i > 0 && done; i--) {
            done = quux_list_prepend(context, quux_vector_element(collection, i - 1), list, &list);
        }
        break;
    case QUUX_KIND_MAP:
    case QUUX_KIND_SET: {
        list_builder builder = {context, &list, kind == QUUX_KIND_MAP, false};
        quux_map_for_each(collection, prepend_entry, &builder);
        done = !builder.failed && quux_list_reverse(context, list, &list);
        break;
    }
    default:
        done = quux_raise(context, "wrong-type", "expected a collection, got a %s",
                          quux_kind_name(kind));
        break;
    }
    if (done) {
        *result = list;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_add(quux_context *context, quux_value collection, quux_value value, quux_value *result) {
    switch (quux_kind_of(collection)) {
    case QUUX_KIND_NOTHING:
    case QUUX_KIND_PAIR:
        return quux_list_prepend(context, value, collection, result);
    case QUUX_KIND_VECTOR:
        return quux_vector_add(context, collection, value, result);
    case QUUX_KIND_SET:
        return quux_map_put(context, collection, value, value, result);
    case QUUX_KIND_MAP:
        if (quux_kind_of(value) == QUUX_KIND_VECTOR && quux_vector_count(value) == 2) {
            return quux_map_put(context, collection, quux_vector_element(value, 0),
                                quux_vector_element(value, 1), result);
        }
        return quux_raise(context, "wrong-type", "adding to a map expects a [key value] vector");
    default:
        return quux_raise(context, "wrong-type", "add expects a collection, got a %s",
                          quux_kind_name(quux_kind_of(collection)));
    }
}
