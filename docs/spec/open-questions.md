# Register of open questions and hypotheses

One row per item of spec FR-026. User Story 5 (tasks T037–T038) fills the **Recommendation**
and **Record** columns. Until then, rows are placeholders.

| # | Question | Recommendation | Record (D-xx / H-n) | Closing phase |
|---|---|---|---|---|
| 1 | Cancellation semantics (cooperative or forced; at which points) | | | |
| 2 | Fate of a future whose scope has ended | | | |
| 3 | Whether and when futures escape scopes | | | |
| 4 | Propagation when several children fail | | | |
| 5 | Agent semantics (ordering, errors, reading current state) | | | |
| 6 | Error model (including timeouts and unreachable nodes) | | | |
| 7 | Scope identity and serialization | | | |
| 8 | Equality and hash of functions and futures | | | |
| 9 | Modules compatible with content addressing | | | |
| 10 | Macro hygiene | | | |
| 11 | Numerics | | | |
| 12 | Deadlock between futures | | | |
| 13 | H3 — scope of the future returned by an agent send | | | |
| 14 | H4 — function hash vs captured environment; mutual recursion | | | |

## Questions found during writing

Questions raised while writing sections or programs (including the SC-007 self-review, T044) are
added below, and then answered in the specification or marked with their closing phase.

| # | Question | Raised by | Resolution |
|---|---|---|---|
| W1 | Must a stream producer be waited for when its scope ends, even if it never finishes? | section 10, program 07 | Resolved: the scope cancels the producers it owns when its body returns (D-17 amendment; section 07 rule 4) |
| W2 | Do futures cancelled by `first-success` fail the scope that holds them? | programs 02, 10 | Resolved: no; cancellations caused by a combinator are dropped (D-11 amendment; section 07 rule 8) |
| W3 | Which error kinds cover integer division by zero, an index out of range, and a closed resource? | sections 02, 05, appendix | Resolved: `:division-by-zero`, `:index-out-of-range`, `:resource-closed` (D-13 amendment) |
| W4 | Syntax of variadic parameters (`& rest`) | section 03 (used by `update`, `transact`) | **Open — closed in Phase 1** |
| W5 | Destructuring in parameter lists and `let` | section 11 | **Open — closed in Phase 1** |
| W6 | Kind predicates (`vector?`, `map?`, …) | section 11 | **Open — closed in Phase 1** (to be proposed in `docs/naming.md`) |
| W7 | How do two futures obtain references to each other for a deadlock test? | program 09 | Resolved for Phase 0: through an agent, with injected start-up waits; a promise-like primitive is not added |
