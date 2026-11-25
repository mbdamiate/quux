/* CHAMP maps and sets (Steindorfer and Vinju, OOPSLA 2015), canonical after any sequence of
 * insertions and removals (section 02, D-36).
 *
 * Map or set object: values [root]; bytes: count (uint64).
 * Node: values [entries…, children…]; bytes: data bitmap, node bitmap, width (uint32 each).
 *   width is 2 for maps (key, value) and 1 for sets (element).
 * Collision node (all hash bits used): values [entries…] sorted by the canonical order; bytes:
 *   width. A collision node always holds at least two entries. */
#include "quux_collections.h"

#include <string.h>

enum { QUUX_LEVEL_BITS = 5, QUUX_LEVEL_MASK = 31, QUUX_HASH_BITS = 64 };

typedef struct node_shape {
    uint32_t data_bitmap;
    uint32_t node_bitmap;
    uint32_t width;
} node_shape;

static node_shape read_node_shape(quux_object *node) {
    node_shape shape = {0, 0, 0};
    unsigned char *bytes = quux_object_bytes(node);
    if (node->kind == QUUX_KIND_COLLISION_NODE) {
        memcpy(&shape.width, bytes, sizeof shape.width);
    } else {
        memcpy(&shape.data_bitmap, bytes, sizeof shape.data_bitmap);
        memcpy(&shape.node_bitmap, bytes + 4, sizeof shape.node_bitmap);
        memcpy(&shape.width, bytes + 8, sizeof shape.width);
    }
    return shape;
}

static uint32_t population(uint32_t bits) {
    return (uint32_t)__builtin_popcount(bits);
}

static uint32_t payload_count(quux_object *node, node_shape shape) {
    if (node->kind == QUUX_KIND_COLLISION_NODE) {
        return node->value_count / shape.width;
    }
    return population(shape.data_bitmap);
}

static bool make_node(quux_context *context, uint32_t data_bitmap, uint32_t node_bitmap,
                      uint32_t width, uint32_t value_count, quux_object **result) {
    quux_object *node = quux_allocate(context, QUUX_KIND_CHAMP_NODE, value_count, 12);
    if (node == NULL) {
        return false;
    }
    unsigned char *bytes = quux_object_bytes(node);
    memcpy(bytes, &data_bitmap, 4);
    memcpy(bytes + 4, &node_bitmap, 4);
    memcpy(bytes + 8, &width, 4);
    *result = node;
    return true;
}

static bool make_collision_node(quux_context *context, uint32_t width, uint32_t value_count,
                                quux_object **result) {
    quux_object *node = quux_allocate(context, QUUX_KIND_COLLISION_NODE, value_count, 4);
    if (node == NULL) {
        return false;
    }
    memcpy(quux_object_bytes(node), &width, 4);
    *result = node;
    return true;
}

static uint32_t fragment(uint64_t hash, uint32_t shift) {
    return (uint32_t)((hash >> shift) & QUUX_LEVEL_MASK);
}

static bool make_collection(quux_context *context, quux_kind kind, quux_value root, uint64_t count,
                            quux_value *result) {
    QUUX_ROOT_FRAME(context, &root);
    quux_object *collection = quux_allocate(context, kind, 1, sizeof(uint64_t));
    QUUX_ROOT_FRAME_END(context);
    if (collection == NULL) {
        return false;
    }
    collection->values[0] = root;
    memcpy(quux_object_bytes(collection), &count, sizeof count);
    *result = quux_object_value(collection);
    return true;
}

static bool empty_collection(quux_context *context, quux_kind kind, uint32_t width,
                             quux_value *result) {
    quux_object *root = NULL;
    if (!make_node(context, 0, 0, width, 0, &root)) {
        return false;
    }
    return make_collection(context, kind, quux_object_value(root), 0, result);
}

bool quux_map_empty(quux_context *context, quux_value *result) {
    return empty_collection(context, QUUX_KIND_MAP, 2, result);
}

bool quux_set_empty(quux_context *context, quux_value *result) {
    return empty_collection(context, QUUX_KIND_SET, 1, result);
}

uint64_t quux_map_count(quux_value map) {
    uint64_t count;
    memcpy(&count, quux_object_bytes(quux_checked_object(map)), sizeof count);
    return count;
}

/* ---- Lookup ---- */

bool quux_map_lookup(quux_context *context, quux_value map, quux_value key, quux_value *value) {
    uint64_t hash = quux_value_hash(context, key);
    quux_object *node = quux_checked_object(quux_checked_object(map)->values[0]);
    for (uint32_t shift = 0;; shift += QUUX_LEVEL_BITS) {
        node_shape shape = read_node_shape(node);
        if (node->kind == QUUX_KIND_COLLISION_NODE) {
            for (uint32_t i = 0; i < node->value_count; i += shape.width) {
                if (quux_equal(context, node->values[i], key)) {
                    *value = node->values[i + shape.width - 1];
                    return true;
                }
            }
            return false;
        }
        uint32_t bit = UINT32_C(1) << fragment(hash, shift);
        if (shape.data_bitmap & bit) {
            uint32_t index = population(shape.data_bitmap & (bit - 1)) * shape.width;
            if (quux_equal(context, node->values[index], key)) {
                *value = node->values[index + shape.width - 1];
                return true;
            }
            return false;
        }
        if (!(shape.node_bitmap & bit)) {
            return false;
        }
        uint32_t child =
            payload_count(node, shape) * shape.width + population(shape.node_bitmap & (bit - 1));
        node = quux_checked_object(node->values[child]);
    }
}

/* ---- Insertion ---- */

/* Inserts into a sorted collision node, or replaces an equal key. */
static bool collision_put(quux_context *context, quux_value node, quux_value key, quux_value value,
                          quux_value *result, bool *added) {
    quux_object *original = quux_checked_object(node);
    uint32_t width = read_node_shape(original).width;
    uint32_t count = original->value_count / width;
    uint32_t position = count;
    for (uint32_t i = 0; i < count; i++) {
        int order = quux_canonical_compare(context, key, original->values[i * width]);
        if (order == 0) {
            if (width == 1 || quux_equal(context, original->values[i * width + 1], value)) {
                *result = node;
                *added = false;
                return true;
            }
            quux_object *copy = NULL;
            QUUX_ROOT_FRAME(context, &node, &value);
            bool made = make_collision_node(context, width, original->value_count, &copy);
            QUUX_ROOT_FRAME_END(context);
            if (!made) {
                return false;
            }
            original = quux_checked_object(node);
            memcpy(copy->values, original->values, original->value_count * sizeof(quux_value));
            copy->values[i * width + 1] = value;
            *result = quux_object_value(copy);
            *added = false;
            return true;
        }
        if (order < 0) {
            position = i;
            break;
        }
    }
    quux_object *grown = NULL;
    QUUX_ROOT_FRAME(context, &node, &key, &value);
    bool made = make_collision_node(context, width, original->value_count + width, &grown);
    QUUX_ROOT_FRAME_END(context);
    if (!made) {
        return false;
    }
    original = quux_checked_object(node);
    memcpy(grown->values, original->values, position * width * sizeof(quux_value));
    grown->values[position * width] = key;
    if (width == 2) {
        grown->values[position * width + 1] = value;
    }
    memcpy(grown->values + (position + 1) * width, original->values + position * width,
           (count - position) * width * sizeof(quux_value));
    *result = quux_object_value(grown);
    *added = true;
    return true;
}

/* A node holding two entries whose hashes agree up to `shift`. */
static bool merge_entries(quux_context *context, uint32_t width, quux_value key1, quux_value value1,
                          uint64_t hash1, quux_value key2, quux_value value2, uint64_t hash2,
                          uint32_t shift, quux_value *result) {
    quux_value child = quux_nothing;
    QUUX_ROOT_FRAME(context, &key1, &value1, &key2, &value2, &child);
    bool done;
    if (shift >= QUUX_HASH_BITS) {
        quux_object *collision = NULL;
        done = make_collision_node(context, width, 2 * width, &collision);
        if (done) {
            bool first_is_key1 = quux_canonical_compare(context, key1, key2) < 0;
            quux_value first_key = first_is_key1 ? key1 : key2;
            quux_value second_key = first_is_key1 ? key2 : key1;
            collision->values[0] = first_key;
            collision->values[width] = second_key;
            if (width == 2) {
                collision->values[1] = first_is_key1 ? value1 : value2;
                collision->values[3] = first_is_key1 ? value2 : value1;
            }
            *result = quux_object_value(collision);
        }
    } else {
        uint32_t fragment1 = fragment(hash1, shift);
        uint32_t fragment2 = fragment(hash2, shift);
        if (fragment1 != fragment2) {
            quux_object *node = NULL;
            done = make_node(context, (UINT32_C(1) << fragment1) | (UINT32_C(1) << fragment2), 0,
                             width, 2 * width, &node);
            if (done) {
                uint32_t first = fragment1 < fragment2 ? 0 : width;
                uint32_t second = width - first;
                node->values[first] = key1;
                node->values[second] = key2;
                if (width == 2) {
                    node->values[first + 1] = value1;
                    node->values[second + 1] = value2;
                }
                *result = quux_object_value(node);
            }
        } else {
            done = merge_entries(context, width, key1, value1, hash1, key2, value2, hash2,
                                 shift + QUUX_LEVEL_BITS, &child);
            quux_object *node = NULL;
            done = done && make_node(context, 0, UINT32_C(1) << fragment1, width, 1, &node);
            if (done) {
                node->values[0] = child;
                *result = quux_object_value(node);
            }
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool node_put(quux_context *context, quux_value node, quux_value key, quux_value value,
                     uint64_t hash, uint32_t shift, quux_value *result, bool *added) {
    quux_object *original = quux_checked_object(node);
    if (original->kind == QUUX_KIND_COLLISION_NODE) {
        return collision_put(context, node, key, value, result, added);
    }
    node_shape shape = read_node_shape(original);
    uint32_t width = shape.width;
    uint32_t bit = UINT32_C(1) << fragment(hash, shift);
    uint32_t payload = population(shape.data_bitmap);
    quux_value replacement = quux_nothing;
    quux_object *copy = NULL;
    bool done = true;
    QUUX_ROOT_FRAME(context, &node, &key, &value, &replacement);
    if (shape.data_bitmap & bit) {
        uint32_t index = population(shape.data_bitmap & (bit - 1)) * width;
        quux_value existing_key = original->values[index];
        if (quux_equal(context, existing_key, key)) {
            if (width == 1 || quux_equal(context, original->values[index + 1], value)) {
                *result = node;
                *added = false;
            } else {
                done = make_node(context, shape.data_bitmap, shape.node_bitmap, width,
                                 original->value_count, &copy);
                if (done) {
                    original = quux_checked_object(node);
                    memcpy(copy->values, original->values,
                           original->value_count * sizeof(quux_value));
                    copy->values[index + 1] = value;
                    *result = quux_object_value(copy);
                    *added = false;
                }
            }
        } else {
            /* Push the existing entry and the new one down into a new child node. */
            quux_value existing_value = original->values[index + width - 1];
            uint64_t existing_hash = quux_value_hash(context, existing_key);
            done = merge_entries(context, width, existing_key, existing_value, existing_hash, key,
                                 value, hash, shift + QUUX_LEVEL_BITS, &replacement);
            done = done && make_node(context, shape.data_bitmap & ~bit, shape.node_bitmap | bit,
                                     width, original->value_count - width + 1, &copy);
            if (done) {
                original = quux_checked_object(node);
                uint32_t children = population(shape.node_bitmap);
                uint32_t child_index = population(shape.node_bitmap & (bit - 1));
                uint32_t new_payload_end = (payload - 1) * width;
                /* entries before and after the removed one */
                memcpy(copy->values, original->values, index * sizeof(quux_value));
                memcpy(copy->values + index, original->values + index + width,
                       (payload * width - index - width) * sizeof(quux_value));
                /* children with the new child inserted at child_index */
                memcpy(copy->values + new_payload_end, original->values + payload * width,
                       child_index * sizeof(quux_value));
                copy->values[new_payload_end + child_index] = replacement;
                memcpy(copy->values + new_payload_end + child_index + 1,
                       original->values + payload * width + child_index,
                       (children - child_index) * sizeof(quux_value));
                *result = quux_object_value(copy);
                *added = true;
            }
        }
    } else if (shape.node_bitmap & bit) {
        uint32_t child_slot = payload * width + population(shape.node_bitmap & (bit - 1));
        done = node_put(context, original->values[child_slot], key, value, hash,
                        shift + QUUX_LEVEL_BITS, &replacement, added);
        if (done) {
            original = quux_checked_object(node);
            if (replacement == original->values[child_slot]) {
                *result = node;
            } else {
                done = make_node(context, shape.data_bitmap, shape.node_bitmap, width,
                                 original->value_count, &copy);
                if (done) {
                    original = quux_checked_object(node);
                    memcpy(copy->values, original->values,
                           original->value_count * sizeof(quux_value));
                    copy->values[child_slot] = replacement;
                    *result = quux_object_value(copy);
                }
            }
        }
    } else {
        uint32_t index = population(shape.data_bitmap & (bit - 1)) * width;
        done = make_node(context, shape.data_bitmap | bit, shape.node_bitmap, width,
                         original->value_count + width, &copy);
        if (done) {
            original = quux_checked_object(node);
            memcpy(copy->values, original->values, index * sizeof(quux_value));
            copy->values[index] = key;
            if (width == 2) {
                copy->values[index + 1] = value;
            }
            memcpy(copy->values + index + width, original->values + index,
                   (original->value_count - index) * sizeof(quux_value));
            *result = quux_object_value(copy);
            *added = true;
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_map_put(quux_context *context, quux_value map, quux_value key, quux_value value,
                  quux_value *result) {
    quux_object *collection = quux_checked_object(map);
    quux_kind kind = (quux_kind)collection->kind;
    if (kind == QUUX_KIND_SET) {
        value = key;
    }
    uint64_t hash = quux_value_hash(context, key);
    quux_value root = quux_nothing;
    bool added = false;
    QUUX_ROOT_FRAME(context, &map, &key, &value, &root);
    bool done =
        node_put(context, quux_checked_object(map)->values[0], key, value, hash, 0, &root, &added);
    if (done) {
        if (root == quux_checked_object(map)->values[0]) {
            *result = map;
        } else {
            done =
                make_collection(context, kind, root, quux_map_count(map) + (added ? 1 : 0), result);
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- Removal with canonical compaction ---- */

typedef enum removal_outcome {
    REMOVAL_NOT_FOUND,
    REMOVAL_NODE,         /* result is the new node */
    REMOVAL_SINGLE_ENTRY, /* the node now holds one entry and no children: inline it */
} removal_outcome;

static bool node_remove(quux_context *context, quux_value node, quux_value key, uint64_t hash,
                        uint32_t shift, quux_value *result, quux_value *single_key,
                        quux_value *single_value, removal_outcome *outcome);

static bool collision_remove(quux_context *context, quux_value node, quux_value key,
                             quux_value *result, quux_value *single_key, quux_value *single_value,
                             removal_outcome *outcome) {
    quux_object *original = quux_checked_object(node);
    uint32_t width = read_node_shape(original).width;
    uint32_t count = original->value_count / width;
    for (uint32_t i = 0; i < count; i++) {
        if (!quux_equal(context, original->values[i * width], key)) {
            continue;
        }
        if (count == 2) {
            uint32_t other = (1 - i) * width;
            *single_key = original->values[other];
            *single_value = original->values[other + width - 1];
            *outcome = REMOVAL_SINGLE_ENTRY;
            return true;
        }
        quux_object *shrunk = NULL;
        QUUX_ROOT_FRAME(context, &node);
        bool made = make_collision_node(context, width, original->value_count - width, &shrunk);
        QUUX_ROOT_FRAME_END(context);
        if (!made) {
            return false;
        }
        original = quux_checked_object(node);
        memcpy(shrunk->values, original->values, i * width * sizeof(quux_value));
        memcpy(shrunk->values + i * width, original->values + (i + 1) * width,
               (count - i - 1) * width * sizeof(quux_value));
        *result = quux_object_value(shrunk);
        *outcome = REMOVAL_NODE;
        return true;
    }
    *outcome = REMOVAL_NOT_FOUND;
    return true;
}

static bool node_remove(quux_context *context, quux_value node, quux_value key, uint64_t hash,
                        uint32_t shift, quux_value *result, quux_value *single_key,
                        quux_value *single_value, removal_outcome *outcome) {
    quux_object *original = quux_checked_object(node);
    if (original->kind == QUUX_KIND_COLLISION_NODE) {
        return collision_remove(context, node, key, result, single_key, single_value, outcome);
    }
    node_shape shape = read_node_shape(original);
    uint32_t width = shape.width;
    uint32_t bit = UINT32_C(1) << fragment(hash, shift);
    uint32_t payload = population(shape.data_bitmap);
    uint32_t children = population(shape.node_bitmap);
    quux_object *copy = NULL;
    bool done = true;
    *outcome = REMOVAL_NOT_FOUND;
    if (shape.data_bitmap & bit) {
        uint32_t index = population(shape.data_bitmap & (bit - 1)) * width;
        if (!quux_equal(context, original->values[index], key)) {
            return true;
        }
        if (payload == 2 && children == 0 && shift > 0) {
            /* The remaining entry moves up into the parent (canonical form). */
            uint32_t other = index == 0 ? width : 0;
            *single_key = original->values[other];
            *single_value = original->values[other + width - 1];
            *outcome = REMOVAL_SINGLE_ENTRY;
            return true;
        }
        QUUX_ROOT_FRAME(context, &node);
        done = make_node(context, shape.data_bitmap & ~bit, shape.node_bitmap, width,
                         original->value_count - width, &copy);
        QUUX_ROOT_FRAME_END(context);
        if (done) {
            original = quux_checked_object(node);
            memcpy(copy->values, original->values, index * sizeof(quux_value));
            memcpy(copy->values + index, original->values + index + width,
                   (original->value_count - index - width) * sizeof(quux_value));
            *result = quux_object_value(copy);
            *outcome = REMOVAL_NODE;
        }
        return done;
    }
    if (!(shape.node_bitmap & bit)) {
        return true;
    }
    uint32_t child_position = population(shape.node_bitmap & (bit - 1));
    uint32_t child_slot = payload * width + child_position;
    quux_value child = quux_nothing;
    quux_value moved_key = quux_nothing;
    quux_value moved_value = quux_nothing;
    removal_outcome child_outcome = REMOVAL_NOT_FOUND;
    QUUX_ROOT_FRAME(context, &node, &child, &moved_key, &moved_value);
    done = node_remove(context, original->values[child_slot], key, hash, shift + QUUX_LEVEL_BITS,
                       &child, &moved_key, &moved_value, &child_outcome);
    if (done && child_outcome == REMOVAL_NODE) {
        original = quux_checked_object(node);
        done = make_node(context, shape.data_bitmap, shape.node_bitmap, width,
                         original->value_count, &copy);
        if (done) {
            original = quux_checked_object(node);
            memcpy(copy->values, original->values, original->value_count * sizeof(quux_value));
            copy->values[child_slot] = child;
            *result = quux_object_value(copy);
            *outcome = REMOVAL_NODE;
        }
    } else if (done && child_outcome == REMOVAL_SINGLE_ENTRY) {
        if (payload == 0 && children == 1 && shift > 0) {
            /* This node would hold only that entry: pass it further up. */
            *single_key = moved_key;
            *single_value = moved_value;
            *outcome = REMOVAL_SINGLE_ENTRY;
        } else {
            /* Inline the entry: remove the child, insert the entry among the data entries. */
            original = quux_checked_object(node);
            done = make_node(context, shape.data_bitmap | bit, shape.node_bitmap & ~bit, width,
                             original->value_count + width - 1, &copy);
            if (done) {
                original = quux_checked_object(node);
                uint32_t index = population(shape.data_bitmap & (bit - 1)) * width;
                uint32_t old_payload_end = payload * width;
                memcpy(copy->values, original->values, index * sizeof(quux_value));
                copy->values[index] = moved_key;
                if (width == 2) {
                    copy->values[index + 1] = moved_value;
                }
                memcpy(copy->values + index + width, original->values + index,
                       (old_payload_end - index) * sizeof(quux_value));
                uint32_t new_payload_end = old_payload_end + width;
                memcpy(copy->values + new_payload_end, original->values + old_payload_end,
                       child_position * sizeof(quux_value));
                memcpy(copy->values + new_payload_end + child_position,
                       original->values + old_payload_end + child_position + 1,
                       (children - child_position - 1) * sizeof(quux_value));
                *result = quux_object_value(copy);
                *outcome = REMOVAL_NODE;
            }
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_map_remove(quux_context *context, quux_value map, quux_value key, quux_value *result) {
    uint64_t hash = quux_value_hash(context, key);
    quux_value root = quux_nothing;
    quux_value single_key = quux_nothing;
    quux_value single_value = quux_nothing;
    removal_outcome outcome = REMOVAL_NOT_FOUND;
    QUUX_ROOT_FRAME(context, &map, &root, &single_key, &single_value);
    bool done = node_remove(context, quux_checked_object(map)->values[0], key, hash, 0, &root,
                            &single_key, &single_value, &outcome);
    if (done) {
        if (outcome == REMOVAL_NOT_FOUND) {
            *result = map;
        } else {
            quux_assert(outcome == REMOVAL_NODE);
            done = make_collection(context, (quux_kind)quux_checked_object(map)->kind, root,
                                   quux_map_count(map) - 1, result);
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

/* ---- Iteration ---- */

static bool visit_node(quux_object *node, quux_entry_visitor visitor, void *data) {
    node_shape shape = read_node_shape(node);
    uint32_t width = shape.width;
    uint32_t payload = payload_count(node, shape);
    for (uint32_t i = 0; i < payload; i++) {
        if (!visitor(data, node->values[i * width], node->values[i * width + width - 1])) {
            return false;
        }
    }
    for (uint32_t i = payload * width; i < node->value_count; i++) {
        if (!visit_node(quux_checked_object(node->values[i]), visitor, data)) {
            return false;
        }
    }
    return true;
}

bool quux_map_for_each(quux_value map, quux_entry_visitor visitor, void *data) {
    return visit_node(quux_checked_object(quux_checked_object(map)->values[0]), visitor, data);
}
