#include "quux_namespaces.h"

#include "quux_collections.h"

#include <stdlib.h>
#include <string.h>

/* runtime->namespaces: the registry, a map from namespace name to namespace (a global root). */
struct quux_namespace_registry {
    quux_value namespaces;
    quux_value core;
};

bool quux_namespaces_initialize(quux_context *context) {
    quux_runtime *runtime = context->runtime;
    struct quux_namespace_registry *registry = calloc(1, sizeof *registry);
    if (registry == NULL) {
        return quux_raise(context, "resource-exhausted", "cannot create the namespace registry");
    }
    registry->namespaces = quux_nothing;
    registry->core = quux_nothing;
    runtime->namespaces = registry;
    quux_memory_register_global_root(runtime->heap, &registry->namespaces);
    quux_memory_register_global_root(runtime->heap, &registry->core);
    if (!quux_map_empty(context, &registry->namespaces)) {
        return false;
    }
    return quux_namespace_find_or_create(context, quux_symbol(context, "core"), &registry->core);
}

quux_value quux_namespace_core(quux_context *context) {
    return context->runtime->namespaces->core;
}

quux_value quux_namespace_find(quux_context *context, quux_value name) {
    quux_value found = quux_nothing;
    if (quux_map_lookup(context, context->runtime->namespaces->namespaces, name, &found)) {
        return found;
    }
    return quux_nothing;
}

bool quux_namespace_find_or_create(quux_context *context, quux_value name, quux_value *result) {
    quux_value existing = quux_namespace_find(context, name);
    if (!quux_is_nothing(existing)) {
        *result = existing;
        return true;
    }
    struct quux_namespace_registry *registry = context->runtime->namespaces;
    quux_value bindings = quux_nothing;
    quux_value imports = quux_nothing;
    quux_value created = quux_nothing;
    QUUX_ROOT_FRAME(context, &name, &bindings, &imports, &created);
    bool done = quux_map_empty(context, &bindings);
    if (done && !quux_is_nothing(registry->core)) {
        done = quux_list_prepend(context, registry->core, quux_nothing, &imports);
    }
    quux_object *namespace_object = NULL;
    if (done) {
        namespace_object = quux_allocate(context, QUUX_KIND_NAMESPACE, 3, 0);
        done = namespace_object != NULL;
    }
    if (done) {
        namespace_object->values[QUUX_NAMESPACE_NAME] = name;
        namespace_object->values[QUUX_NAMESPACE_BINDINGS] = bindings;
        namespace_object->values[QUUX_NAMESPACE_IMPORTS] = imports;
        created = quux_object_value(namespace_object);
        quux_value updated = quux_nothing;
        done = quux_map_put(context, registry->namespaces, name, created, &updated);
        if (done) {
            registry->namespaces = updated;
            *result = created;
        }
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

bool quux_namespace_import(quux_context *context, quux_value namespace_value, quux_value imported) {
    quux_object *object = quux_checked_object(namespace_value);
    quux_value imports = object->values[QUUX_NAMESPACE_IMPORTS];
    for (quux_value cell = imports; !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
        if (quux_pair_first(cell) == imported) {
            return true;
        }
    }
    /* Imports are searched in the order they are written, and `core` stays last (section 14):
     * the new import goes after the existing ones and before `core`. */
    quux_value core = context->runtime->namespaces->core;
    quux_value remaining = imports;
    quux_value earlier = quux_nothing;
    quux_value updated = quux_nothing;
    QUUX_ROOT_FRAME(context, &namespace_value, &imported, &core, &remaining, &earlier, &updated);
    bool done = true;
    for (; done && !quux_is_nothing(remaining); remaining = quux_pair_rest(remaining)) {
        if (quux_pair_first(remaining) != core) {
            done = quux_list_prepend(context, quux_pair_first(remaining), earlier, &earlier);
        }
    }
    if (done && !quux_is_nothing(core)) {
        done = quux_list_prepend(context, core, quux_nothing, &updated);
    }
    done = done && quux_list_prepend(context, imported, updated, &updated);
    for (quux_value cell = earlier; done && !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
        done = quux_list_prepend(context, quux_pair_first(cell), updated, &updated);
    }
    if (done) {
        quux_store(context, quux_checked_object(namespace_value), QUUX_NAMESPACE_IMPORTS, updated);
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

static bool lookup_binding(quux_context *context, quux_value namespace_value, quux_value name,
                           quux_value *definition) {
    quux_value bindings = quux_checked_object(namespace_value)->values[QUUX_NAMESPACE_BINDINGS];
    return quux_map_lookup(context, bindings, name, definition);
}

bool quux_namespace_resolve(quux_context *context, quux_value namespace_value, quux_value symbol,
                            quux_value *definition) {
    *definition = quux_nothing;
    size_t namespace_length = quux_symbol_namespace_length(symbol);
    if (namespace_length > 0) {
        quux_value namespace_name = quux_nothing;
        quux_value local_name = quux_nothing;
        QUUX_ROOT_FRAME(context, &symbol, &namespace_name, &local_name);
        const char *text = quux_name_bytes(symbol);
        size_t length = quux_name_length(symbol);
        bool done = quux_intern_symbol(context, text, namespace_length, &namespace_name) &&
                    quux_intern_symbol(context, text + namespace_length + 1,
                                       length - namespace_length - 1, &local_name);
        if (done) {
            quux_value target = quux_namespace_find(context, namespace_name);
            if (!quux_is_nothing(target)) {
                (void)lookup_binding(context, target, local_name, definition);
            }
        }
        QUUX_ROOT_FRAME_END(context);
        return done;
    }
    if (!quux_is_nothing(namespace_value) &&
        lookup_binding(context, namespace_value, symbol, definition)) {
        return true;
    }
    if (!quux_is_nothing(namespace_value)) {
        quux_value imports = quux_checked_object(namespace_value)->values[QUUX_NAMESPACE_IMPORTS];
        for (quux_value cell = imports; !quux_is_nothing(cell); cell = quux_pair_rest(cell)) {
            if (lookup_binding(context, quux_pair_first(cell), symbol, definition)) {
                return true;
            }
        }
    }
    quux_value core = quux_namespace_core(context);
    if (!quux_is_nothing(core)) {
        (void)lookup_binding(context, core, symbol, definition);
    }
    return true;
}

bool quux_namespace_define(quux_context *context, quux_value namespace_value, quux_value name,
                           quux_value value, quux_value code, uint32_t flags,
                           quux_value *definition) {
    quux_value record = quux_nothing;
    quux_value bindings = quux_nothing;
    QUUX_ROOT_FRAME(context, &namespace_value, &name, &value, &code, &record, &bindings);
    quux_object *object = quux_allocate(context, QUUX_KIND_DEFINITION, 4, sizeof(uint32_t));
    bool done = object != NULL;
    if (done) {
        object->values[QUUX_DEFINITION_NAME] = name;
        object->values[QUUX_DEFINITION_NAMESPACE] = namespace_value;
        object->values[QUUX_DEFINITION_VALUE] = value;
        object->values[QUUX_DEFINITION_CODE] = code;
        memcpy(quux_object_bytes(object), &flags, sizeof flags);
        record = quux_object_value(object);
        done = quux_map_put(context,
                            quux_checked_object(namespace_value)->values[QUUX_NAMESPACE_BINDINGS],
                            name, record, &bindings);
    }
    if (done) {
        quux_store(context, quux_checked_object(namespace_value), QUUX_NAMESPACE_BINDINGS,
                   bindings);
        *definition = record;
    }
    QUUX_ROOT_FRAME_END(context);
    return done;
}

uint32_t quux_definition_flags(quux_value definition) {
    uint32_t flags;
    memcpy(&flags, quux_object_bytes(quux_checked_object(definition)), sizeof flags);
    return flags;
}
