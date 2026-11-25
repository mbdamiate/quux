# Research and Decisions: 001 — Language Specification by Example (Phase 0)

**Feature**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md) | **Date**: 2026-11-24

This file closes the design decisions of Phase 0, numbered from D-01 (the project log in
`docs/decisions.md` was empty). Each decision has:

- **Decision**: what was chosen;
- **Rationale**: why it was chosen;
- **Alternatives**: the options considered and their trade-offs;
- **Inherited by**: the later phases that depend on it.

**Labels.** Every claim is labeled per constitution Principle VII:

- `[FACT: source]`: verified against a cited source;
- `[MEMORY]`: from prior knowledge, may be outdated;
- `[OPINION]`: a judgement;
- `[ESTIMATE]`: used only for risk probabilities and relative sizes.

**Research note.** Phase 0 makes no version- or library-dependent decision, so no web research
was required. References to other languages are `[MEMORY]` and only motivate design choices.
They are not compatibility claims. Facts about library and tool versions are deferred to the
Phase 2 gate (feature 003), as the constitution requires.

---

## Values, syntax, evaluation

### D-01 — Surface syntax: S-expressions with collection literals

- **Decision**: S-expressions with literal syntax for each collection and for keywords:

  | Literal | Meaning |
  |---|---|
  | `(…)` | list |
  | `[…]` | vector |
  | `{…}` | map |
  | `#{…}` | set |
  | `:name` | keyword |

  There are **no reader shorthands** (naming rules R5 and R6): no `'x`, `` `x ``, `~x`, `~@x`
  or `@x`. Quoting and waiting are always written as forms: `(quote x)`, `(quasi-quote …)`,
  `(unquote x)`, `(unquote-splicing x)`, `(wait-for x)`. The literal for absence is `nothing`
  (R7). Comments start with `;`. Namespace-qualified symbols are written `namespace/name`. All
  public names follow `docs/naming.md` (D-27).
- **Rationale**: collections are central to the design (canonical persistent structures), and
  literal syntax keeps the 12 programs short. The author and the study references (Clojure
  agents and futures) already use this notation `[OPINION]`.
- **Alternatives**: Scheme (lists only, with constructors for vectors and maps); this is more
  verbose for maps and sets. A custom syntax was rejected as an unneeded novelty. Clojure's
  reader shorthands were rejected by the owner during the naming review (D-27).
- **Inherited by**: Phase 1 (reader).

### D-02 — Equality and hashing per value kind

- **Decision**: `equal?` is structural for data values. Integers and floats are different types,
  so `(equal? 1 1.0)` is false; `numerically-equal?` is numeric equality across types.
  - Functions and closures are equal when their content hash (D-19) and their captured
    environments are equal.
  - Futures, agents, scopes, nodes and local resources are equal only by identity.

  `hash` agrees with `equal?` for every kind. `equal?` is reflexive for NaN, so `(equal? not-a-number not-a-number)` is true
  and maps and sets stay well-behaved. `numerically-equal?` follows IEEE. The complete table is in
  [data-model.md](./data-model.md#equality-hash-and-serialization-table).
- **Rationale**: content identity for functions follows from D-19. Comparing futures by
  content would require blocking (waiting) inside `equal?`, and its result would change over
  time. Identity comparison is stable.
- **Alternatives**:
  - Functions equal only by reference (as in Clojure `[MEMORY]`). This breaks content
    addressing: two nodes that load the same code would see different functions.
  - Futures compared by value once filled. This makes equality time-dependent.
- **Inherited by**: Phase 3 (VM representation), Phase 5 (remote references keep identity).

### D-03 — Canonical persistent collections and the two hashes

- **Decision**:
  - Maps and sets are CHAMP (Steindorfer & Vinju, OOPSLA 2015 `[MEMORY]`) in canonical form:
    the same contents always give the same tree.
  - Vectors are a bit-partitioned persistent trie with a tail. RRB, which adds O(log n)
    concatenation, is postponed until a program needs it.
  - Lists are persistent cons lists.
  - Two distinct hashes exist:
    1. The **value hash** is a fast 64-bit hash used for `equal?`/`hash`, maps and sets in memory.
       For an unordered collection it is order-independent. It is cached per node.
    2. The **content hash** is a cryptographic hash of a canonical serialization. It is used
       for code identity (D-19) and for transferring structures by node hash (Phase 5c). It is
       computed lazily, never on the hot path.
  - The algorithms for both hashes are chosen in Phase 2.
- **Rationale**: one hash cannot be both cheap enough for every map insertion and
  collision-resistant enough to name code across machines. Canonical form makes both hashes
  well-defined over values, independent of construction history (spec FR-003).
- **Alternatives**:
  - A single cryptographic hash everywhere: too slow for map operations `[OPINION]`.
  - HAMT without canonicalization (classic Clojure `[MEMORY]`): equal maps can have different
    trees after deletions, which breaks Merkle-style transfer.
- **Inherited by**: Phase 1 (structures), Phase 2 (algorithm choice), Phase 5 (5c transfer by
  hash).

### D-04 — Numerics

- **Decision**:
  - Integers are 64-bit signed. Overflow in `+`, `-`, `*`, `increment`, `decrement` and `absolute-value` raises an
    error of kind `:overflow`; it never wraps silently.
  - Floats are IEEE-754 double.
  - Integer division is explicit: `quotient` truncates and `modulo` floors. `/` on integers returns a
    float only when an argument is already a float; otherwise it raises `:inexact-division` when the
    result is not exact.
  - There are no implicit conversions besides integer-to-float in mixed arithmetic.
  - Bignums and rationals are postponed, to be revisited after Phase 3, when a library can be
    adopted at the gate.
- **Rationale**: this is the user's choice (Q3: A). Raising on overflow is consistent with
  "errors are never hidden" (D-13).
- **Alternatives**:
  - Automatic promotion to bignum, as in Scheme or Clojure `+'` `[MEMORY]`. It needs bignums in
    Phase 1.
  - Wraparound, which hides errors.
- **Inherited by**: Phase 1, Phase 6 (overflow checks in native code).

### D-05 — Tail calls

- **Decision**: every call in tail position is a proper tail call (TCO is mandatory; there is
  no `recur`-only restriction). The tail positions are:
  - the last expression of a `function` body, `sequence`, `let` and `let-functions`;
  - both branches of `if`;
  - the last clause of the derived forms `conditional`, `case`, `when`, `and` and `or`.

  The body of `try` is not a tail position when `catch` or `finally` is present. The
  specification's test example runs 1,000,000 mutually recursive calls (`even?`/`odd?`) in
  constant stack.
- **Rationale**: this is a constitution principle. Clojure's `recur` exists because of the JVM
  `[MEMORY]`; quux has no such constraint.
- **Alternatives**: `recur` only, which is incompatible with the stated requirement.
- **Inherited by**: Phase 1 (evaluator), Phase 3 (VM), Phase 6 (`musttail`).

### D-06 — Macros and their relation to code identity

- **Decision**:
  - `define-macro` is not hygienic.
  - `quasi-quote` qualifies symbols with their namespace (the Clojure model `[MEMORY]`).
    There is no automatic gensym; fresh symbols are created explicitly with
    `(unique-symbol "name")`.
  - Macros expand completely before a definition is hashed: the content hash (D-19) covers the
    expanded code, never the macro calls. A macro is therefore a dependency at compile time,
    not at run time. Changing a macro changes the hashes of the definitions that are
    re-expanded with it, and only when they are re-expanded.
- **Rationale**: this keeps the cost low for one developer, and namespace qualification
  avoids the common capture bugs. Hashing after expansion makes the identity of remote code
  independent of the macro environment of the receiver.
- **Alternatives**: full hygiene (`syntax-rules`/`syntax-case`), which needs a more complex
  expander and syntax objects carrying scopes `[OPINION]`. It is recorded as possible later,
  without affecting hashes, because hashing happens after expansion.
- **Inherited by**: Phase 1 (expander), Phase 5 (5c hashing).

---

## Concurrency

### D-07 — Future states

- **Decision**: a future has three states: `:empty`, `:filled` (with a value) and `:failed`
  (with an error value, D-13). Leaving `:empty` happens once and is final. Cancellation is the
  `:failed` state with an error of kind `:cancelled`. `(state f)` returns the state without
  blocking.
- **Rationale**: this is the base decision. Making "cancelled" a kind of error keeps three
  states and allows a single handling path.
- **Alternatives**: a fourth `:cancelled` state, which Java's `Future` has `[MEMORY]`. It forces
  every combinator to handle it as a special case.
- **Inherited by**: Phase 3, Phase 5 (a remote future has the same three states).

### D-08 — Scopes, spawning and the root scope

- **Decision**:
  - `(with-scope [s] body…)` creates a scope bound to `s` and makes it the current scope for
    the dynamic extent of `body`.
  - `(future expr)` spawns in the current scope; `(spawn s expr)` spawns in an explicit scope,
    which must be the current scope or one of its ancestors that is still alive.
  - The program runs inside an implicit root scope. A REPL evaluation runs in a fresh child
    of the REPL session's scope.
  - On exit, the scope joins all of its children. If one child fails, the remaining children
    are cancelled (D-09) and the scope raises D-11's error.
  - A scope can also be cancelled from outside with `(cancel! s)`.
- **Rationale**: this is the nursery model of Trio and Kotlin's `coroutineScope` `[MEMORY]`.
  The implicit current scope keeps the code free of async contagion: any function can spawn
  without receiving the scope as a parameter.
- **Alternatives**:
  - Explicit scopes only, which passes a scope through every signature. This is close to the
    contagion the design forbids.
  - Unstructured futures as in Clojure `[MEMORY]`, which go against the base decision.
- **Inherited by**: Phase 1, Phase 3.

### D-09 — Cancellation semantics (cooperative)

- **Decision**: cancellation is a request and is observed only at cancellation points:
  - `wait-for`, agent `update` and `transact`, stream advance, `sleep`, blocking I/O, remote
    calls;
  - an explicit `(check-cancellation)`.

  At a point, the task raises `:cancelled`. `finally` blocks run. `(shield-from-cancellation body…)` protects
  cleanup code from further cancellation until it ends, like Trio's shielded cancel scopes
  `[MEMORY]`. Cancellation is never forced: threads are not killed and stacks are not unwound
  asynchronously. A pure loop with no points runs until it finishes, and the documentation
  tells users to put `(check-cancellation)` in long pure loops.
- **Rationale**: forced termination in a shared heap with C resources leaves locks and
  resources inconsistent. The constitution chose cooperative cancellation. Cancellation points
  coincide with the IR's suspension points (Phase 6).
- **Alternatives**:
  - Forced cancellation, like `pthread_cancel` or asynchronous exceptions: unsafe.
  - Cancellation only at `wait-for`: too coarse for streams and I/O.
- **Inherited by**: Phase 3 (scheduler), Phase 4 (I/O cancellation), Phase 5 (cross-node
  cancellation, 5b), Phase 6 (IR cancellation points).

### D-10 — Futures that escape their scope

- **Decision**: a future value may be returned or stored past the end of its scope. Because
  the scope joined its children, such a future is always `:filled` or `:failed`, never
  `:empty`. `wait-for` on it returns the value or raises the error, immediately. No pending
  future exists without a live enclosing scope.
- **Rationale**: no extra rule is needed for escape; the invariant follows from joining.
  Results remain first-class values.
- **Alternatives**:
  - Forbidding escape statically: this needs an effect or region system, which is too
    costly.
  - Raising `:scope-ended` on `wait-for` after escape: a needless failure, because the value is
    known.
- **Inherited by**: Phase 3, Phase 5 (a remote future reference is valid only while its scope
  lives, or once it is settled; see D-21).

### D-11 — Several children fail

- **Decision**: the scope raises the first non-cancellation failure in order of completion as
  the primary error. Other non-cancellation failures that occur before the join ends are
  attached in `:suppressed`. Cancellation errors that the first failure caused are dropped.
  Tests must assert membership ("one of A's or B's error"), never a specific winner, when
  several failures race.
- **Rationale**: this is the model of Kotlin and Java `StructuredTaskScope` (`ShutdownOnFailure`
  / `Joiner.awaitAllSuccessfulOrThrow`) `[MEMORY]`. Information is kept without the
  ergonomic cost of error groups.
- **Alternatives**:
  - Trio `ExceptionGroup` `[MEMORY]`: complete and deterministic in its content, but every
    handler must unpack groups.
  - Lowest child index wins: deterministic, but the cancellation that the first failure causes
    can turn into the reported error. Rejected `[OPINION]`.
- **Amendment (implementation of US2, 2026-11-24)**: the cancellations that a combinator
  causes (the losers of `first-success`, the inner computation of `with-timeout`, the rest of
  `all` after a failure) are dropped like the scope's own cancellations, so they do not fail the
  scope. Programs 02 and 10 depend on this rule. See `docs/spec/07-scopes-and-cancellation.md`,
  rule 8.
- **Inherited by**: Phase 3, Phase 5 (5b).

### D-12 — Combinators and the async macro

- **Decision**:

  | Form | Behavior |
  |---|---|
  | `(all futures)` | A future of a vector of results, in the order of `futures`. It fails on the first failure and cancels the rest. |
  | `(first-success futures)` | The first success. The others are cancelled. If all fail, it fails with `:all-failed` and `:errors` holding every error, in the order of `futures`. |
  | `(then f g)` | A future of `(g (wait-for f))`, spawned in the current scope. |
  | `(with-timeout milliseconds future-or-function)` | Fails with `:timeout` and cancels the inner computation when the time is up. The first transition wins. |
  | `(let-parallel [a e1 b e2] body)` | Spawns every binding in the current scope, then waits for them in order before the body. |

  There is no `await` keyword: `(wait-for f)` suspends the current task and does not mark the caller.
- **Rationale**: this follows the base decision (suspension belongs to the runtime). `let-parallel`
  is the sequential-looking form that program 1 needs.
- **Alternatives**: async/await with colored functions, which the base decision rules out.
- **Inherited by**: Phase 1, Phase 3.

### D-13 — Error model

- **Decision**: an error is an immutable map-like value
  `{:kind k :message s :data map :origin {:node n :scope identifier} :cause e :suppressed [e…]}`.
  - Standard kinds:

    | Group | Kinds |
    |---|---|
    | Concurrency | `:cancelled`, `:timeout`, `:deadlock`, `:all-failed` |
    | Distribution | `:node-unreachable`, `:transport`, `:remote`, `:not-serializable` |
    | Numerics | `:overflow`, `:inexact-division` |
    | Program | `:wrong-argument-count`, `:wrong-type`, `:undefined-name`, `:user` |

  - `(raise e)`, `(try … (catch kind-or-predicate e …) (finally …))` and `(error kind message data)`
    are the forms.
  - A failed future holds an error value, and `wait-for` re-raises it with a `:cause` chain that
    preserves the origin.
  - `:remote` wraps an error raised on another node, keeping the original `:kind` in its
    `:cause`.
  - Retries are never implicit: no runtime layer retries an operation without the program
    asking.
- **Rationale**: network errors and timeouts are part of the model from the start, as the base
  decision requires. Errors are serializable values (D-20), so they can cross nodes.
- **Alternatives**:
  - Typed exception hierarchies: these need a class system.
  - Result values without exceptions: they make code that waits on many futures verbose `[OPINION]`.
- **Amendment (implementation of US1 and US2, 2026-11-24)**: the kinds `:division-by-zero`,
  `:index-out-of-range` and `:resource-closed` were added (approved in `docs/naming.md`). See `docs/spec/12-errors.md`.
- **Inherited by**: Phase 1, Phase 3, Phase 5.

### D-14 — Deadlock detection

- **Decision**: the runtime keeps a wait-for relation: which task waits on which future, and
  which task fills that future. When a `wait-for` would close a cycle, every future in the cycle
  fails with `:deadlock`, and `:data {:cycle [ids…]}` lists the cycle. The error propagates
  through the scopes as usual.
  - The guarantee holds within one node, and also across simulated nodes in Phase 1.
  - A distributed cycle across real nodes is not guaranteed to be detected; the documented
    remedy there is `with-timeout`.
  - An agent that waits inside its own update on the future of a send to itself is a cycle,
    and is detected.
- **Rationale**: this is the user's choice (Q1: A). The cost in the blocking prototype is a
  mutex-protected graph. Phase 3 must keep this cheap (H8).
- **Alternatives**:
  - Structural restriction: `wait-for` only on children or settled futures.
  - No detection, timeouts only.
- **Inherited by**: Phase 3 (H8), Phase 5 (distributed: timeouts only).

### D-15 — Agent semantics

- **Decision**:
  - `(agent v)` creates an agent. `(state a)` reads the last committed value and never
    blocks.
  - `(update a f & args)` enqueues `f`. Its future yields the new state.
  - `(transact a f & args)` enqueues `f`, which returns `[new-state reply]`. Its future yields
    `reply`. This is the form for "test and act" (program 4).
  - Updates run one at a time, in the order in which the agent accepts them. The order between
    sends from one sender is FIFO; between different senders there is no defined order.
  - An update that throws leaves the state unchanged and fails only that send's future. The
    agent keeps working. This differs from Clojure's default failed-agent mode `[MEMORY]`.
  - An update may `update`/`transact` other agents or itself; these are enqueued, never run
    inline. `wait-for` inside an update is allowed but holds the agent's queue (D-14 detects
    self-waits).
  - An agent's updates run in an internal scope owned by the agent, not by the sender.
  - Agents are not owned by scopes. They live while they are reachable.
- **Rationale**: per-sender FIFO is what one node can guarantee cheaply, and it is what
  Phase 5d can keep with sequence numbers per sender. `transact` avoids a separate lock or
  compare-and-swap for test-and-act.
- **Alternatives**:
  - A global total order: impossible to keep across nodes without consensus.
  - Clojure's `send`/`send-off` split by thread pool `[MEMORY]`: the Phase 1 runtime decides
    blocking behavior, so the split is unnecessary.
- **Inherited by**: Phase 1, Phase 3, Phase 5 (5d).

### D-16 — Scope of an agent send's future and pending-send cancellation (resolves H3)

- **Decision**: the future returned by `update`/`transact` belongs to the sender's current
  scope, so it does not outlive that scope, as the base rule requires. The agent itself is
  outside the scope tree (D-15), so any scope may use it. When the sender's scope is
  cancelled:
  - if the update is still queued, it is withdrawn and never applied;
  - if it has started, it runs to completion and commits atomically.

  In both cases the sender's future ends as `:cancelled`. A sender that needs to know the
  outcome reads the state or, from Phase 5d on, uses operation ids.
- **Rationale**: this is the user's choice (Q2: A). The structured-concurrency invariant
  covers futures; an agent is state with identity, not a task, so it does not break the rule.
- **Alternatives**:
  - Every accepted update is always applied: simpler, but leaves orphaned work.
  - Updates are interrupted midway: this needs cancellation points inside update functions
    and risks partial reasoning.
- **Inherited by**: Phase 3, Phase 5 (5d, H5).

### D-17 — Streams

- **Decision**:
  - A stream is `nothing` (the end) or a cell `(first, rest-future)`.
  - `(make-stream producer-function)` spawns the producer in the current scope, with a buffer bound
    `:maximum-ahead` (default 1): the producer suspends when it is more than `:maximum-ahead` items ahead of
    the consumer's demand.
  - `stream-transform`, `stream-filter`, `stream-take` and `stream-reduce` are pull-driven and asynchronous; `stream-take` stops
    pulling after `n` items.
  - Leaving the scope that owns the stream, or calling `(close! s)`, cancels its producer.
    Reading a rest after that fails with `:cancelled`.
  - Pulling is the only form of demand, so backpressure needs no protocol.
- **Rationale**: the base decision defines streams as "linked lists whose rest is a future" and
  calls for natural backpressure by pull. Owning the stream by the creator's scope keeps
  infinite producers structured.
- **Alternatives**:
  - Push streams with explicit backpressure, as in Reactive Streams `request(n)` `[MEMORY]`:
    more protocol.
  - Unbounded lazy sequences: no bound, so no backpressure.
- **Amendment (implementation of US1, 2026-11-24)**:
  - `emit` is a cancellation point while it waits for demand.
  - When the owning scope's body returns, the scope cancels its stream producers instead of
    waiting for them, because a producer may never finish. The resulting `:cancelled` errors do
    not fail the scope.
  - See `docs/spec/07-scopes-and-cancellation.md` (rule 4) and `docs/spec/10-streams.md`.
- **Inherited by**: Phase 1, Phase 4 (I/O streams), Phase 5 (5e streams between machines keep
  `:maximum-ahead` as a credit window).

### D-18 — Deterministic data parallelism

- **Decision**:
  - `parallel-transform` preserves order.
  - `parallel-reduce`, reducers and fork/join combine partial results along a fixed tree whose shape
    depends only on the input size and a chunk constant, never on scheduling. Work-stealing
    may run the chunks in any order, but the combination order is fixed. This gives
    bit-identical float results (Q3: A).
  - All parallel work runs in a child scope: a failure cancels the remaining chunks
    (program 8).
- **Rationale**: determinism for pure functions is a base requirement. Floating-point addition
  is not associative, so only a fixed tree guarantees identical results.
- **Alternatives**: dynamic splitting by the scheduler, as in Java fork/join with adaptive
  splitting `[MEMORY]`. It is faster, but float results then depend on the order of execution.
- **Inherited by**: Phase 3 (H12), Phase 5 (5e: partitioning follows the same tree).

---

## Distribution-affecting decisions

### D-19 — Function identity by content (resolves the Phase 0–1 part of H4)

- **Decision**:
  - A top-level definition's content hash is the cryptographic hash (D-03) of its canonical
    form:
    - the expanded AST (D-06);
    - with local variables replaced by de Bruijn indices;
    - with each free reference to another definition replaced by that definition's hash;
    - with literals in canonical form.
  - Names are metadata and are not hashed.
  - Mutual recursion: the call graph's strongly connected components are hashed as one unit,
    with members ordered by the hash of their own bodies, in which references inside the
    group are placeholders. Each member is identified by the group hash plus its index. This
    follows Unison's approach to cycles `[MEMORY]`.
  - A closure is identified by `(function-hash, captured-environment-values)`. The function
    hash never covers the environment. Captured values serialize by D-20; identity values in
    the environment travel as remote references.
- **Rationale**: this lets a node send a hash instead of code, cache code by hash, and fetch
  what is missing (Phase 5c). Excluding the environment keeps the code hash stable across
  calls.
- **Alternatives**:
  - Identity by name, with the same code on every node (Erlang `[MEMORY]`): this needs
    coordinated deployment and allows silent version skew.
  - Shipping bytecode together with the environment: no cache, and code is sent repeatedly.

  Both stay as fallbacks compared in Phase 5c.
- **Inherited by**: Phase 1 (the prototype computes hashes for definitions, even though nothing
  is sent yet), Phase 5 (5c protocol; the transferred part of H4).

### D-20 — Serialization classes and local resources

- **Decision**: every value kind belongs to exactly one class.
  1. **By value**: nil, booleans, numbers, strings, keywords, symbols, collections, errors, and
     closures whose environment is entirely serializable. Large persistent structures may
     later be sent by node hash (Phase 5c).
  2. **By remote reference**: futures, agents and scopes. What is sent is an id plus the owner
     node, never the contents.
  3. **Local only**: files, sockets, OS handles, and any value a native library creates with
     the `local-resource` flag.

  Sending a class-3 value, or a closure that captures one, fails at the sender with
  `:not-serializable`. The error's `:data` names the value kind and the path inside the
  structure. `(serializable? v)` lets a program check beforehand.
- **Rationale**: a failure at the sender is diagnosable; a failure at the receiver is not. The
  flag is a property of the kind, so the check is a walk over the value, not a type system.
- **Alternatives**:
  - Silently proxying local resources as remote references, as Java RMI does with remote
    objects `[MEMORY]`: this hides that a call is remote, one of the fallacies.
  - A static marker in the type system: there are no static types.
- **Inherited by**: Phase 1 (`serializable?` in the prototype), Phase 5 (wire format).

### D-21 — Scope identity

- **Decision**:
  - A scope id is globally unique: a 128-bit random value created when the scope is created.
    It does not depend on node ids, so it works before nodes exist.
  - A scope reference carries the id, the owner node and a cancellation capability. Holding
    it lets the holder request `cancel!` and observe whether the scope is still alive.
  - A scope's children are never serialized.
  - A remote future created through `spawn-on` (D-22) belongs to the caller's scope. Its
    remote execution is a child task of a proxy scope on the remote node, linked to the
    caller's scope id, so that cross-node cancellation (Phase 5b) has an address.
- **Rationale**: the base decision requires scopes to be first-class values with identity, so
  that cancellation can cross the network.
- **Alternatives**:
  - `(node-id, counter)`: compact, but it requires node ids to be assigned first.
  - No serializable scopes, with cancellation sent only along the edges of remote futures:
    this loses the ability to cancel a scope passed as an argument.
- **Inherited by**: Phase 3, Phase 5 (5b leases and heartbeats attach to the scope id; H6).

### D-22 — Remote calls are explicit

- **Decision**:
  - The only primitives that cross nodes are `(spawn-on node expr)`, which returns a future in
    the current scope, and remote agents and streams obtained explicitly.
  - Nodes are values with identity: `(node :name)`.
  - Nothing makes a local-looking call remote.
  - A remote future can fail with `:node-unreachable`, `:transport` or `:remote` in addition to
    the local kinds.
  - Promise pipelining (sending `then` to the node that holds the future, as in E and
    Cap'n Proto `[MEMORY]`) is allowed by these semantics but is not required before Phase 5a.
- **Rationale**: this avoids the fallacies of distributed computing, because the program sees
  every remote boundary.
- **Alternatives**: transparent remote references (RPC in disguise).
- **Inherited by**: Phase 5 (5a; H6).

### D-23 — Modules compatible with content addressing

- **Decision**:
  - A namespace is an immutable map from name to definition hash.
  - `(namespace name (:import …))` binds names; `define` creates or replaces the binding to a new hash.
  - References in compiled code are by hash. Redefinition does not change existing closures,
    which keep the old hash; new code resolves the name again.
  - The REPL resolves names when a form is evaluated.
  - The policies for code reloading (re-resolving live references) are transferred to Phase 5c
    and Phase 7.
- **Rationale**: renaming must not change identity, and two nodes must agree on what a hash
  means regardless of how they name it.
- **Alternatives**: Clojure-style vars with late binding by name `[MEMORY]`. Easy to reload,
  but identity becomes the name, which conflicts with D-19.
- **Inherited by**: Phase 1, Phase 5 (5c), Phase 7.

### D-24 — Actors stay outside the core; the comparison covers programs 4, 10 and 12

- **Decision**: actors are not specified as part of the language.
  - For programs 4, 10 and 12, an actor version is written against a hypothetical library
    built on agents (`spawn-actor`, `receive`, `send-message`, `link`).
  - The comparison table has the same rows for each pair:
    - lines of code;
    - where atomicity lives;
    - how a failure is observed by the caller;
    - what supervision with restart adds;
    - what is lost without actors.
- **Rationale**: the base decision excludes actors from the core. The comparison feeds the
  Phase 4 evaluation and Phase 5f.
- **Inherited by**: Phase 4, Phase 5 (5f).

### D-25 — Fault injection lives outside program source

- **Decision**: a reference program's source contains only language code. Its injected
  conditions live in a separate `conditions.qx` data file, read by the test harness. The
  conditions are:
  - delays per call site, tagged with `(tag :name expr)`;
  - failures;
  - the node layout and crashes (`{:node :b :crash-after {:completed 4}}`);
  - message loss.

  Programs 11 and 12 therefore run unchanged against simulated nodes (Phase 1) and real nodes
  (Phase 5).
- **Rationale**: spec FR-024. It also keeps the programs readable as documentation.
- **Alternatives**: fault injection written inline in the programs. That changes the program
  between simulation and reality.
- **Inherited by**: Phase 1 (simulator harness), Phase 5 (Docker network tests), Phase 7
  (failure-simulation tool).

### D-26 — Document and program formats

- **Decision**:
  - The specification lives in `docs/spec/NN-topic.md`, one file per topic, using the
    section format of [contracts/spec-section.md](./contracts/spec-section.md).
  - Each program lives in `programs/NN-name/` and holds `main.qx`, `conditions.qx`,
    `expected.qx` and `README.md`; programs that have an actor version also hold `actors.qx`.
  - `expected.qx` holds the assertions as quux data, following
    [contracts/reference-program.md](./contracts/reference-program.md), so that the Phase 1
    reader can load it and the harness can check it.
- **Rationale**: expected behavior written as data rather than prose is "precise enough to
  become an automated test" (SC-001), and it is the first real input to the reader.
- **Alternatives**: expected behavior in prose, which needs to be rewritten as tests in
  Phase 1; JSON or YAML, which brings a second syntax and a parser dependency.
- **Inherited by**: Phase 1 (test harness).

### D-27 — Readable names, with no abbreviations or Lisp heritage

- **Decision**: constitution Principle VIII (v1.1.0) applies.
  - Every public quux name is in full English words. This covers forms, functions, keywords,
    error kinds, literals, reader shorthands and file extensions.
  - Every C identifier is in full words, `snake_case`, with the prefix `quux_` on exported
    symbols. The exceptions are loop counters, externally imposed names and the acronyms
    approved in `docs/naming.md`.
  - The owner approves each name individually in `docs/naming.md`.
- **Rationale**: one person reads the code and the language across long gaps between work
  sessions. Full words do not require remembering conventions that exist only for historical
  reasons (`car`, `cdr`, `cond`, `conj`, `assoc`).
- **Alternatives**:
  - Clojure names, the earlier implicit choice in D-01: familiar to Lisp users, but opaque
    to others.
  - Full words only in quux, with conventional C abbreviations: rejected by the owner.
- **Consequences**: D-01 keeps the S-expression structure and the collection literals, but
  drops every reader shorthand (rules R5 and R6). The owner reviewed the whole glossary name
  by name on 2026-11-24, and D-01..D-26 and the contracts now use the approved names. Notable
  choices:
  - `?` marks predicates and `!` marks effects (`cancel!`, `close!`).
  - The literal for absence is `nothing`.
  - `wait-for` replaces `deref`/`@`.
  - Macros use `quasi-quote`, `unquote` and `unquote-splicing`.
  - Allowed C acronyms: `champ`, `hamt`, `rrb`, `utf8`, `llvm`, `posix`, `ieee`.
- **Inherited by**: Phase 1 (reader, primitives, C conventions, clang-tidy configuration) and
  every later phase.

### D-28 — Linux x86_64 as a CI-only target

- **Decision**: the targets are macOS arm64, Linux arm64 and Linux x86_64 (constitution
  v1.2.0). x86_64 is built and tested only in GitHub Actions CI, with the same CMake presets
  and sanitizer gates as the other targets. Whether the native backend (Phase 6) emits x86_64
  code is decided in Phase 6; the VM must run on all three targets.
- **Rationale**: CI provides x86_64 at no local cost. A second architecture catches problems
  in ABI, alignment, code generation and portability that one architecture hides. arm64 stays
  the stricter test for memory ordering, because its memory model is weaker than x86 TSO.
- **Alternatives**:
  - Keep x86_64 out of scope (the previous decision). This leaves code paths that are never
    tested and an unmitigated risk.
  - Run x86_64 locally under emulation. This is slow, and TSan under emulation is unreliable
    `[MEMORY]`.
- **Consequences**:
  - Feature 002's smoke test includes a CI pipeline.
  - The portability header must cover two architectures.
  - The page size is queried at run time.
  - Phase 2 verifies the availability of GitHub runners (x86_64, arm64, macOS) with links.
- **Inherited by**: Phase 1 (CI pipeline, portability header), Phase 2 (runner and
  toolchain verification), Phase 3 (atomics and memory-order review on two architectures),
  Phase 6 (backend targets).

---

## Hypotheses

| ID | Outcome in this feature |
|---|---|
| H3 | **Confirmed and resolved** by D-15 and D-16. The send future belongs to the sender's scope; agents are outside the scope tree and can be used from any scope. |
| H4 | **Partly resolved** by D-19 and D-06: the hash excludes the environment, cycles are hashed per SCC, and macros are expanded before hashing. **Transferred to 5c**: the fetch protocol, cache and reloading. |
| H1, H2, H7 | Not part of this feature: they belong to feature 002 (Phase 1). |
| H5, H6 | Still open; they belong to Phase 5 (5d; 5a and 5b). |

New hypotheses found while writing these decisions:

| ID | Hypothesis | Revisit in |
|---|---|---|
| H8 | Deadlock detection (D-14) needs a global or striped wait-for structure. Can a work-stealing VM keep it cheap, for example by checking only on the slow suspension path? | Phase 3 |
| H9 | "First failure in completion order" (D-11) makes the primary error nondeterministic. Is a deterministic order needed for test oracles or for replay in deterministic simulation? | Phase 1, Phase 7 |
| H10 | Two hashes per structure (D-03). Is the memory and complexity cost of caching both justified, or should the content hash be computed only on demand, never cached? | Phase 2 (spike) |
| H11 | Streams owned by their creator's scope (D-17). Is passing a live stream to a sibling scope a common need that this rule makes awkward? | Phase 1 (programs 6, 7, 10) |
| H12 | A fixed reduction tree (D-18) limits how work-stealing balances irregular work. Is the loss acceptable for fork/join over unbalanced trees (program 8)? | Phase 3 |
