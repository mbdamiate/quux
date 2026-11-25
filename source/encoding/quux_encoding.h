/* Reuse: keep. The canonical binary encoding of values and analyzed code, and the content hashes
 * built on it (D-19, section 13 rules 1-5): function identity covers the expanded code with
 * locals by position and every global reference replaced by the referenced definition's hash;
 * mutually recursive definitions (a strongly connected component) are hashed as one group.
 * The simulator (D-45) sends values in this encoding. */
#ifndef QUUX_ENCODING_H
#define QUUX_ENCODING_H

#include "quux_hashing.h"
#include "quux_values.h"

/* Function values. Closure: values [lambda, environment]. Primitive: values [name]; bytes: the
 * C function pointer and the arity (minimum, maximum; maximum UINT32_MAX when variadic). */
enum { QUUX_CLOSURE_LAMBDA, QUUX_CLOSURE_ENVIRONMENT };
enum { QUUX_PRIMITIVE_NAME };
/* Environment frame: values [parent, slot 0, slot 1, ...]. */
enum { QUUX_ENVIRONMENT_PARENT, QUUX_ENVIRONMENT_FIRST_SLOT };

/* The 32-byte content hash of a definition (its identity), computed with its whole strongly
 * connected component on first request and cached. */
bool quux_definition_identity(quux_context *context, quux_value definition,
                              unsigned char digest[QUUX_CONTENT_HASH_BYTES]);

/* The content hash of a function's code, without its captured environment (section 13 rule 5). */
bool quux_function_content_hash(quux_context *context, quux_value function,
                                unsigned char digest[QUUX_CONTENT_HASH_BYTES]);

/* The value captured by a closure for its i-th free reference. */
quux_value quux_closure_captured_value(quux_value closure, uint32_t index);
uint32_t quux_closure_captured_count(quux_value closure);

/* Installs equal? and hash for function values (closures and primitives) in the collections
 * module. */
void quux_encoding_install_function_hooks(void);

#endif
