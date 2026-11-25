/* Reuse: keep. Namespaces map names to immutable definition records (D-23, section 14). A
 * redefinition creates a new record and rebinds the name; code analyzed earlier keeps the record
 * it resolved. The namespace `core` holds the core forms and is imported into every namespace. */
#ifndef QUUX_NAMESPACES_H
#define QUUX_NAMESPACES_H

#include "quux_values.h"

/* Namespace object: values [name, bindings (map symbol -> definition), imports (list)]. */
enum { QUUX_NAMESPACE_NAME, QUUX_NAMESPACE_BINDINGS, QUUX_NAMESPACE_IMPORTS };

/* Definition record: values [name, namespace, value, code]; bytes: flags. */
enum {
    QUUX_DEFINITION_NAME,
    QUUX_DEFINITION_NAMESPACE,
    QUUX_DEFINITION_VALUE,
    QUUX_DEFINITION_CODE, /* the analyzed value expression, or nothing for primitives */
};
enum { QUUX_DEFINITION_FLAG_MACRO = 1u };

bool quux_namespaces_initialize(quux_context *context);

/* The namespace with this name, created when absent (its imports start with `core`). */
bool quux_namespace_find_or_create(quux_context *context, quux_value name, quux_value *result);
/* The namespace with this name, or nothing. */
quux_value quux_namespace_find(quux_context *context, quux_value name);
quux_value quux_namespace_core(quux_context *context);

bool quux_namespace_import(quux_context *context, quux_value namespace_value, quux_value imported);

/* Resolves a symbol as seen from a namespace: a qualified symbol in its namespace; an
 * unqualified one in the namespace itself, then its imports, then `core`. Returns the definition
 * record or nothing. */
bool quux_namespace_resolve(quux_context *context, quux_value namespace_value, quux_value symbol,
                            quux_value *definition);

bool quux_namespace_define(quux_context *context, quux_value namespace_value, quux_value name,
                           quux_value value, quux_value code, uint32_t flags,
                           quux_value *definition);

static inline quux_value quux_definition_field(quux_value definition, int field) {
    return quux_checked_object(definition)->values[field];
}
uint32_t quux_definition_flags(quux_value definition);

#endif
