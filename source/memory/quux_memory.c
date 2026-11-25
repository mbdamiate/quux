#include "quux_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    QUUX_NOTHING_WORD = 0x02u,
    QUUX_RELEASED_KIND = 0xDEADu,
    QUUX_RELEASED_BYTE = 0xDB,
};

typedef struct quux_enumerator_entry {
    quux_root_enumerator enumerator;
    void *data;
} quux_enumerator_entry;

struct quux_heap {
    quux_mutex mutex;
    quux_object *allocations;
    size_t live_count;
    quux_value **global_roots;
    size_t global_root_count;
    size_t global_root_capacity;
    quux_enumerator_entry *enumerators;
    size_t enumerator_count;
    size_t enumerator_capacity;
    quux_root_stack *root_stacks;
    size_t root_stack_count;
    quux_verify_mode verify_mode;
    uint64_t allocations_since_verify;
    uint64_t verify_interval;
    bool verifying;
    uint64_t allocation_limit; /* 0: no limit; otherwise allocations left before failing */
    quux_object **mark_stack;
    size_t mark_stack_count;
    size_t mark_stack_capacity;
};

static bool grow_array(void **array, size_t *capacity, size_t element_size, size_t needed) {
    if (needed <= *capacity) {
        return true;
    }
    size_t new_capacity = *capacity == 0 ? 16 : *capacity * 2;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    void *grown = realloc(*array, new_capacity * element_size);
    if (grown == NULL) {
        return false;
    }
    *array = grown;
    *capacity = new_capacity;
    return true;
}

quux_heap *quux_memory_create_heap(void) {
    quux_heap *heap = calloc(1, sizeof *heap);
    if (heap == NULL) {
        return NULL;
    }
    quux_mutex_initialize(&heap->mutex);
    heap->verify_mode = QUUX_VERIFY_OFF;
    heap->verify_interval = 1;
    return heap;
}

static void release_object(quux_object *object) {
    free(object->content_hash);
    size_t size =
        sizeof(quux_object) + (size_t)object->value_count * sizeof(quux_value) + object->byte_count;
    quux_unpoison_memory(object, size);
    memset(object, QUUX_RELEASED_BYTE, size);
    object->kind = QUUX_RELEASED_KIND;
    free(object);
}

void quux_memory_shutdown(quux_heap *heap) {
    if (heap == NULL) {
        return;
    }
    quux_object *object = heap->allocations;
    while (object != NULL) {
        quux_object *next = object->next_allocation;
        free(object->content_hash);
        free(object);
        object = next;
    }
    free(heap->global_roots);
    free(heap->enumerators);
    free(heap->mark_stack);
    quux_mutex_destroy(&heap->mutex);
    free(heap);
}

void quux_memory_close_frame(quux_root_stack *roots, quux_root_frame *frame) {
#ifndef NDEBUG
    if (roots->top != frame) {
        /* The open inner frame belongs to a function that already returned, so its record is
         * not read: only this function's name is reported. */
        fprintf(stderr,
                "quux: root frame of %s closed while an inner frame is still open "
                "(a callee returned without QUUX_ROOT_FRAME_END)\n",
                frame->function_name);
        abort();
    }
#endif
    roots->top = frame->previous;
}

static size_t verify_locked(quux_heap *heap);

quux_object *quux_memory_allocate(quux_heap *heap, quux_root_stack *roots, uint32_t kind,
                                  uint32_t value_count, uint32_t byte_count) {
    (void)roots;
    quux_mutex_lock(&heap->mutex);
    if (heap->allocation_limit != 0) {
        if (heap->allocation_limit == 1) {
            quux_mutex_unlock(&heap->mutex);
            return NULL;
        }
        heap->allocation_limit--;
    }
    if (heap->verify_mode != QUUX_VERIFY_OFF && !heap->verifying && heap->root_stack_count == 1) {
        heap->allocations_since_verify++;
        if (heap->allocations_since_verify >= heap->verify_interval) {
            verify_locked(heap);
        }
    }
    size_t size = sizeof(quux_object) + (size_t)value_count * sizeof(quux_value) + byte_count;
    quux_object *object = malloc(size);
    if (object == NULL) {
        quux_mutex_unlock(&heap->mutex);
        return NULL;
    }
    object->kind = kind;
    object->flags = 0;
    object->value_count = value_count;
    object->byte_count = byte_count;
    object->value_hash = 0;
    object->content_hash = NULL;
    for (uint32_t i = 0; i < value_count; i++) {
        object->values[i] = QUUX_NOTHING_WORD;
    }
    memset(quux_object_bytes(object), 0, byte_count);
    object->next_allocation = heap->allocations;
    heap->allocations = object;
    heap->live_count++;
    quux_mutex_unlock(&heap->mutex);
    return object;
}

void quux_memory_register_global_root(quux_heap *heap, quux_value *slot) {
    quux_mutex_lock(&heap->mutex);
    bool grown = grow_array((void **)&heap->global_roots, &heap->global_root_capacity,
                            sizeof(quux_value *), heap->global_root_count + 1);
    quux_assert(grown);
    (void)grown;
    heap->global_roots[heap->global_root_count++] = slot;
    quux_mutex_unlock(&heap->mutex);
}

void quux_memory_register_root_enumerator(quux_heap *heap, quux_root_enumerator enumerator,
                                          void *data) {
    quux_mutex_lock(&heap->mutex);
    bool grown = grow_array((void **)&heap->enumerators, &heap->enumerator_capacity,
                            sizeof(quux_enumerator_entry), heap->enumerator_count + 1);
    quux_assert(grown);
    (void)grown;
    heap->enumerators[heap->enumerator_count++] = (quux_enumerator_entry){enumerator, data};
    quux_mutex_unlock(&heap->mutex);
}

void quux_memory_register_root_stack(quux_heap *heap, quux_root_stack *roots) {
    quux_mutex_lock(&heap->mutex);
    roots->previous_registered = NULL;
    roots->next_registered = heap->root_stacks;
    if (heap->root_stacks != NULL) {
        heap->root_stacks->previous_registered = roots;
    }
    heap->root_stacks = roots;
    heap->root_stack_count++;
    quux_mutex_unlock(&heap->mutex);
}

void quux_memory_unregister_root_stack(quux_heap *heap, quux_root_stack *roots) {
    quux_mutex_lock(&heap->mutex);
    if (roots->previous_registered != NULL) {
        roots->previous_registered->next_registered = roots->next_registered;
    } else {
        heap->root_stacks = roots->next_registered;
    }
    if (roots->next_registered != NULL) {
        roots->next_registered->previous_registered = roots->previous_registered;
    }
    heap->root_stack_count--;
    quux_mutex_unlock(&heap->mutex);
}

void quux_memory_write_barrier(quux_heap *heap, quux_object *owner, quux_value value) {
    (void)heap;
    (void)owner;
    (void)value;
}

void quux_memory_set_verify_mode(quux_heap *heap, quux_verify_mode mode) {
    quux_mutex_lock(&heap->mutex);
    heap->verify_mode = mode;
    heap->verify_interval = 1;
    heap->allocations_since_verify = 0;
    quux_mutex_unlock(&heap->mutex);
}

void quux_memory_set_allocation_limit(quux_heap *heap, uint64_t remaining) {
    quux_mutex_lock(&heap->mutex);
    heap->allocation_limit = remaining == 0 ? 0 : remaining + 1;
    quux_mutex_unlock(&heap->mutex);
}

size_t quux_memory_live_object_count(quux_heap *heap) {
    quux_mutex_lock(&heap->mutex);
    size_t count = heap->live_count;
    quux_mutex_unlock(&heap->mutex);
    return count;
}

void quux_memory_check_object(const quux_object *object) {
#ifndef NDEBUG
    if (object->kind == QUUX_RELEASED_KIND) {
        fprintf(stderr, "quux: use of an object released by the root verifier "
                        "(an unregistered reference; see contracts/root-convention.md)\n");
        abort();
    }
#else
    (void)object;
#endif
}

/* ---- Verifier ---- */

static void push_mark(quux_heap *heap, quux_value value) {
    if (!quux_value_is_object(value)) {
        return;
    }
    quux_object *object = quux_value_object(value);
    if (object->flags & QUUX_OBJECT_FLAG_MARKED) {
        return;
    }
    quux_memory_check_object(object);
    object->flags |= QUUX_OBJECT_FLAG_MARKED;
    bool grown = grow_array((void **)&heap->mark_stack, &heap->mark_stack_capacity,
                            sizeof(quux_object *), heap->mark_stack_count + 1);
    if (!grown) {
        fprintf(stderr, "quux: out of memory in the root verifier\n");
        abort();
    }
    heap->mark_stack[heap->mark_stack_count++] = object;
}

static void mark_callback(void *mark_state, quux_value value) {
    push_mark(mark_state, value);
}

static size_t verify_locked(quux_heap *heap) {
    if (heap->root_stack_count != 1) {
        return 0;
    }
    heap->verifying = true;
    for (size_t i = 0; i < heap->global_root_count; i++) {
        push_mark(heap, *heap->global_roots[i]);
    }
    for (size_t i = 0; i < heap->enumerator_count; i++) {
        heap->enumerators[i].enumerator(heap->enumerators[i].data, mark_callback, heap);
    }
    for (quux_root_stack *roots = heap->root_stacks; roots != NULL;
         roots = roots->next_registered) {
        for (size_t i = 0; i < roots->fixed_count; i++) {
            push_mark(heap, roots->fixed_slots[i]);
        }
        for (quux_root_frame *frame = roots->top; frame != NULL; frame = frame->previous) {
            for (size_t i = 0; i < frame->count; i++) {
                push_mark(heap, *frame->slots[i]);
            }
        }
    }
    size_t marked = 0;
    while (heap->mark_stack_count > 0) {
        quux_object *object = heap->mark_stack[--heap->mark_stack_count];
        marked++;
        for (uint32_t i = 0; i < object->value_count; i++) {
            push_mark(heap, object->values[i]);
        }
    }
    size_t released = 0;
    quux_object **link = &heap->allocations;
    while (*link != NULL) {
        quux_object *object = *link;
        if (object->flags & QUUX_OBJECT_FLAG_MARKED) {
            object->flags &= ~(uint32_t)QUUX_OBJECT_FLAG_MARKED;
            link = &object->next_allocation;
        } else {
            *link = object->next_allocation;
            release_object(object);
            released++;
        }
    }
    heap->live_count -= released;
    heap->allocations_since_verify = 0;
    if (heap->verify_mode == QUUX_VERIFY_AMORTIZED) {
        heap->verify_interval = marked / 4 + 1;
    } else {
        heap->verify_interval = 1;
    }
    heap->verifying = false;
    return released;
}

size_t quux_memory_verify_roots(quux_heap *heap) {
    quux_mutex_lock(&heap->mutex);
    size_t released = verify_locked(heap);
    quux_mutex_unlock(&heap->mutex);
    return released;
}
