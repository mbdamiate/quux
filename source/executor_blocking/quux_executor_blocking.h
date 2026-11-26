/* Reuse: replace (by the work-stealing scheduler of the Phase 3 VM). The blocking executor (D-40):
 * one operating-system thread per task, with an explicit stack size, and one condition variable
 * per task. Parking is a timed wait on the task's condition under the executor's single lock,
 * which also protects the semantic state of the concurrency module. */
#ifndef QUUX_EXECUTOR_BLOCKING_H
#define QUUX_EXECUTOR_BLOCKING_H

#include "quux_concurrency.h"

/* stack_bytes: the C stack of every task (rounded up to the page size). Returns NULL when out
 * of memory. */
quux_executor *quux_executor_blocking_create(size_t stack_bytes);

#endif
