# 12 — Errors

**Question:** What is an error, and which kinds exist?

**Status:** Normative

## Answer

**Decision:** D-13 (error model), D-11 (suppressed errors), D-14 (`:deadlock`)

### Shape

1. An error is an immutable value (`Error` in
   [data-model.md](../../specs/001-language-spec/data-model.md)) with these keys:

   | Key | Content |
   |---|---|
   | `:kind` | a keyword from the table below (`Error.kind`) |
   | `:message` | a human-readable string |
   | `:data` | a map with details specific to the kind |
   | `:origin` | a map with `:node` and `:scope`, where the error was raised (`Error.origin`) |
   | `:cause` | the error that caused this one, or `nothing` (`Error.cause`) |
   | `:suppressed` | a vector of errors that happened while this one propagated (`Error.suppressed`) |

2. Errors are ordinary data: they can be compared with `equal?`, stored, and sent to other nodes
   (section 13).

### Forms

| Form | Meaning |
|---|---|
| `(error kind message data)` | creates an error value without raising it |
| `(raise error)` | raises an error value |
| `(try body … (catch kind-or-predicate name handler …) … (finally cleanup …))` | runs `body`; a `catch` clause matches an error whose `:kind` equals the keyword, or for which the predicate returns true; `finally` always runs |
| `(error-kind error)`, `(error-data error)` | read `:kind` and `:data` |

3. `catch` clauses are tried in order. An error that no clause matches continues to propagate
   after `finally` runs.
4. `wait-for` on a failed future re-raises its error unchanged, so the `:origin` and `:cause`
   chain are preserved.
5. **No retries are implicit.** No part of the runtime retries an operation unless the program
   asks for it. A network error is an ordinary failed state of a future.

### Kinds

| Group | Kind | Raised when | Introduced in |
|---|---|---|---|
| Program | `:user` | the program raises its own error | Phase 1 |
| | `:wrong-argument-count` | wrong number of arguments | Phase 1 |
| | `:wrong-type` | a value of the wrong kind | Phase 1 |
| | `:syntax` | the reader cannot read the text (unbalanced brackets, a reader shorthand, a malformed literal, text that ends inside a form) | Phase 1 |
| | `:undefined-name` | a symbol without a binding | Phase 1 |
| | `:index-out-of-range` | `element-at` outside the vector | Phase 1 |
| | `:resource-closed` | reading from or writing to a closed file or connection | Phase 1 |
| | `:stack-exhausted` | non-tail calls nested deeper than the implementation's limit (section 03) | Phase 1 |
| | `:resource-exhausted` | a thread or memory allocation failed | Phase 1 |
| Numerics | `:overflow` | integer result outside 64 bits | Phase 1 |
| | `:inexact-division` | `/` on integers without an exact quotient | Phase 1 |
| | `:division-by-zero` | integer division by zero | Phase 1 |
| Concurrency | `:cancelled` | a cancelled task reaches a cancellation point | Phase 1 |
| | `:timeout` | `with-timeout` ran out of time | Phase 1 |
| | `:deadlock` | `wait-for` would close a cycle; `:data` has `:cycle` | Phase 1 |
| | `:all-failed` | every alternative of `first-success` failed; `:data` has `:errors` | Phase 1 |
| Distribution | `:node-unreachable` | the node that owns a future, agent or scope cannot be reached | Phase 1 (simulated), Phase 5 |
| | `:transport` | a message could not be delivered or was corrupted | Phase 1 (simulated), Phase 5 |
| | `:remote` | an error raised on another node; its `:cause` keeps the original kind | Phase 1 (simulated), Phase 5 |
| | `:not-serializable` | a value that cannot leave its node was sent; raised at the sender | Phase 1 |

The full semantics of the distribution kinds are in section 13. Phase 1 produces them on
simulated nodes in one process; Phase 5 produces them on real nodes.

## Examples

```quux
(define problem (error :user "insufficient funds" {:balance 10 :requested 50}))
(error-kind problem)                          ;=> :user
(get (error-data problem) :requested)         ;=> 50

(try
  (raise problem)
  (catch :user caught (get (error-data caught) :balance))
  (finally (print-line "audited")))
;=> 10   ; output: audited

(try
  (quotient 1 0)
  (catch (function [candidate] (equal? (error-kind candidate) :division-by-zero)) caught
    :handled))
;=> :handled

;; a predicate that is always true catches every error
(try
  (element-at [] 0)
  (catch (function [candidate] true) caught (error-kind caught)))
;=> :index-out-of-range
```

<!-- example-requires: concurrency -->
```quux
;; several failing children: one primary, the others suppressed. Neither child reaches a
;; cancellation point, so the scope's cancellation cannot stop the second failure.
(try
  (with-scope [scope]
    (future (raise (error :user "first" {})))
    (future (raise (error :timeout "second" {})))
    :unreachable)
  (catch (function [candidate] true) caught
    [(error-kind caught) (count (get caught :suppressed))]))
;=> [:user 1] or [:timeout 1]   ; the two failures race (section 07, rule 9)
```

### Distribution kinds

<!-- example-requires: simulated-nodes -->
```quux
;; a node that crashed: its futures fail with :node-unreachable
(try
  (with-scope [scope]
    (wait-for (spawn-on (node :b) (* 6 7))))     ; node :b has crashed
  (catch :node-unreachable problem (error-kind problem)))
;=> :node-unreachable

;; an error raised on the remote node arrives as :remote, keeping the original kind
(try
  (with-scope [scope]
    (wait-for (spawn-on (node :b) (quotient 1 0))))
  (catch :remote problem (error-kind (get problem :cause))))
;=> :division-by-zero

;; a lost message: the program decides whether to retry (no implicit retries)
(define-function send-with-retry [account identifier amount]
  (try
    (with-scope [attempt] (wait-for (update-once account identifier + amount)))
    (catch :transport problem (send-with-retry account identifier amount))))

;; a local resource cannot leave its node
(with-scope [scope]
  (let [file (open-file "lines.txt")]
    (wait-for (spawn-on (node :b) (read-line file)))))
;!! :not-serializable   ; raised at the sender, before anything is sent
```

The recovery from `:node-unreachable` with a nested scope is shown in reference program 11: a
failed child always fails its scope (section 07), so each remote attempt runs in its own scope,
and the error is caught outside that scope.

## Errors

This section defines the error kinds; it raises none of its own besides `:wrong-type` when
`raise` receives something that is not an error.

## Open points

- Stack traces and source positions in errors. **Open — closed in Phase 7** (tooling).
- How `:origin` names a real node. **Open — closed in Phase 5a.**

## Used by programs

03 (propagation), 04 (`:user` for refused withdrawals is a reply, not an error), 08, 09
(`:deadlock`), 11 (`:node-unreachable`), 12 (`:transport`).
