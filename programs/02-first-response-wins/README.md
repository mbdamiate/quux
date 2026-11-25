# 02 — First response wins

**What this tests in the design:** `first-success` returns the fastest answer and cancels the
other futures before the scope ends. The scope still joins them, but cancellation is observed at
the injected waits (cancellation points), so the run ends well before the 200 ms replica.

**Specification sections:** 07 scopes and cancellation, 08 combinators (`first-success`).

**Decisions:** D-09, D-12.

**Hypotheses informed:** none.
