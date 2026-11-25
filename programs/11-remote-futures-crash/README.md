# 11 — Remote futures with a node crash

**What this tests in the design:** eight partitions are computed with `spawn-on`, alternating
between nodes `:a` and `:b`.

- Node `:b` crashes after completing two of its four partitions. The futures of the other two
  fail with `:node-unreachable`; they never hang and are never silently retried.
- The program catches the failure outside a per-attempt scope and recomputes the lost
  partitions on `:a`. Pure functions make recomputation safe.
- The total equals the result of a run without failures.

The same source runs on simulated nodes (Phase 1) and on real nodes in containers (Phase 5):
the crash is described only in `conditions.qx` (D-25).

**Specification sections:** 13 serialization and identity (`spawn-on`, remote failure kinds),
12 errors, 07 scopes.

**Decisions:** D-13, D-19, D-20, D-22, D-25.

**Hypotheses informed:** H6 (cancellation of in-flight remote work), and the lineage-based
recomputation of Phase 5e.
