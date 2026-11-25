# 13 — Serialization and Identity

**Question:** Which values can leave a node, how is a function identified, and how are remote
things referenced?

**Status:** Partially open

## Answer

**Decision:** D-19 (function identity by content), D-20 (serialization classes), D-21 (scope
identity), D-22 (explicit remote calls), D-03 (content hash), D-06 (hashing after expansion)

### Function identity (D-19)

1. Every top-level definition has a **content hash**: a cryptographic hash (algorithm chosen in
   Phase 2) of its canonical form, `Function / Closure.function-hash` in
   [data-model.md](../../specs/001-language-spec/data-model.md). The canonical form is:
   - the code **after full macro expansion** (section 04);
   - with local variables replaced by their position (de Bruijn indices), so parameter names do
     not matter;
   - with every reference to another definition replaced by that definition's hash;
   - with literals written in their canonical form (section 02).
2. The name of a definition (`Function / Closure.name`) is metadata and is never hashed.
   Renaming does not change identity (section 14).
3. **Mutual recursion.** The definitions that call each other (a strongly connected component of
   the call graph) are hashed together as one group. Inside the group, references to members are
   placeholders, and the members are ordered by the hash of their own bodies. Each member is then
   identified by the group hash plus its index.
4. A **closure** is identified by its function hash plus the values it captured
   (`Function / Closure.captured-environment`). The function hash never includes the captured
   environment.
5. `(function-hash function)` returns the content hash of the code of `function`, without the
   captured environment.

### Serialization classes (D-20)

6. Every value kind belongs to exactly one class:

   | Class | Kinds | How it travels |
   |---|---|---|
   | **By value** | `nothing`, booleans, numbers, strings, keywords, symbols, lists, vectors, maps, sets, errors, closures whose captured values are all serializable | copied; large persistent structures may later travel by node hash (Phase 5c) |
   | **By remote reference** | futures, agents, scopes, nodes | an identifier plus the owner node; never the contents |
   | **Local only** | files, listeners, connections, other local resources (flag `local-resource`) | cannot leave the node |

7. A stream cell travels by value: its `first` value is copied and its `rest` future travels by
   remote reference. Advancing a stream on another node therefore asks the producer's node for
   the next value, and backpressure crosses the network unchanged (program 12).
8. Sending a local-only value, or a closure that captures one, **fails at the sender** with
   `:not-serializable`. The error's `:data` names the value kind and its path inside the value
   being sent. It never fails silently on the receiver.
9. `(serializable? value)` returns whether `value` can be sent, without sending it.

### Scope identity (D-21)

10. Every scope has a globally unique identifier of 128 random bits (`Scope.identifier`). The
    identifier does not depend on nodes, so it exists before any node does.
11. A scope travels by remote reference: its identifier, its owner node and a cancellation
    capability. Whoever holds the reference can request `cancel!` and observe whether the scope
    is still alive. A scope's children never travel.
12. The work started by `spawn-on` (rule 13) runs on the remote node inside a **proxy scope**
    linked to the caller's scope identifier, so that cancelling the caller's scope reaches it
    (Phase 5b).

### Remote calls are explicit (D-22)

13. `(spawn-on node expression …)` evaluates the expressions on `node` and returns a future in the
    **current scope**. `(node name)` returns the node with that name. The expressions travel as
    a closure: the hash of the code, plus the captured values in the classes above. The remote
    node fetches any code it does not have (Phase 5c).
14. A remote future has the same three states as a local one (section 06). In addition to the
    local kinds, it can fail with:
    - `:node-unreachable`: the owner node cannot be reached (it crashed, or the network is
      partitioned);
    - `:transport`: a message was lost or corrupted;
    - `:remote`: the expression raised on the remote node; `:cause` keeps the original error.
15. Nothing else crosses a node boundary implicitly. A value with identity that was created on
    another node (for example an agent created by `spawn-on`) is used through its remote
    reference, and every operation on it can fail with the remote kinds above.
16. **No implicit retries.** When a remote operation fails, the program decides whether to retry
    or recompute (programs 11 and 12).

### Idempotent updates

17. `(update-once agent operation-identifier function argument …)` is `update` (section 09) that
    is **applied at most once per operation identifier**. Sending it again with the same
    identifier, for example after a `:transport` failure, returns a future of the state the
    first application produced, without applying it again. The identifier is any serializable
    value chosen by the program.
18. How long the agent's owner remembers identifiers is hypothesis H5. **Open — closed in
    Phase 5d.**

## Examples

```quux
;; identity by content
(define-function double [x] (* 2 x))
(define-function twice-as-much [amount] (* 2 amount))
(equal? (function-hash double) (function-hash twice-as-much))     ;=> true

;; closures: same code, different captured values
(define-function adder [n] (function [x] (+ x n)))
(equal? (function-hash (adder 1)) (function-hash (adder 2)))       ;=> true
(equal? (adder 1) (adder 2))                                       ;=> false
```

<!-- example-requires: distribution -->
```quux
;; serialization classes
(serializable? {:balance 100})                                     ;=> true
(with-scope [scope]
  (serializable? (future 1)))                                      ;=> true   (by remote reference)
(with-scope [scope]
  (let [file (open-file "lines.txt")]
    (serializable? (function [] (read-line file)))))               ;=> false
```

<!-- example-requires: simulated-nodes -->
```quux
;; sending a local resource fails at the sender
(with-scope [scope]
  (let [file (open-file "lines.txt")]
    (wait-for (spawn-on (node :b) (read-line file)))))
;!! :not-serializable

;; a remote computation
(with-scope [scope]
  (wait-for (spawn-on (node :b) (double 21))))                     ;=> 42

;; idempotent update after a lost acknowledgement
(define account (with-scope [scope] (wait-for (spawn-on (node :b) (agent 0)))))
(with-scope [scope] (wait-for (update-once account 7 + 10)))      ; first send: applied
(with-scope [scope] (wait-for (update-once account 7 + 10)))      ; resend: not applied again
(with-scope [scope] (wait-for (spawn-on (node :b) (state account))))   ;=> 10
```

## Errors

| Kind | When |
|---|---|
| `:not-serializable` | a local-only value, or a closure that captures one, is sent; raised at the sender |
| `:node-unreachable` | the node that owns a future, agent or scope cannot be reached |
| `:transport` | a message was lost or corrupted |
| `:remote` | the remote expression raised; `:cause` holds the original error |

## Open points

- The rest of H4: the protocol for fetching missing code, the per-node code cache, and code
  reloading. **Open — closed in Phase 5c.**
- H5: how long operation identifiers are remembered. **Open — closed in Phase 5d.**
- H6: whether cancelling a scope cancels pipelined calls already in flight on other nodes.
  **Open — closed in Phase 5a/5b.**
- The hash algorithms (value hash and content hash). **Open — closed in Phase 2** (H10).
- Transferring persistent structures by node hash instead of by value. **Open — closed in
  Phase 5c.**
- Automatic task placement. **Open — closed in Phase 5**; Phase 0 has explicit `spawn-on` only.

## Used by programs

11 (`spawn-on`, `:node-unreachable`, recomputation), 12 (`update-once`, `:transport`, a stream
across nodes).
