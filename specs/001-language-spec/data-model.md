# Data Model: 001 — Language Specification by Example

These are the language-level entities. Each one is described by its fields as the language
programmer observes them, never by its representation in memory. The decisions referenced are
recorded in [research.md](./research.md).

## Entities

### Value (data)

- **Kinds**:
  - scalars: `nothing`, boolean, integer (64-bit, D-04), float (IEEE double), string, keyword,
    symbol;
  - collections: list, vector, map, set;
  - error (D-13);
  - function or closure.
- **Invariants**:
  - Values are immutable.
  - Maps and sets are canonical (D-03).
  - `(equal? a b)` implies `(equal? (hash a) (hash b))`.

### Function / Closure

- **Fields**:
  - `function-hash`: the content hash (D-19);
  - `argument-count`;
  - `captured-environment`: an ordered map from captured variable to value;
  - `name`: metadata only, never hashed.
- **Identity**: the pair `(function-hash, captured-environment)`.
- **Relationships**: a function references other definitions by hash. A function in a
  mutually recursive group is identified as `(group-hash, index)`.

### Future

- **Fields**:
  - `identifier`;
  - `scope`: the owning scope;
  - `state`: `:empty`, `:filled` or `:failed`;
  - `value` or `error`;
  - `node`: the owner, in Phase 5.
- **State machine (D-07)**:

  ```text
  :empty ──fill(v)──► :filled
  :empty ──fail(e)──► :failed      (e.kind ∈ {:cancelled, :timeout, :deadlock,
                                     :node-unreachable, :transport, :remote, …})
  (no other transitions; first transition wins; final)
  ```

- **Invariants**:
  - A future in `:empty` always has a live owning scope (D-10).
  - Futures are equal only by identity (D-02).

### Scope

- **Fields**:
  - `identifier`: 128-bit, globally unique (D-21);
  - `parent`;
  - `children`: futures, and nested scopes;
  - `status`: `:running`, `:cancelling`, `:joined-successfully` or `:joined-failed`;
  - `primary-error`;
  - `suppressed`: a vector of errors.
- **State machine (D-08, D-09, D-11)**:

  ```text
  :running ──body returns & all children filled──► :joined-successfully
  :running ──child fails / cancel! / parent cancels──► :cancelling
  :cancelling ──all children settled──► :joined-failed  (raises primary error, suppressed attached)
  ```

- **Serializable part**: the `identifier`, the owner node and the cancellation capability, never the
  children (D-20, D-21).

### Agent

- **Fields**:
  - `identifier`;
  - `state`: the last committed value;
  - `queue`: pending updates, FIFO per sender (D-15);
  - `owner-node`: in Phase 5.
- **Update lifecycle (D-15, D-16)**:

  ```text
  queued ──sender scope cancelled──► withdrawn       (sender future :failed :cancelled)
  queued ──dequeued──► running
  running ──f returns s'──► committed               (future :filled; or :cancelled if sender scope was cancelled meanwhile)
  running ──f throws e──► rejected (state unchanged; future :failed e)
  ```

- **Invariants**:
  - At most one update runs at a time.
  - Reading the state never blocks.
  - An agent is not owned by any scope.

### Stream

- **Shape**: `nothing`, or a cell `{first: value, rest: Future<Stream>}`.
- **Fields**:
  - `maximum-ahead`: the buffer bound, 1 by default;
  - `owner-scope`: the scope that owns the producer (D-17).
- **Invariants**:
  - The number of items produced but not yet demanded is at most `maximum-ahead`.
  - When the owning scope exits, or the stream is closed, the producer is cancelled.

### Error

- **Fields**:
  - `:kind` (D-13);
  - `:message`;
  - `:data`;
  - `:origin {:node :scope}`;
  - `:cause`;
  - `:suppressed`.
- **Invariants**: an error is a serializable value, and `:remote` keeps the original kind in
  its `:cause`.

### Node

- **Fields**: `name`, `identifier`.
- **Identity**: by identity.
- **Phase 0–1**: nodes are simulated in one process (D-25).

### Local resource

- **Kinds**: file, socket, handle.
- **Flag**: `local-resource`.
- **Behavior**: it cannot be serialized (D-20). Sending one raises `:not-serializable` at the
  sender.

### Reference program

- **Files**: `main.qx`, `conditions.qx`, `expected.qx`, `README.md`, and `actors.qx` when it has
  an actor version.
- **Format**: see [contracts/reference-program.md](./contracts/reference-program.md).

### Decision / Hypothesis records

- A decision has an ID, a decision, its rationale, alternatives, an "inherited by" list and a
  state.
- A hypothesis has an ID, a statement, the phase that revisits it and a state.
- Both follow constitution Principle VII.

## Equality, hash and serialization table

| Kind | `equal?` | `hash` | Serialization class (D-20) |
|---|---|---|---|
| nil, boolean | value | value | by value |
| integer | value (≠ float of the same magnitude) | value | by value |
| float | value; IEEE rules except `(equal? not-a-number not-a-number)` is true | value | by value |
| string, keyword, symbol | value | value | by value |
| list, vector | structural, ordered | ordered combination | by value (or by node hash, Phase 5c) |
| map, set | structural, unordered | order-independent combination | by value (or by node hash, Phase 5c) |
| error | structural | structural | by value |
| function / closure | function-hash and captured environment | combination of both | by value if the environment is serializable; otherwise `:not-serializable` |
| future | identity | identity | by remote reference |
| agent | identity | identity | by remote reference |
| scope | identity | identity | by remote reference (identifier and cancellation capability only) |
| node | identity | identity | by reference (name and identifier) |
| local resource | identity | identity | not serializable |

Note on NaN: structural equality needs `equal?` to be reflexive, or maps keyed by NaN break. This
departs from IEEE `numerically-equal?`, which still follows IEEE (`(numerically-equal? not-a-number not-a-number)` is false). It is
recorded under D-02. `[OPINION]`
