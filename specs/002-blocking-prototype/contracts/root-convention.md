# Contract: allocation and root convention

Decision: D-34 (resolves H2 for Phase 1). Every runtime C function follows these rules, even
though no collector runs in Phase 1.

## Rules

1. **Allocation** goes only through `quux_memory_allocate(context, kind, size, &result)`. No
   direct `malloc` of quux values anywhere else.
2. **Local roots**: a function that holds a `quux_value` across any call that can allocate
   (directly or indirectly) registers the **address** of that variable in a root frame:

   ```c
   bool quux_list_reverse(quux_context *context, quux_value list, quux_value *result) {
       quux_value reversed = quux_nothing;
       QUUX_ROOT_FRAME(context, &list, &reversed);
       while (!quux_value_is_nothing(list)) {
           if (!quux_list_prepend(context, quux_list_first(list), reversed, &reversed)) {
               QUUX_ROOT_FRAME_END(context);
               return false;
           }
           list = quux_list_rest(list);
       }
       *result = reversed;
       QUUX_ROOT_FRAME_END(context);
       return true;
   }
   ```

   Parameters count as locals: if they are used after an allocation, they are registered.
3. Every exit path closes the frame (`QUUX_ROOT_FRAME_END`). In debug builds, a frame that is
   not closed is detected at the next frame operation and aborts with the function name.
4. **Global roots** (namespaces, interned symbols and keywords, nodes, the code store) are
   registered once with `quux_memory_register_global_root`.
5. **Write barrier**: every store of a value into a mutable cell (future result, agent state,
   stream cell, namespace binding, agent queue entry) calls
   `quux_memory_write_barrier(context, owner, value)` before the store becomes visible to other
   tasks.
6. **Values crossing tasks** are published only through mutable cells (rule 5) or through a
   task's start argument, which the executor roots for the task's lifetime.
7. **No hidden references**: quux values are not stored in C static variables or in memory not
   known to the memory module, except through rule 4.

## Shutdown and allocation failure

8. The memory module records every allocation. `quux_memory_shutdown(runtime)` frees all of them
   when the runtime ends, so LSan and valgrind report only real leaks (SC-002).
9. If an allocation fails, `quux_memory_allocate` returns `false` with `:resource-exhausted`
   (FR-017). A test-only allocation limit makes this testable.

## Verifier

With `--verify-roots` (debug builds), whenever exactly one task is running, every allocation:

1. marks everything reachable from the root frames of that task, the global roots and the
   values held by idle futures, agents and streams;
2. poisons every unmarked object (ASan manual poisoning when built with ASan, otherwise a byte
   pattern that `quux_value_*` accessors check in debug builds).

A use of an unregistered reference then fails with an ASan report or a poison-check abort that
names the object kind. The example suite and every program's sequential parts run with the
verifier in CI (`debug` preset).

## What Phase 3 inherits

The macros and the barrier keep their names and meaning; the collector replaces the verifier.
Native code (Phase 6) must produce the same frames.
