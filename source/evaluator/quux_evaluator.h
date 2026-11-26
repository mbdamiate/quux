/* Reuse: replace (by the Phase 3 VM). A tree-walking evaluator over analyzed code with a tail-call
 * loop: every call in tail position reuses the C frame, so tail calls, including mutual
 * recursion, run in constant stack (D-38, section 03). Calls that are not in tail position
 * recurse in C, up to runtime->maximum_recursion_depth, then raise :stack-exhausted. */
#ifndef QUUX_EVALUATOR_H
#define QUUX_EVALUATOR_H

#include "quux_values.h"

/* A primitive receives its arguments in rooted memory (an ARGUMENTS object of the caller). */
typedef bool (*quux_primitive_function)(quux_context *context, quux_value *arguments,
                                        uint32_t count, quux_value *result);

#define QUUX_VARIADIC UINT32_MAX

/* Creates the namespaces `core` (primitives and prelude macros) and `user`, and makes `user` the
 * context's current namespace. */
bool quux_evaluator_initialize(quux_context *context);

bool quux_define_primitive(quux_context *context, const char *name,
                           quux_primitive_function function, uint32_t minimum, uint32_t maximum);
/* A primitive that is not defined in any namespace (the functions behind the concurrency forms,
 * and the bodies of runtime tasks). Its name appears in messages and in content hashes. */
bool quux_make_primitive(quux_context *context, const char *name, quux_primitive_function function,
                         uint32_t minimum, uint32_t maximum, quux_value *result);

/* Analyzes (expanding macros completely) and evaluates one top-level form. */
bool quux_evaluate_form(quux_context *context, quux_value form, quux_value *result);
/* Reads and evaluates every form of a text; *result is the value of the last one. */
bool quux_evaluate_text(quux_context *context, const char *text, size_t length, quux_value *result);

/* Evaluates analyzed code in an environment (nothing at the top level). */
bool quux_evaluate(quux_context *context, quux_value code, quux_value environment,
                   quux_value *result);

/* Calls a function with arguments held in an ARGUMENTS object. */
bool quux_make_arguments(quux_context *context, uint32_t count, quux_value *result);
bool quux_apply(quux_context *context, quux_value function, quux_value arguments,
                quux_value *result);
/* Shorthands for one and two arguments. */
bool quux_call1(quux_context *context, quux_value function, quux_value first, quux_value *result);
bool quux_call2(quux_context *context, quux_value function, quux_value first, quux_value second,
                quux_value *result);

/* Registers the sequential core functions (quux_core_functions.c). */
bool quux_core_functions_register(quux_context *context);

#endif
