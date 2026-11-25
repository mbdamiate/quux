/* Reuse: keep. Persistent canonical collections (D-36, section 02): linked lists, a 32-way vector
 * trie with a tail, CHAMP maps and sets with canonical removal and canonical collision nodes,
 * plus structural equality, the value hash and a canonical total order (section 01). */
#ifndef QUUX_COLLECTIONS_H
#define QUUX_COLLECTIONS_H

#include "quux_values.h"

#include <stdbool.h>
#include <stdint.h>

/* ---- Lists (the empty list is `nothing`) ---- */

enum { QUUX_PAIR_FIRST, QUUX_PAIR_REST };

bool quux_list_prepend(quux_context *context, quux_value value, quux_value list,
                       quux_value *result);
static inline quux_value quux_pair_first(quux_value pair) {
    return quux_checked_object(pair)->values[QUUX_PAIR_FIRST];
}
static inline quux_value quux_pair_rest(quux_value pair) {
    return quux_checked_object(pair)->values[QUUX_PAIR_REST];
}
uint64_t quux_list_count(quux_value list);
/* Builds a list from values that live in rooted memory (for example a rooted object). */
bool quux_list_from_object_values(quux_context *context, quux_object *holder, uint32_t start,
                                  uint32_t count, quux_value *result);
/* Reverses a list (used when building lists front to back). */
bool quux_list_reverse(quux_context *context, quux_value list, quux_value *result);

/* ---- Vectors ---- */

bool quux_vector_empty(quux_context *context, quux_value *result);
bool quux_vector_add(quux_context *context, quux_value vector, quux_value value,
                     quux_value *result);
/* Replaces the element at index (index < count), or appends when index == count. */
bool quux_vector_put(quux_context *context, quux_value vector, uint64_t index, quux_value value,
                     quux_value *result);
uint64_t quux_vector_count(quux_value vector);
quux_value quux_vector_element(quux_value vector, uint64_t index);

/* ---- Maps and sets (CHAMP) ---- */

bool quux_map_empty(quux_context *context, quux_value *result);
bool quux_set_empty(quux_context *context, quux_value *result);
/* For a set, `value` is ignored. */
bool quux_map_put(quux_context *context, quux_value map, quux_value key, quux_value value,
                  quux_value *result);
bool quux_map_remove(quux_context *context, quux_value map, quux_value key, quux_value *result);
/* Looks up a key; for a set the found value is the stored element. */
bool quux_map_lookup(quux_context *context, quux_value map, quux_value key, quux_value *value);
uint64_t quux_map_count(quux_value map);

/* Visits entries in canonical order (a function of the contents only). Return false from the
 * visitor to stop. For a set, value is the element too. The visitor may allocate only if the
 * caller roots what it needs; the collection itself must be rooted by the caller. */
typedef bool (*quux_entry_visitor)(void *data, quux_value key, quux_value value);
bool quux_map_for_each(quux_value map, quux_entry_visitor visitor, void *data);

/* ---- Generic operations ---- */

/* Whether the value is a list (pair or nothing) or a vector. */
bool quux_is_sequential(quux_value value);
/* Number of elements of a list, vector, map, set or string; raises :wrong-type otherwise. */
bool quux_count(quux_context *context, quux_value collection, uint64_t *count);
/* (get collection key default) for maps, sets, vectors (integer keys), errors and nothing. */
bool quux_get(quux_context *context, quux_value collection, quux_value key,
              quux_value default_value, quux_value *result);
/* An ordered list view: lists and vectors in order, maps as [key value] vectors and sets as
 * elements, in canonical order. */
bool quux_as_list(quux_context *context, quux_value collection, quux_value *result);
/* (add collection value): conj on sets, vectors (end), lists (front) and maps ([k v] entries). */
bool quux_add(quux_context *context, quux_value collection, quux_value value, quux_value *result);

/* ---- Equality, hashing, order (section 01) ---- */

bool quux_equal(quux_context *context, quux_value left, quux_value right);
uint64_t quux_value_hash(quux_context *context, quux_value value);
/* A total order consistent with equal? (0 exactly when equal?). Used for canonical collision
 * nodes. */
int quux_canonical_compare(quux_context *context, quux_value left, quux_value right);

/* Hooks installed by the evaluator for function values (D-19). */
typedef struct quux_function_hooks {
    bool (*equal)(quux_context *context, quux_value left, quux_value right);
    uint64_t (*hash)(quux_context *context, quux_value function);
} quux_function_hooks;
void quux_collections_set_function_hooks(const quux_function_hooks *hooks);

#endif
