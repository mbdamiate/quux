# 05 — Parallel transform, deterministic

**What this tests in the design:** `parallel-transform` and `parallel-reduce` over a vector of
1,000,000 integers give the same result as the sequential versions. The floating-point sum is
reduced along the fixed tree (D-18), so it is bit-identical on every run.

The bound of 600 s is deliberately loose: the Phase 1 runtime uses one thread per future and is
not meant to be fast.

**Specification sections:** 11 data parallelism, 05 numerics, 01 equality and hashing.

**Decisions:** D-04, D-18.

**Hypotheses informed:** H12 (cost of the fixed tree).
