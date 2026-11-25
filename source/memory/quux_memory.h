/* Reuse: adapt. Allocation, precise roots (root frames on a per-task shadow stack, global roots),
 * the write barrier and the root verifier (D-34, contracts/root-convention.md). Phase 3 replaces
 * the verifier with a collector; the frame macros and the barrier keep their meaning. */
#ifndef QUUX_MEMORY_H
#define QUUX_MEMORY_H

#include "quux_portability.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A quux value is a tagged 64-bit word (D-35). Low 3 bits: 000 heap object pointer, 001 small
 * integer, 010 constant. The memory module only needs to know which words are pointers. */
typedef uint64_t quux_value;

enum {
    QUUX_TAG_MASK = 7u,
    QUUX_TAG_OBJECT = 0u,
    QUUX_TAG_SMALL_INTEGER = 1u,
    QUUX_TAG_CONSTANT = 2u,
};

enum {
    QUUX_OBJECT_FLAG_HASHED = 1u << 0,         /* value_hash is valid */
    QUUX_OBJECT_FLAG_MARKED = 1u << 1,         /* reached by the verifier */
    QUUX_OBJECT_FLAG_LOCAL_RESOURCE = 1u << 2, /* cannot leave its node (D-20) */
    QUUX_OBJECT_FLAG_HAS_IDENTITY = 1u << 3,   /* compared and hashed by identity (D-02) */
};

/* Every heap object: a header, then value_count quux values (traced), then byte_count raw
 * bytes (not traced). Fields follow data-model.md "Heap object header". */
typedef struct quux_object {
    uint32_t kind;
    uint32_t flags;
    uint32_t value_count;
    uint32_t byte_count;
    uint64_t value_hash;
    unsigned char *content_hash; /* 32 bytes, computed lazily; owned by the object */
    struct quux_object *next_allocation;
    quux_value values[];
} quux_object;

static inline bool quux_value_is_object(quux_value value) {
    return (value & QUUX_TAG_MASK) == QUUX_TAG_OBJECT && value != 0;
}

static inline quux_object *quux_value_object(quux_value value) {
    return (quux_object *)(uintptr_t)value;
}

static inline quux_value quux_object_value(const quux_object *object) {
    return (quux_value)(uintptr_t)object;
}

static inline unsigned char *quux_object_bytes(quux_object *object) {
    return (unsigned char *)&object->values[object->value_count];
}

/* ---- Root frames (data-model.md "Root frame") ---- */

typedef struct quux_root_frame {
    struct quux_root_frame *previous;
    size_t count;
    quux_value **slots;
    const char *function_name;
} quux_root_frame;

/* The roots of one task: its frame stack plus a fixed set of slots (the task's context fields). */
typedef struct quux_root_stack {
    quux_root_frame *top;
    quux_value *fixed_slots;
    size_t fixed_count;
    struct quux_root_stack *next_registered;
    struct quux_root_stack *previous_registered;
} quux_root_stack;

/* Opens a root frame that registers the addresses of the given quux_value variables. One frame
 * per C block; every exit path must close it with QUUX_ROOT_FRAME_END. */
#define QUUX_ROOT_FRAME(context, ...)                                                              \
    quux_value *quux_root_frame_slots[] = {__VA_ARGS__};                                           \
    quux_root_frame quux_root_frame_record = {                                                     \
        (context)->roots.top, sizeof quux_root_frame_slots / sizeof quux_root_frame_slots[0],      \
        quux_root_frame_slots, __func__};                                                          \
    (context)->roots.top = &quux_root_frame_record

#define QUUX_ROOT_FRAME_END(context)                                                               \
    quux_memory_close_frame(&(context)->roots, &quux_root_frame_record)

/* Closes a frame; in debug builds aborts, naming the function, if an inner frame was left open. */
void quux_memory_close_frame(quux_root_stack *roots, quux_root_frame *frame);

/* ---- The heap ---- */

typedef void (*quux_root_enumerator)(void *data, void (*mark)(void *mark_state, quux_value),
                                     void *mark_state);

typedef enum quux_verify_mode {
    QUUX_VERIFY_OFF,
    QUUX_VERIFY_EVERY_ALLOCATION,
    QUUX_VERIFY_AMORTIZED, /* every max(1, live / 4) allocations */
} quux_verify_mode;

typedef struct quux_heap quux_heap;

quux_heap *quux_memory_create_heap(void);
/* Frees every allocation (rule 8 of the root convention) and the heap itself. */
void quux_memory_shutdown(quux_heap *heap);

/* Allocates an object with value_count values (initialized to `nothing`, 0x02) and byte_count
 * zeroed bytes. Returns NULL when the allocation fails or exceeds the test-only limit; the
 * caller turns that into :resource-exhausted. */
quux_object *quux_memory_allocate(quux_heap *heap, quux_root_stack *roots, uint32_t kind,
                                  uint32_t value_count, uint32_t byte_count);

void quux_memory_register_global_root(quux_heap *heap, quux_value *slot);
void quux_memory_register_root_enumerator(quux_heap *heap, quux_root_enumerator enumerator,
                                          void *data);

/* Root stacks of running tasks. The verifier runs only while exactly one is registered. */
void quux_memory_register_root_stack(quux_heap *heap, quux_root_stack *roots);
void quux_memory_unregister_root_stack(quux_heap *heap, quux_root_stack *roots);

/* Every store into a mutable cell calls this before the store becomes visible (rule 5).
 * A no-op in Phase 1. */
void quux_memory_write_barrier(quux_heap *heap, quux_object *owner, quux_value value);

void quux_memory_set_verify_mode(quux_heap *heap, quux_verify_mode mode);
/* Runs the verifier now (if exactly one root stack is registered). Returns the number of
 * objects released. */
size_t quux_memory_verify_roots(quux_heap *heap);
/* Test support: the allocation after `remaining` more allocations fails. 0 removes the limit. */
void quux_memory_set_allocation_limit(quux_heap *heap, uint64_t remaining);
size_t quux_memory_live_object_count(quux_heap *heap);

/* Checks that an object was not released by the verifier (debug builds). */
void quux_memory_check_object(const quux_object *object);

#endif
