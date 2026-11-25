# 09 — Deadlock between futures

**What this tests in the design:** future A waits for B and B waits for A. The runtime detects
the wait-for cycle at the second `wait-for`, both futures fail with `:deadlock` (whose `:data`
lists the cycle), the scope propagates the error, and the program ends instead of hanging.

The injected waits replace the `sleep` of the example in section 07. They only order the
start-up; they are not part of the semantics being tested.

**Specification sections:** 07 scopes and cancellation (deadlock), 06 futures.

**Decisions:** D-14.

**Hypotheses informed:** H8 (cost of detection in the VM).
