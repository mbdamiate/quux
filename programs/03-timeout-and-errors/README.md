# 03 — Timeout and error propagation

**What this tests in the design:** two scenarios in one run.

- A: a failing child cancels its sibling, and the scope raises the child's error to the parent.
- B: `with-timeout` fails with `:timeout`, a kind distinct from other errors, and cancels the
  inner computation.

Both finish in under 1 s although the injected work would take 15 s.

**Specification sections:** 07 scopes and cancellation, 08 combinators (`with-timeout`),
12 errors.

**Decisions:** D-09, D-11, D-12, D-13.

**Hypotheses informed:** none.
