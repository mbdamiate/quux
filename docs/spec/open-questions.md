# Register of open questions and hypotheses

One row per item of spec FR-026, each with a recommendation, the decision or hypothesis that
records it, and the phase that closes what is still open. Decisions are in
[docs/decisions.md](../decisions.md); hypotheses in [docs/hypotheses.md](../hypotheses.md).

| # | Question | Recommendation | Record (D-xx / H-n) | Closing phase |
|---|---|---|---|---|
| 1 | Cancellation semantics (cooperative or forced; at which points) | Cooperative only; observed at `wait-for`, agent sends, stream advance and `emit`, `sleep`, I/O, remote calls and `check-cancellation`; `finally` runs; `shield-from-cancellation` protects cleanup | D-09 (section 07) | Closed for the language; cross-node cancellation in Phase 5b, I/O cancellation in Phase 4 |
| 2 | Fate of a future whose scope has ended | The scope joins every child before it ends, so a future observed after its scope is always `:filled` or `:failed`; `wait-for` returns or raises immediately | D-10 (section 06) | Closed |
| 3 | Whether and when futures escape scopes | Futures MAY escape as values, but only settled; no future is `:empty` without a live scope. Stream producers are cancelled, not waited for, when their scope's body returns | D-10, D-17 amendment (sections 06, 07, 10) | Closed; remote references to escaped futures in Phase 5a |
| 4 | Propagation when several children fail | First non-cancellation failure (completion order) is primary; later ones are `:suppressed`; cancellations caused by the scope or by a combinator are dropped. Tests accept any racing error | D-11 and amendment (section 07) | Closed; deterministic choice for replay is H9 (Phase 1 / Phase 7) |
| 5 | Agent semantics (ordering, errors, reading current state) | One update at a time; FIFO per sender, no order between senders; a failed update keeps the state and fails only its send; `state` never waits; `transact` for test-and-act; `update-once` for idempotence | D-15, D-16 (sections 09, 13) | Closed locally; remote agents and delivery semantics in Phase 5d (H5) |
| 6 | Error model (including timeouts and unreachable nodes) | Errors are serializable values with `:kind`; standard kinds include `:timeout`, `:cancelled`, `:deadlock`, `:node-unreachable`, `:transport`, `:remote`, `:not-serializable`; no implicit retries | D-13 and amendment (section 12) | Closed; error origin on real nodes in Phase 5a |
| 7 | Scope identity and serialization | 128-bit random identifier; travels as identifier + owner node + cancellation capability, never with its children; remote work runs in a proxy scope linked to the caller | D-21 (section 13) | Leases and heartbeats in Phase 5b |
| 8 | Equality and hash of functions and futures | Functions: content hash of the expanded code plus captured values; futures, agents, scopes and nodes: identity only; `equal?` never waits | D-02, D-19 (sections 01, 13) | Closed; hash algorithms in Phase 2 (H10) |
| 9 | Modules compatible with content addressing | A namespace maps names to definition hashes; redefinition creates a new hash; renaming keeps identity | D-23 (section 14) | Code reloading policy in Phase 5c and Phase 7 |
| 10 | Macro hygiene | Non-hygienic `define-macro`; `quasi-quote` qualifies symbols; explicit `unique-symbol`; hashing after full expansion | D-06 (section 04) | Closed; full hygiene reconsidered in Phase 7 if needed |
| 11 | Numerics | 64-bit integers with `:overflow`; IEEE doubles; explicit `quotient`/`remainder`/`modulo`; `:inexact-division`, `:division-by-zero`; fixed reduction tree for parallel float sums | D-04, D-18 (sections 05, 11) | Bignums and rationals after Phase 3; native overflow checks in Phase 6 |
| 12 | Deadlock between futures | The runtime detects wait-for cycles; every future in the cycle fails with `:deadlock`; guaranteed within one process (including simulated nodes); `with-timeout` across real nodes | D-14 (section 07, program 09) | Cost in the VM: H8 (Phase 3); distributed cycles: Phase 5 |
| 13 | H3 — scope of the future returned by an agent send | Resolved: the send's future belongs to the sender's current scope; agents are outside the scope tree and usable from any scope; a queued update is withdrawn on cancellation, a started one commits | D-15, D-16; H3 resolved | Closed |
| 14 | H4 — function hash vs captured environment; mutual recursion | Partly resolved: the hash excludes the environment; mutually recursive groups are hashed together; hashing happens after macro expansion | D-06, D-19; H4 partly resolved | Fetch protocol, code cache and reloading in Phase 5c |

## Hypotheses outside this feature

| Hypothesis | Where it is examined |
|---|---|
| H1 (reuse of the prototype), H2 (GC root discipline), H7 (portability, x86_64 in CI) | Feature 002 (Phase 1) |
| H5 (lifetime of operation identifiers) | Phase 5d |
| H6 (pipelining vs. structured cancellation) | Phase 5a/5b |
| H8 (cost of deadlock detection) | Phase 3 |
| H9 (deterministic primary error) | Phase 1, Phase 7 |
| H10 (two hashes per structure) | Phase 2 (spike) |
| H11 (streams owned by the creator scope) | Phase 1 (programs 06, 07, 10) |
| H12 (fixed reduction tree vs. load balancing) | Phase 3 |

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
| W9 | When does a lost message become a `:transport` failure? | program 12 (self-review) | Resolved for Phase 0–1: in simulation, the pending future fails as soon as the message is dropped (harness contract). **Open — closed in Phase 5a** for the deadline on real networks |
| W10 | What is the current scope inside the body of a future, an agent update or a `spawn-on`? | program 10 (self-review) | Resolved: the scope that owns the future; the agent's own scope; the remote proxy scope (section 06, rule 3) |
| W8 | Can actors coexist with structured scopes? A long-lived actor created by `spawn-on` is a child of a proxy scope that must join it | actor version of program 12 | **Open — closed in Phase 4** (actor evaluation) **and Phase 5f**: actors would need to live outside the scope tree, like agents, or in a node-owned scope (see `programs/actors-comparison.md`) |
