# 06 — Futures

**Question:** What is a future, which states can it be in, and what does waiting for it do?

**Status:** Normative

## Answer

**Decision:** D-07 (states), D-10 (futures that leave their scope), D-12 (`future`, `wait-for`)

### Creating a future

1. `(future expression …)` starts evaluating the expressions concurrently and immediately
   returns a future. The future belongs to the **current scope** (section 07). There is no
   way to create a future outside a scope.
2. `(spawn scope expression …)` does the same in an explicit scope, which MUST be the current
   scope or one of its live ancestors.

### States

3. A future is in exactly one of three states (the field `Future.state` of
   [data-model.md](../../specs/001-language-spec/data-model.md)):

   | State | Meaning |
   |---|---|
   | `:empty` | not finished yet |
   | `:filled` | finished with a value |
   | `:failed` | finished with an error value (section 12) |

4. A future leaves `:empty` at most once, and the new state is final. When two transitions
   race (for example a value arrives at the same moment as a cancellation), the first one wins
   and the other is discarded.
5. **Cancellation is not a fourth state.** A cancelled future is `:failed` with an error of kind
   `:cancelled`. The same holds for `:timeout`, `:deadlock`, and, from Phase 5 on,
   `:node-unreachable`.
6. `(state future)` returns the current state and never waits.

### Waiting

7. `(wait-for future)` returns the value of a `:filled` future, raises the error of a `:failed`
   future, and suspends the current task while the future is `:empty`. Suspension is handled by
   the runtime: the function that calls `wait-for` needs no annotation, and neither do its
   callers.
8. `wait-for` is a cancellation point (section 07): if the waiting task is cancelled while it
   waits, `wait-for` raises `:cancelled`.
9. If waiting would close a cycle of tasks that wait for each other, `wait-for` raises
   `:deadlock` instead of suspending (section 07, D-14).

### Ownership and leaving the scope

10. Every future belongs to exactly one scope (`Future.scope`), and that scope stays alive while
    the future is `:empty`.
11. A future MAY be returned from its scope or stored in a data structure. Because a scope
    always waits for all its children before it ends, such a future is always `:filled` or
    `:failed`: `wait-for` on it never suspends. There is never an `:empty` future without a live
    scope.
12. Futures are compared and hashed by identity (section 01).

## Examples

```quux
(with-scope [scope]
  (let [answer (future (* 6 7))]
    (wait-for answer)))                     ;=> 42

(with-scope [scope]
  (let [broken (future (raise (error :user "no data" {})))]
    (wait-for broken)))
;!! :user

;; catching inside the scope does not stop the scope from failing: a failed child
;; always fails its scope (section 07), so the error is caught outside
(try
  (with-scope [scope]
    (future (raise (error :user "no data" {})))
    :unreachable)
  (catch :user problem (error-kind problem)))
;=> :user

;; a future that leaves its scope is always settled
(define result
  (with-scope [scope]
    (future (sleep 10) :done)))
(state result)                              ;=> :filled
(wait-for result)                           ;=> :done   ; returns immediately
```

## Errors

| Kind | When |
|---|---|
| any kind | `wait-for` re-raises the error of a `:failed` future, keeping its `:cause` chain |
| `:cancelled` | the waiting task is cancelled while it waits |
| `:deadlock` | waiting would close a cycle |

## Open points

- A remote future (Phase 5a) has the same three states and the same `wait-for`, plus the kinds
  `:node-unreachable`, `:transport` and `:remote`. **Open — closed in Phase 5a.**

## Used by programs

01, 02, 03, 08, 09 (every concurrent program), and 11 (remote futures).
