# 08 — Recursive fork/join

**What this tests in the design:** three uses of `fork-join` in one run.

- A tree sum over 2^16 values (leaves of 256 values) equals the sequential sum.
- A parallel quicksort equals the sequential quicksort. Small vectors are sorted inside one
  leaf, to keep the number of tasks small for the Phase 1 runtime (one thread per future).
- A subtask fails: the error reaches the root, and the slow subtask still pending is cancelled
  instead of being waited for.

`sequential-main` uses `sequential-solve`, a plain recursive version of the same split and
combine functions. The sequential run of the failure case hits the faulty leaf before the slow
one, so both versions report `:user`.

**Specification sections:** 11 data parallelism, 07 scopes (failure in a child).

**Decisions:** D-08, D-11, D-18.

**Hypotheses informed:** H12 (fixed tree vs. load balancing on unbalanced splits; the quicksort
splits are unbalanced).
