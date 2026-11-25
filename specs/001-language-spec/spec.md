# Feature Specification: quux Language Specification by Example (Phase 0)

**Feature Branch**: `001-language-spec`

**Created**: 2026-11-24

**Status**: Draft

**Input**: User description: "Language specification by example for quux (Phase 0): specification
skeleton, the 12 reference programs with expected observable behavior, 2–3 of them also written
with actors for comparison, and the decisions that affect distribution (function identity,
serializability, network errors in the error model). Resolve or transfer H3 and H4 and decide or
flag the open semantic questions. Success = every program's expected behavior is precise enough
to become an automated test in Phase 1; every open question has a recommendation or names the
phase that closes it."

## Context

This feature produces documents and example programs only, no runtime. Its "users" are:

- **The implementer** (the project's single developer), who will turn each program's expected
  behavior into an automated test in Phase 1 and implement against the specification.
- **The future quux programmer**, who reads the specification to learn what the language
  guarantees: values, concurrency, errors, and eventually distribution.

Governing principles: constitution v1.2.2, in particular II (immutability and canonical
collections), III (structured concurrency and three-state futures), IV (distribution decisions
taken early) and VII (decisions D-xx, hypotheses H-n, ambiguity flagged).

## Clarifications

### Session 2026-11-24

- Q: Deadlock policy for program 9 (A waits for B, B waits for A)? → A: The runtime detects
  wait-for cycles, and every future in the cycle fails with a `deadlock` error.
- Q: Cancellation of a pending agent send when the sender's scope is cancelled? → A: A queued
  update is withdrawn. An update that has started completes and commits. The sender's future
  ends cancelled.
- Q: Numerics? → A: 64-bit integers with an error on overflow, plus IEEE doubles. Parallel
  reductions use a fixed reduction tree, for deterministic float results. Bignums are
  postponed.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Specification skeleton with the core semantics answered (Priority: P1)

The implementer opens the specification and finds one section per topic. Each section opens
with the question it answers and gives either a normative answer with examples or an explicit
"open — closed in Phase N" marker. The topics are: values and equality, collections, evaluation
and TCO, macros and hygiene, numerics, futures and their three states, scopes and cancellation,
combinators (`all`, `first-success`, `then`, `with-timeout`, and the sequential-looking `let-parallel`), agents,
streams, data parallelism, the error model, serialization and identity, and modules.

**Why this priority**: every other deliverable (programs, tests, Phase 1 code) is written
against these sections. Without them the programs have no reference semantics.

**Independent Test**: a reviewer lists the 14 topics and confirms each one has a section that
states its question and either a normative answer with at least one example, or an open marker
naming the phase that closes it.

**Acceptance Scenarios**:

1. **Given** the specification, **When** a reader looks up "what does `wait-for` on a failed future
   do?", **Then** a single section answers it with an example and the exact error that is
   observed.
2. **Given** a topic that this phase does not settle (for example, the distributed garbage
   collection protocol), **When** the reader opens its section, **Then** they find the question,
   the current recommendation if there is one, and the phase that closes it.
3. **Given** two collections built by different insertion orders with the same contents,
   **When** the reader checks the equality and hash section, **Then** the section states that
   both are equal and have the same hash, because the canonical form makes this hold.

---

### User Story 2 - Local reference programs 1–10 with testable expected behavior (Priority: P1)

For each of programs 1 to 10, the implementer finds four things:

- the program in quux syntax;
- its expected observable behavior: return value, printed output, final states of futures and
  scopes, and errors;
- the injected conditions: delays, failures, inputs;
- a short "what this tests in the design" note.

The expected behavior is written so that it can be turned directly into an assertion.

**Why this priority**: these programs are the acceptance suite of every later phase (see the
constitution's Development Workflow). Phase 1 cannot start without them.

**Independent Test**: for each program, a second person (or the implementer later) could write
the assertions from the document alone without asking any question; ambiguities found that way
are defects of this feature.

**Acceptance Scenarios**:

1. **Program 1 (parallel search with `all`)**: **Given** a user lookup that takes 100 ms and
   an orders lookup that takes 150 ms (simulated), **When** the page is assembled with `all`
   inside a scope, **Then** the result contains both the user and the orders, the total elapsed
   time is at least 150 ms and under 250 ms (the longer delay plus a margin, well below the
   250 ms sum), and both futures end in the filled state.
2. **Program 2 (`first-success`)**: **Given** three replicas that respond after 300 ms, 50 ms and 200 ms,
   **When** `first-success` is used, **Then** the 50 ms answer is returned and the other two futures end in
   the cancelled state before the scope exits.
3. **Program 3 (timeout and error propagation)**: **Given** a scope with a child that fails
   after 50 ms and a sibling that would run for 10 s, **When** the scope runs, **Then** it exits
   in under 1 s, the parent observes the child's error, and the sibling ends in the cancelled
   state. **Given** a `with-timeout` of 100 ms around a computation of 5 s, **Then** the result is a
   failed future whose error is a timeout error that can be distinguished from other errors.
4. **Program 4 (bank account agent)**: **Given** an account with balance 100 and 10 concurrent
   withdrawals of 20, **When** each withdrawal checks the balance and debits in the same update
   function, **Then** exactly 5 succeed, 5 are rejected with an "insufficient funds" outcome, and
   the final balance is 0. The balance is never negative in any observed state.
5. **Program 5 (`parallel-transform`)**: **Given** a persistent vector of 1,000,000 integers and a pure
   function, **When** `parallel-transform` and the sequential `transform` are applied, **Then** the results are
   equal (same value, same hash) on every run and with any degree of parallelism.
6. **Program 6 (file stream)**: **Given** a file of 10,000 lines, **When** a stream of lines
   goes through an asynchronous `stream-transform`, a `stream-filter` and a `stream-take` of 5, **Then** exactly the first 5
   matching transformed lines are produced in file order, and the stream reads at most
   `:maximum-ahead` + 1 lines past the line of the 5th match (2 lines with the default bound),
   never the whole file.
7. **Program 7 (backpressure)**: **Given** a producer that can produce one item per millisecond
   and a consumer that takes 50 ms per item, **When** they are connected by a stream, **Then**
   the number of items produced but not yet consumed never exceeds the declared buffer bound
   (default: 1 ahead of demand).
8. **Program 8 (recursive fork/join)**: **Given** a tree of 2^16 leaves, or a vector to sort,
   **When** it is summed or quicksorted with recursive fork/join, **Then** the result equals the
   sequential result. If one subtask fails, the error reaches the root and all pending subtasks
   are cancelled.
9. **Program 9 (deadlock)**: **Given** futures A and B in the same scope where A waits for B and
   B waits for A, **When** the scope runs, **Then** the runtime detects the wait-for cycle, both A and B fail with a
   distinguishable `deadlock` error that names the futures in the cycle, the scope propagates
   that error to the parent, and the program never hangs.
10. **Program 10 (server)**: **Given** a chat server with one scope per connection and the room
    state in an agent, **When** three clients connect, exchange messages and one disconnects
    abruptly, **Then** the remaining clients each receive every message, and all of them see
    the same total order, which is the order in which the room agent committed the messages. The
    disconnected client's scope ends with its tasks cancelled, and the other connections are not
    affected.

---

### User Story 3 - Distribution-affecting decisions and programs 11–12 (Priority: P2)

The implementer finds the decisions that cannot wait for Phase 5, written as normative text,
and programs 11 and 12 written so that they can run against simulated nodes in a single process.
The decisions are: function identity, which values are serializable and how non-serializable
values are marked, the place of network errors and timeouts in the error model, and scope
identity.

**Why this priority**: the constitution (Principle IV) requires these decisions early. The
programs that exercise them are validated only on simulated nodes in Phase 1, so they come after
the local suite.

**Independent Test**: for every value kind in the specification, a reader can say whether it
is serializable, how it travels (copied, sent by hash, or sent as a remote reference), and which
error a program observes when the node holding it becomes unreachable.

**Acceptance Scenarios**:

1. **Program 11 (remote futures and node crash)**: **Given** two nodes, with work partitioned
   through `spawn-on` and node B crashing after completing half of its partitions, **When** the
   scope observes B's futures, **Then** they fail with a "node unreachable" error. The program
   recomputes the lost partitions on node A and returns the same result it would have returned
   with no failure.
2. **Program 12 (remote agent and stream)**: **Given** a remote agent receiving operations
   that each carry an operation id, and a network that drops the acknowledgement of operation 7,
   **When** the client resends operation 7, **Then** it is applied exactly once and the agent's
   final state equals the fault-free run. **Given** a stream pipeline from machine A to
   machine B with a slow consumer on B, **Then** A's production stays within the declared bound,
   across the network as well.
3. **Given** a closure that captures a file handle, **When** a program tries to send it to
   another node, **Then** the specification says the send fails at the sender with a
   "not serializable" error that names the offending value kind. It never fails silently on the
   receiver.

---

### User Story 4 - Actor versions side by side (Priority: P3)

For two or three of the reference programs (recommended: 4 bank account, 10 server, 12 remote
agent), the reader finds an actor-based version next to the future/agent version. A short
comparison covers lines of code, where failure handling lives, what supervision with restart
would add, and what the design gives up without actors.

**Why this priority**: it informs the optional actor layer (Phase 4 evaluation, Phase 5f), but
nothing in Phase 1 depends on it.

**Independent Test**: each pair has both versions and a comparison table with the same rows
for each pair.

**Acceptance Scenarios**:

1. **Given** program 4 in both styles, **When** the reader compares them, **Then** the table
   states where the atomic check-and-debit lives in each version and how a crash in the update
   is observed by the caller.

---

### User Story 5 - Register of open questions and hypotheses (Priority: P2)

Every semantic question listed in the input has an entry with a recommendation, or a marker
saying which phase closes it, plus a link to the decision ID or hypothesis that records it.
H3 and H4 are confirmed, refuted or transferred.

**Why this priority**: this is how Phase 1 and later modules inherit decisions without
re-deriving them (constitution Principle VII).

**Independent Test**: a check of the list in FR-020 finds no item without a recommendation or a
closing phase.

**Acceptance Scenarios**:

1. **Given** H3, **When** the register is read, **Then** it states the scope of the future
   returned by an agent send and whether an agent may be used from several scopes, and it
   records the outcome in the hypotheses log.

---

### Edge Cases

- A future is returned out of its scope as a value. What is observed when it is waited for
  later? (See FR-011.)
- Several children fail at almost the same time. Which error does the parent observe, and are
  the others lost? (See FR-012.)
- A cancelled task is inside a pure computation that never reaches a suspension point.
  (See FR-010.)
- An agent's update function throws. What happens to the agent's state, the caller's future,
  and later sends? (See FR-014.)
- An agent's update function itself waits for a future or sends to the same agent (re-entrancy).
- `first-success` where every alternative fails: which error is reported?
- A `with-timeout` fires at the same moment the computation succeeds.
- A stream consumer abandons a stream midway. The producer and its scope must stop.
- The hash of a function that is part of a mutually recursive group, and of a closure that
  captures a future. (See FR-017, FR-018.)
- Equality of two structurally identical closures, and of two distinct futures that hold the
  same value. (See FR-019.)
- Integer overflow and float results inside `parallel-transform` reductions, where reassociation can change
  the result. (See FR-005.)

## Requirements *(mandatory)*

### Functional Requirements

**Specification structure**

- **FR-001**: The specification MUST have one section per topic in User Story 1. Each section
  MUST open with the question it answers and MUST contain either a normative answer with at
  least one quux example, or an "open" marker naming the closing phase.
- **FR-002**: Every normative statement that settles a design choice MUST reference a decision
  ID (D-01 onward, continuous with `docs/decisions.md`) whose record contains alternatives and
  trade-offs. Decisions that later phases depend on MUST be marked "inherited by: Phase 3" or
  "inherited by: Phase 5". This applies at least to the error model, the cancellation semantics,
  scope identity and serialization.

**Values, evaluation, numerics**

- **FR-003**: Values MUST be immutable. Lists, vectors, maps and sets MUST be persistent. Maps
  and sets MUST be canonical: equal contents imply the same structure and the same hash,
  independent of construction history.
- **FR-004**: Tail calls in tail position MUST NOT grow the stack. The specification MUST
  define tail position for every special form, and MUST include a test example that does
  1,000,000 mutually recursive tail calls.
- **FR-005**: The numeric tower MUST be defined, including the behavior on integer overflow
  and the determinism guarantee for parallel reductions over floating-point values. The
  integers are 64-bit, and overflow raises an `overflow` error; it never wraps silently. Floats
  are IEEE double. Parallel reductions (`parallel-transform` followed by reduce, reducers, fork/join) combine
  results in a fixed reduction tree that depends only on the input size, never on scheduling,
  so float results are identical on every run. Bignums and rationals are postponed (decision
  recorded with the phase that revisits them).
- **FR-006**: Macro hygiene MUST be specified. Recommended default: non-hygienic `define-macro`
  in which `quasi-quote` qualifies symbols with their namespace. There is no automatic gensym:
  fresh symbols come from an explicit `unique-symbol` (D-06). Full hygiene is listed as a
  rejected alternative, with its cost.

**Futures, scopes, cancellation, combinators**

- **FR-007**: A future MUST be in exactly one of three states: empty, filled (with a value), or
  failed (with an error). A transition out of empty happens once and is final. "Cancelled" is a
  failed state with a distinguished cancellation error, not a fourth state.
- **FR-008**: Every future MUST be created inside a scope. When a scope exits, all of its
  children are settled (filled or failed). If any child fails, its siblings are cancelled and
  the error propagates to the parent.
- **FR-009**: Scopes MUST be first-class values with an identity that is unique across nodes.
  A scope can be passed as a value to request cancellation. The specification MUST state what
  of a scope is serializable: its identity and cancellation capability, never its children.
- **FR-010**: Cancellation MUST be cooperative. It is observed at defined cancellation points:
  `wait-for`, agent send, stream advance, blocking calls, remote calls, and an explicit
  `check-cancellation`. Pure code between points runs to the next point. Forced termination is
  rejected, and the decision record states why (shared heap and resource safety).
- **FR-011**: A future MAY be returned out of its scope only as a settled value. Because the
  scope joins all of its children on exit, any future observed outside its scope is filled or
  failed, never empty. Waiting for it later returns its value or raises its error. No future
  can be pending without an enclosing live scope.
- **FR-012**: When several children fail, the parent MUST observe the first failure in
  completion order as the primary error, with the other non-cancellation failures attached as
  suppressed errors. Cancellation errors caused by the first failure are not attached.
- **FR-013**: The combinators MUST be specified with their failure and cancellation behavior:
  - `all` fails fast on the first failure and cancels the rest.
  - `first-success` returns the first success and cancels the rest. If all alternatives fail, it fails
    with an aggregate error that contains each alternative's error.
  - `then` chains a function onto a future's result.
  - `with-timeout` fails with a timeout error and cancels the inner computation. When completion
    and timeout race, the first transition wins.
  - `let-parallel` is the sequential-looking form and needs no annotation on callers. There is
    no separate async macro, because `future` already covers that case.

**Agents, streams, parallelism**

- **FR-014**: Agents MUST apply update functions one at a time in the order sends are accepted
  from each sender. `update` returns a future of the new state. `transact` returns a future of
  the reply that its function returns together with the new state (D-15). Reading an agent's current state never blocks and
  returns the last committed value. An update that throws leaves the state unchanged, fails only
  that send's future, and does not stop later sends.
- **FR-015**: An agent MUST NOT be tied to the lifetime of a single scope. The future returned
  by a send belongs to the sender's current scope (this resolves H3). The behavior when the
  sender's scope is cancelled while the update is queued or running depends on whether the
  update has started:
  - If the update is still queued, it is withdrawn and never applied.
  - If the update has already started, it runs to completion and its result is committed, so
    there is never a partial effect. The sender's future still ends cancelled, so the sender
    cannot tell from it whether the update was applied; it must read the agent's state.
    Phase 5d revisits this with operation ids.
- **FR-016**: A stream MUST be a sequence whose rest is a future. Its `stream-transform`, `stream-filter` and `stream-take`
  operations are asynchronous and pull-driven. A producer runs ahead of demand by at most a
  declared buffer bound. Abandoning a stream (scope exit or explicit close) cancels its
  producer.
- **FR-017**: `parallel-transform`, fork/join and reducers MUST produce results equal to their sequential
  counterparts for pure functions, regardless of scheduling.

**Identity, hashing, serialization, modules (distribution-affecting)**

- **FR-018**: Function identity MUST be by content. A function's hash covers its normalized
  code and the hashes of the definitions it references, not the names. Mutually recursive
  groups are hashed as one unit, with members identified by their position in the group, as in
  Unison. A closure is identified by its function hash plus the values of its captured
  environment. The function hash itself never includes the environment (this resolves the
  first part of H4; the full protocol is transferred to Phase 5c).
- **FR-019**: Equality and hashing MUST be defined for every value kind:
  - Data values are compared structurally.
  - Functions compare equal when their content hash and captured environment are equal.
  - Futures, agents and scopes are compared by identity only, never by their contents.
- **FR-020**: Serializability MUST be defined for every value kind in three classes:
  - **Copied by value**: data and closures whose environments are serializable.
  - **Sent as a remote reference**: futures, agents and scopes.
  - **Not serializable**: files, sockets and other local resources. These are marked as local
    resources, and sending one fails at the sender with a distinguishable error.
- **FR-021**: The error model MUST have a single error value shape with a kind. It MUST include
  from the start: timeout, cancellation, node unreachable, network or transport failure,
  remote error (an error raised on another node, carrying its origin), not serializable, and
  user errors. Network errors are ordinary failed-future states. They are never hidden behind
  retries the program cannot see.
- **FR-022**: Modules MUST be compatible with content addressing. A name binds to a hash, and
  renaming does not change identity. Redefinition creates a new hash; existing references keep
  the old one until they are re-resolved. Code reloading details are transferred to Phase 5c
  and Phase 7.

**Programs and comparisons**

- **FR-023**: Each of the 12 reference programs MUST contain:
  - its source in quux syntax;
  - its injected conditions (delays, failures, inputs, node layout);
  - its expected observable behavior as assertable statements;
  - its "what this tests" note.
- **FR-024**: Programs 11 and 12 MUST be specified so that they run unchanged against simulated
  nodes in one process, with injected crashes, delays and message loss.
- **FR-025**: Two or three programs MUST have an actor version and a comparison per User
  Story 4.
- **FR-026**: The register of open questions MUST cover:
  - cancellation semantics;
  - the fate of a future whose scope ended;
  - whether futures escape scopes;
  - multiple failing children;
  - agent semantics;
  - the error model;
  - scope identity and serialization;
  - equality and hash of functions and futures;
  - modules and content addressing;
  - macro hygiene;
  - numerics;
  - deadlock;
  - H3 and H4.

  Each entry MUST have a recommendation or a closing phase. H1, H2 and H7 are recorded as
  belonging to Phase 1 (feature 002). H5 and H6 are recorded as open for Phase 5.

**Naming**

- **FR-027**: Every public name used in the specification and the programs MUST be approved in
  `docs/naming.md`, following constitution Principle VIII. The name must be in full words,
  with no abbreviations or Lisp historical names. The names in this spec follow the approved glossary (owner review, 2026-11-24).

### Key Entities

- **Value**: any immutable datum (scalar, list, vector, map, set, function/closure). Has
  structural equality and a stable hash.
- **Future**: an identity-bearing cell in the state empty, filled or failed. It belongs to
  exactly one scope.
- **Scope**: an identity-bearing node in a tree of scopes. It owns its child futures, carries a
  cancellation capability, and is unique across nodes.
- **Agent**: an identity-bearing reference to an immutable value, updated by serialized
  functions. It is not owned by a scope.
- **Stream**: a pull-driven sequence whose rest is a future, with a buffer bound.
- **Error**: a value with a kind (timeout, cancelled, node-unreachable, transport, remote,
  not-serializable, user), a message, optional origin information and suppressed errors.
- **Local resource**: a non-serializable value tied to one node (a file or socket handle).
- **Node**: a named execution location. In Phase 0–1 it may be simulated.
- **Reference program**: source, injected conditions, expected behavior, design note.
- **Decision (D-xx) / Hypothesis (H-n)**: traceability records per constitution Principle VII.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 12 of 12 reference programs have source, injected conditions, expected behavior
  and a design note. Each expected behavior is written as assertable statements, with zero
  statements that require interpretation ("works correctly", "fast").
- **SC-002**: The 14 specification topics each have a section with an answer or an open marker
  naming its closing phase. No topic is missing.
- **SC-003**: 100% of the items listed in FR-026 have a recommendation or a closing phase, and
  each points to a D-xx or H-n record.
- **SC-004**: Every decision that Phase 3 or Phase 5 inherits is marked as such in the
  decision log. At minimum this covers the error model, cancellation, scope identity and
  serialization.
- **SC-005**: For every value kind, the reader can determine its equality rule, hash rule and
  serialization class from the specification alone. The check is a table with no empty cells.
- **SC-006**: 2 or 3 programs have actor counterparts, with a comparison table whose rows are
  identical across pairs.
- **SC-007**: In a self-review pass (simulating Phase 1 test writing), every program's
  assertions can be drafted without opening a new question. Any question found is either
  answered in the specification or added to the register.

## Assumptions

- The surface syntax is S-expressions close to Clojure: `[]` vectors, `{}` maps, `#{}` sets,
  keywords. This is the default unless a later decision changes it.
- Timings in the programs (ms values) are injected delays used to make ordering observable.
  They are not performance targets. Assertions use ordering and bounds with generous margins,
  never exact times.
- "Simulated node" means a named execution location inside one process, with a controllable
  network between nodes (delays, loss, crash). Its design belongs to feature 002.
- Recommended defaults adopted without asking, each recorded as a decision during planning:
  - cooperative cancellation (FR-010);
  - settled-only escape of futures (FR-011);
  - first-failure-plus-suppressed propagation (FR-012);
  - per-sender FIFO agents, with failed updates leaving the state unchanged (FR-014);
  - content-addressed functions with SCC hashing (FR-018);
  - non-hygienic macros with `quasi-quote` and explicit `unique-symbol` (FR-006);
  - a stream buffer bound of 1 by default (FR-016).
- The specification documents and programs live in the repository: specification sections in
  `docs/spec/` and programs in `programs/`, as proposed in the roadmap. The exact layout is
  decided in planning.
- Out of scope: any runtime code, the distributed protocol details (Phase 5), the GC root
  convention (H2, Phase 1), and the C module boundaries (H1, Phase 1).
