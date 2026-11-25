# Contract: modules, C conventions and the executor interface

Decisions: D-32 (modules and reuse tags), D-33 (conventions), D-48 (portability).

## Module rules

1. Each directory under `source/` is one module and one static library, with exactly one public
   header `source/<module>/quux_<module>.h`. Other headers in the directory are private.
2. The first comment of the public header states the reuse tag: `Reuse: keep`, `Reuse: adapt`
   or `Reuse: replace`, with one sentence on why (D-32 table).
3. A module may include only the public headers of the modules it declares as dependencies in
   CMake. The allowed direction is:

   ```text
   portability → memory → values → hashing → collections → reader/printer → namespaces
     → expander → encoding → evaluator → concurrency (→ executor interface)
     → executor_blocking, input_output, simulation → harness → main
   ```

   `concurrency` never includes `executor_blocking`; it receives an executor through the
   interface below. `namespaces` comes before `expander` because the analyzer resolves global
   names and macros while it expands (changed during US2); the expander calls macros through a
   hook installed by the evaluator.
4. Only `portability` includes `<pthread.h>`, `<time.h>`, `<unistd.h>`, `<sys/random.h>` and
   contains `#if` on the operating system or the architecture (H7).

## C conventions

- Exported names: `quux_<module>_<words>` (functions), `quux_<noun>` (types),
  `QUUX_<WORDS>` (macros). Full words only; the abbreviations allowed are those of
  `docs/naming.md` section H.
- Fallible functions: `bool quux_<module>_<verb>(quux_context *context, …, quux_value *result)`.
  On `false`, `context->pending_error` holds the error value. No `setjmp`/`longjmp`.
- Every function that can allocate takes `quux_context *context` and follows the
  [root convention](./root-convention.md).
- Shared mutable state uses `<stdatomic.h>` through `quux_atomic_*` wrappers with an explicit
  memory order at every call; the default sequentially consistent order is not used implicitly.
- Lock order: the global wait-for lock, then scope locks (parent before child), then task locks.

## Executor interface (`quux_executor`)

The seam between the semantics (`concurrency`) and the thread model (`executor_blocking` now, the
VM scheduler in Phase 3).

| Operation | Contract |
|---|---|
| `start_task(executor, task, entry, argument)` | Runs `entry(context, argument)` concurrently. Returns `false` with `:resource-exhausted` if it cannot. |
| `park(executor, context, condition_check, deadline)` | Suspends the current task until `condition_check` is true, the deadline passes, or the task is cancelled. Returns which of the three happened. Spurious wake-ups are handled inside. |
| `wake(executor, task)` | Makes a parked task re-evaluate its condition. |
| `cancel(executor, task)` | Sets the cancellation flag and wakes the task, including from blocked input/output. |
| `current_time(executor)` | Monotonic milliseconds. |
| `join_task(executor, task)` | Releases the task's resources after it ended. |

Every cancellation point of section 07 is implemented in `concurrency` on top of `park`.
