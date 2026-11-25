/* Helpers for unit tests that evaluate quux text in a fresh session. */
#ifndef QUUX_UNIT_EVALUATION_H
#define QUUX_UNIT_EVALUATION_H

#include "quux_harness.h"
#include "quux_session.h"
#include "quux_unit_test.h"

/* Evaluates text and compares the printed (readable) result with `expected`, or, when expected
 * starts with "!! ", the error kind (for example "!! :overflow"). */
bool quux_unit_evaluates_to(quux_context *context, const char *text, const char *expected);

#endif
