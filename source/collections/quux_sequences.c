/* Lists and the persistent vector (a 32-way bit-partitioned trie with a tail, D-36). */
#include "quux_collections.h"

#include <string.h>

/* ---- Lists ---- */

/* A pair: values [first, rest]; bytes: the list's count (uint64). */
bool quux_list_prepend(quux_context *context, quux_value value, quux_value list,
                       quux_value *result) {
    if (!quux_is_nothing(list) && !quux_is_kind(list, QUUX_KIND_PAIR)) {
        return quux_raise(context, "wrong-type", "prepend expects a list, got a %s",
                          quux_kind_name(quux_kind_of(list)));
    }
    QUUX_ROOT_FRAME(context, &value, &list);
    quux_object *pair = quux_allocate(context, QUUX_KIND_PAIR, 2, sizeof(uint64_t));
    QUUX_ROOT_FRAME_END(context);
    if (pair == NULL) {
        return false;
    }
    pair->values[QUUX_PAIR_FIRST] = value;
    pair->values[QUUX_PAIR_REST] = list;
    uint64_t count = quux_list_count(list) + 1;
    memcpy(quux_object_bytes(pair), &count, sizeof count);
    *result = quux_object_value(pair);
    return true;
}

uint64_t quux_list_count(quux_value list) {
    if (quux_is_nothing(list)) {
        return 0;
    }
    uint64_t count;
    memcpy(&count, quux_object_bytes(quux_checked_object(list)), sizeof count);
    return count;
}

bool quux_list_from_object_values(quux_context *context, quux_object *holder, uint32_t start,
                                  uint32_t count, quux_value *result) {
    quux_value list = quux_nothing;
    QUUX_ROOT_FRAME(context, &list);
    for (uint32_t i = count; i > 0; i--) {
        if (!quux_list_prepend(context, holder->values[start + i - 1], list, &list)) {
            QUUX_ROOT_FRAME_END(context);
            return false;
        }
    }
    QUUX_ROOT_FRAME_END(context);
    *result = list;
    return true;
}

bool quux_list_reverse(quux_context *context, quux_value list, quux_value *result) {
    quux_value reversed = quux_nothing;
    QUUX_ROOT_FRAME(context, &list, &reversed);
    while (!quux_is_nothing(list)) {
        if (!quux_list_prepend(context, quux_pair_first(list), reversed, &reversed)) {
            QUUX_ROOT_FRAME_END(context);
            return false;
        }
        list = quux_pair_rest(list);
    }
    QUUX_ROOT_FRAME_END(context);
    *result = reversed;
    return true;
}

/* ---- Vectors ----
 * Vector object: values [root, tail]; bytes: count (uint64), shift (uint32).
 * The root is a VECTOR_NODE (possibly with no children); interior nodes hold child nodes, leaf
 * nodes hold elements; every node holds only the slots in use, so equal vectors have equal
 * structure. The tail is a VECTOR_NODE with the last 1..32 elements (0 when empty). */

enum { QUUX_VECTOR_ROOT, QUUX_VECTOR_TAIL };
enum { QUUX_BRANCH_BITS = 5, QUUX_BRANCH_WIDTH = 32, QUUX_BRANCH_MASK = 31 };

typedef struct vector_shape {
    uint64_t count;
    uint32_t shift;
} vector_shape;

static vector_shape read_shape(quux_object *vector) {
    vector_shape shape;
    memcpy(&shape.count, quux_object_bytes(vector), sizeof shape.count);
    memcpy(&shape.shift, quux_object_bytes(vector) + sizeof shape.count, sizeof shape.shift);
    return shape;
}

static bool make_vector(quux_context *context, quux_value root, quux_value tail, uint64_t count,
                        uint32_t shift, quux_value *result) {
    QUUX_ROOT_FRAME(context, &root, &tail);
    quux_object *vector =
        quux_allocate(context, QUUX_KIND_VECTOR, 2, sizeof(uint64_t) + sizeof(uint32_t));
    QUUX_ROOT_FRAME_END(context);
    if (vector == NULL) {
        return false;
    }
    vector->values[QUUX_VECTOR_ROOT] = root;
    vector->values[QUUX_VECTOR_TAIL] = tail;
    memcpy(quux_object_bytes(vector), &count, sizeof count);
    memcpy(quux_object_bytes(vector) + sizeof count, &shift, sizeof shift);
    *result = quux_object_value(vector);
    return true;
}

/* A copy of `node` with `size` slots (extra slots are `nothing`). */
static bool copy_node(quux_context *context, quux_value node, uint32_t size, quux_value *result) {
    QUUX_ROOT_FRAME(context, &node);
    quux_object *copy = quux_allocate(context, QUUX_KIND_VECTOR_NODE, size, 0);
    QUUX_ROOT_FRAME_END(context);
    if (copy == NULL) {
        return false;
    }
    if (!quux_is_nothing(node)) {
        quux_object *original = quux_checked_object(node);
        uint32_t keep = original->value_count < size ? original->value_count : size;
        memcpy(copy->values, original->values, keep * sizeof(quux_value));
    }
    *result = quux_object_value(copy);
    return true;
}

bool quux_vector_empty(quux_context *context, quux_value *result) {
    quux_value root = quux_nothing;
    quux_value tail = quux_nothing;
    QUUX_ROOT_FRAME(context, &root, &tail);
    bool made = copy_node(context, quux_nothing, 0, &root) &&
                copy_node(context, quux_nothing, 0, &tail) &&
                make_vector(context, root, tail, 0, QUUX_BRANCH_BITS, result);
    QUUX_ROOT_FRAME_END(context);
    return made;
}

uint64_t quux_vector_count(quux_value vector) {
    return read_shape(quux_checked_object(vector)).count;
}

static uint64_t tail_offset(uint64_t count) {
    return count < QUUX_BRANCH_WIDTH ? 0 : ((count - 1) >> QUUX_BRANCH_BITS) << QUUX_BRANCH_BITS;
}

quux_value quux_vector_element(quux_value vector, uint64_t index) {
    quux_object *object = quux_checked_object(vector);
    vector_shape shape = read_shape(object);
    quux_assert(index < shape.count);
    if (index >= tail_offset(shape.count)) {
        return quux_checked_object(object->values[QUUX_VECTOR_TAIL])
            ->values[index & QUUX_BRANCH_MASK];
    }
    quux_object *node = quux_checked_object(object->values[QUUX_VECTOR_ROOT]);
    for (uint32_t level = shape.shift; level > 0; level -= QUUX_BRANCH_BITS) {
        node = quux_checked_object(node->values[(index >> level) & QUUX_BRANCH_MASK]);
    }
    return node->values[index & QUUX_BRANCH_MASK];
}

/* A chain of single-child nodes from `level` down to `node`. */
static bool new_path(quux_context *context, uint32_t level, quux_value node, quux_value *result) {
    quux_value current = node;
    QUUX_ROOT_FRAME(context, &current);
    for (uint32_t remaining = level; remaining > 0; remaining -= QUUX_BRANCH_BITS) {
        quux_value parent = quux_nothing;
        if (!copy_node(context, quux_nothing, 1, &parent)) {
            QUUX_ROOT_FRAME_END(context);
            return false;
        }
        quux_checked_object(parent)->values[0] = current;
        current = parent;
    }
    QUUX_ROOT_FRAME_END(context);
    *result = current;
    return true;
}

static bool push_tail(quux_context *context, uint64_t count, uint32_t level, quux_value parent,
                      quux_value tail, quux_value *result) {
    uint32_t index = (uint32_t)(((count - 1) >> level) & QUUX_BRANCH_MASK);
    quux_object *parent_object = quux_checked_object(parent);
    uint32_t size = index < parent_object->value_count ? parent_object->value_count : index + 1;
    quux_value copy = quux_nothing;
    quux_value inserted = quux_nothing;
    QUUX_ROOT_FRAME(context, &parent, &tail, &copy, &inserted);
    bool done = copy_node(context, parent, size, &copy);
    if (done) {
        if (level == QUUX_BRANCH_BITS) {
            inserted = tail;
        } else {
            quux_object *original = quux_checked_object(parent);
            quux_value child =
                index < original->value_count ? original->values[index] : quux_nothing;
            done =
                quux_is_nothing(child)
                    ? new_path(context, level - QUUX_BRANCH_BITS, tail, &inserted)
                    : push_tail(context, count, level - QUUX_BRANCH_BITS, child, tail, &inserted);
        }
    }
    if (done) {
        quux_checked_object(copy)->values[index] = inserted;
        *result = copy;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_vector_add(quux_context *context, quux_value vector, quux_value value,
                     quux_value *result) {
    quux_object *object = quux_checked_object(vector);
    vector_shape shape = read_shape(object);
    quux_value root = object->values[QUUX_VECTOR_ROOT];
    quux_value tail = object->values[QUUX_VECTOR_TAIL];
    quux_value new_tail = quux_nothing;
    quux_value new_root = quux_nothing;
    quux_value path = quux_nothing;
    QUUX_ROOT_FRAME(context, &vector, &value, &root, &tail, &new_tail, &new_root, &path);
    bool done;
    uint32_t tail_size = quux_checked_object(tail)->value_count;
    if (tail_size < QUUX_BRANCH_WIDTH) {
        done = copy_node(context, tail, tail_size + 1, &new_tail);
        if (done) {
            quux_checked_object(new_tail)->values[tail_size] = value;
            done = make_vector(context, root, new_tail, shape.count + 1, shape.shift, result);
        }
    } else {
        uint32_t shift = shape.shift;
        if ((shape.count >> QUUX_BRANCH_BITS) > (UINT64_C(1) << shift)) {
            /* The root is full: grow the tree by one level. */
            done = new_path(context, shift, tail, &path) &&
                   copy_node(context, quux_nothing, 2, &new_root);
            if (done) {
                quux_checked_object(new_root)->values[0] = root;
                quux_checked_object(new_root)->values[1] = path;
            }
            shift += QUUX_BRANCH_BITS;
        } else {
            done = push_tail(context, shape.count, shift, root, tail, &new_root);
        }
        done = done && copy_node(context, quux_nothing, 1, &new_tail);
        if (done) {
            quux_checked_object(new_tail)->values[0] = value;
            done = make_vector(context, new_root, new_tail, shape.count + 1, shift, result);
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool assoc_in_tree(quux_context *context, uint32_t level, quux_value node, uint64_t index,
                          quux_value value, quux_value *result) {
    quux_value copy = quux_nothing;
    quux_value child = quux_nothing;
    QUUX_ROOT_FRAME(context, &node, &value, &copy, &child);
    bool done = copy_node(context, node, quux_checked_object(node)->value_count, &copy);
    if (done) {
        uint32_t slot = (uint32_t)((index >> level) & QUUX_BRANCH_MASK);
        if (level == 0) {
            quux_checked_object(copy)->values[slot] = value;
        } else {
            done = assoc_in_tree(context, level - QUUX_BRANCH_BITS,
                                 quux_checked_object(node)->values[slot], index, value, &child);
            if (done) {
                quux_checked_object(copy)->values[slot] = child;
            }
        }
    }
    if (done) {
        *result = copy;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_vector_put(quux_context *context, quux_value vector, uint64_t index, quux_value value,
                     quux_value *result) {
    quux_object *object = quux_checked_object(vector);
    vector_shape shape = read_shape(object);
    if (index == shape.count) {
        return quux_vector_add(context, vector, value, result);
    }
    if (index > shape.count) {
        return quux_raise(context, "index-out-of-range", "index %llu outside a vector of %llu",
                          (unsigned long long)index, (unsigned long long)shape.count);
    }
    quux_value replaced = quux_nothing;
    QUUX_ROOT_FRAME(context, &vector, &value, &replaced);
    bool done;
    if (index >= tail_offset(shape.count)) {
        quux_value tail = quux_checked_object(vector)->values[QUUX_VECTOR_TAIL];
        done = copy_node(context, tail, quux_checked_object(tail)->value_count, &replaced);
        if (done) {
            quux_checked_object(replaced)->values[index & QUUX_BRANCH_MASK] = value;
            done = make_vector(context, quux_checked_object(vector)->values[QUUX_VECTOR_ROOT],
                               replaced, shape.count, shape.shift, result);
        }
    } else {
        done = assoc_in_tree(context, shape.shift,
                             quux_checked_object(vector)->values[QUUX_VECTOR_ROOT], index, value,
                             &replaced) &&
               make_vector(context, replaced, quux_checked_object(vector)->values[QUUX_VECTOR_TAIL],
                           shape.count, shape.shift, result);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}
