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
| W4 | Syntax of variadic parameters | section 03 (used by `update`, `transact`) | Resolved in Phase 1: the keyword `:rest` before the last parameter (D-39; section 03 rule 5) |
| W5 | Destructuring in parameter lists and `let` | section 11 | Resolved in Phase 1: vector and map patterns, nesting allowed (D-39; section 03 rule 6) |
| W6 | Kind predicates (`vector?`, `map?`, …) | section 11 | Resolved in Phase 1: 16 predicates approved in `docs/naming.md` J3 (D-50) |
| W7 | How do two futures obtain references to each other for a deadlock test? | program 09 | Resolved for Phase 0: through an agent, with injected start-up waits; a promise-like primitive is not added |
| W9 | When does a lost message become a `:transport` failure? | program 12 (self-review) | Resolved for Phase 0–1: in simulation, the pending future fails as soon as the message is dropped (harness contract). **Open — closed in Phase 5a** for the deadline on real networks |
| W10 | What is the current scope inside the body of a future, an agent update or a `spawn-on`? | program 10 (self-review) | Resolved: the scope that owns the future; the agent's own scope; the remote proxy scope (section 06, rule 3) |
| W11 | Which error kind does the reader raise for a syntax error (unbalanced brackets, a reader shorthand, a malformed number)? | feature 002, reader | Resolved in Phase 1: `:syntax` (D-54, naming J8; section 12); an integer literal outside 64 bits stays `:overflow` |
| W8 | Can actors coexist with structured scopes? A long-lived actor created by `spawn-on` is a child of a proxy scope that must join it | actor version of program 12 | **Open — closed in Phase 4** (actor evaluation) **and Phase 5f**: actors would need to live outside the scope tree, like agents, or in a node-owned scope (see `programs/actors-comparison.md`) |
| W12 | Should the core have string functions to split, trim and search text? | feature 003, ergonomics report K-01 | Resolved: approved by the owner (D-65, naming K1, report K-01); to be implemented by a later feature |
| W13 | Should the core expose the reader as a function that reads data from a string? | feature 003, ergonomics report K-02 | Resolved: approved by the owner (D-66, naming K2, report K-02); to be implemented by a later feature |
| W14 | Should the core read and change values through a path of keys? | feature 003, ergonomics report K-03 | Resolved: approved by the owner (D-67, naming K3, report K-03); to be implemented by a later feature |
| W15 | Should the core find the first element (or its position) that satisfies a predicate? | feature 003, ergonomics report K-04 | Resolved: approved by the owner (D-68, naming K4, report K-04); to be implemented by a later feature |
| W16 | Should the core have a function that runs an effect for each element? | feature 003, ergonomics report K-05 | Resolved: approved by the owner (D-69, naming K5, report K-05); to be implemented by a later feature |
| W17 | Should the core concatenate collections? | feature 003, ergonomics report K-06 | Resolved: approved by the owner (D-70, naming K6, report K-06); to be implemented by a later feature |
| W18 | Should the core join text and format numbers and padding? | feature 003, ergonomics report K-07 | Resolved: approved by the owner (D-71, naming K7, report K-07); to be implemented by a later feature |
| W19 | Should the core sort collections and order strings? | feature 003, ergonomics report K-08 | Resolved: approved by the owner (D-72, naming K8, report K-08); to be implemented by a later feature |
| W20 | Should the core convert and round numbers, have a square root and a power, and a division that always returns a float? | feature 003, ergonomics report K-09 | Resolved: approved by the owner (D-73, naming K9, report K-09); to be implemented by a later feature |
| W21 | Should the core have a transform that passes each element's index? | feature 003, ergonomics report K-10 | Resolved: approved by the owner (D-74, naming K10, report K-10); to be implemented by a later feature |
| W22 | Should errors have a message accessor and a cause, and core errors carry the wrong value in `:data`? | feature 003, ergonomics report K-11 | Resolved: approved by the owner (D-75, naming K11, report K-11); to be implemented by a later feature |
| W23 | Should the core expand a macro call without evaluating it? | feature 003, ergonomics report K-12 | Resolved: approved by the owner (D-76, naming K12, report K-12); to be implemented by a later feature |
| W24 | Should a `case` with no default and no matching clause raise an error instead of returning `nothing`? | feature 003, ergonomics report K-13 | Resolved: approved by the owner (D-77, naming K13, report K-13); to be implemented by a later feature |
| W25 | Should map patterns have a short form that binds names to the keywords of the same name? | feature 003, ergonomics report K-14 | Resolved: approved by the owner (D-78, naming K14, report K-14); to be implemented by a later feature |
| W26 | Should malformed special forms raise `:syntax` with a line, and should a name defined in two imports raise an error? (Section 14 rule 1 says a clashing name is not available unqualified; its closed open point says imports are searched in order.) | feature 003, ergonomics report K-15 | Resolved: approved by the owner (D-79, naming K15, report K-15); to be implemented by a later feature |
| W27 | Does `count` on a string count bytes or characters? Phase 1 counts bytes (documented in section 02) | feature 003, ergonomics report E-28 | **Open — owner decision, feature 003**; Phase 1 behavior documented |
| W28 | Can a namespace keep a definition private? | feature 003, ergonomics report E-30 | **Open — deferred with the module decisions of section 14** (Phase 7) |
| W29 | Does a failure of an alternative of `first-success` fail the scope that owns it? (Section 07 rule 5 says yes; the example of section 08 expected `:all-failed`) | feature 002, US3 example suite | **Open — owner confirmation**: Phase 1 implements D-80 (the alternatives' failures do not fail the scope; only `:all-failed` does), stated in section 07 rule 8 and section 08 rule 5 |
| W30 | What happens to a future or scope created in a scope that is already cancelling, and to an agent update sent from `shield-from-cancellation`? | feature 002, US3 unit tests | **Open — owner confirmation**: Phase 1 implements D-81 (they start cancelled; a shielded update is never withdrawn), stated in section 07 rules 10 and 14 and section 09 rule 11 |

### Example corrections in feature 002, User Story 3

The example suite (T046) ran sections 06–09 and the concurrent example of section 12 for the
first time. These examples were corrected; none changes a rule except where D-80 is named:

- Sections 07 and 08: three `;=>` lines mixed the value with prose (`;=> [:user :orders], after
  about 150 ms`). The prose now follows a second `;`, as everywhere else.
- Section 08, `first-success` when everything fails: the alternatives now sleep 10 ms before they
  fail, so that they fail after `first-success` has received them (D-80, W29).
- Section 12, several failing children: the children no longer sleep. With the sleeps, the first
  failure usually cancelled the second child while it slept, so `[:user 0]` was also correct
  under section 07 rule 7 and the example's `[:user 1] or [:timeout 1]` failed. Without a
  cancellation point, both children always fail.
