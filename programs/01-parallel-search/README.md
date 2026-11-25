# 01 — Parallel search with `all`

**What this tests in the design:** two independent lookups run in parallel inside one scope and
are combined. The elapsed time proves parallelism (close to the longer delay, not the sum), and
both futures end `:filled`. `let-parallel` reads like sequential code with no async marker.

**Specification sections:** 06 futures, 07 scopes, 08 combinators (`let-parallel`, `all`).

**Decisions:** D-07, D-08, D-12.

**Hypotheses informed:** none.
