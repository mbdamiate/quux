# 07 — Scopes and Cancellation

**Question:** How do scopes bound futures, and how does cancellation work?

**Status:** Normative

## Answer

**Decision:** D-08 (scopes), D-09 (cooperative cancellation), D-11 (several failures),
D-14 (deadlock detection), D-21 (scope identity; see section 13)

### Scopes

1. `(with-scope [name] body …)` creates a scope, binds it to `name`, and makes it the **current
   scope** while `body` runs. `future` creates its futures in the current scope (section 06).
2. A program runs inside an implicit **root scope**. Each evaluation in the interactive loop
   runs in a new child of the session's scope.
3. A scope is a value with identity. Its fields are in
   [data-model.md](../../specs/001-language-spec/data-model.md) (`Scope.status`,
   `Scope.primary-error`, `Scope.suppressed`).
4. **Join.** When `body` returns or raises, the scope waits until every child is `:filled` or
   `:failed`. Only then does `with-scope` return or raise. No future outlives its scope.
   Stream producers are the one exception to waiting: when the body returns, the scope
   **cancels** the producers of the streams it owns, because a producer may never finish on
   its own, and then joins them (section 10). Their `:cancelled` errors are dropped and do not
   fail the scope.
5. **Failure.** When a child fails, the scope:
   - moves from `:running` to `:cancelling`;
   - cancels every other child that is still `:empty`, and cancels the body;
   - after the join, ends as `:joined-failed` and raises the primary error.
6. **Success.** When the body returns and every child is `:filled`, the scope ends as
   `:joined-successfully` and `with-scope` returns the value of the body.

   ```text
   :running ──body returns and all children filled──► :joined-successfully
   :running ──a child fails / cancel! / the parent is cancelled──► :cancelling
   :cancelling ──all children settled──► :joined-failed   (raises the primary error)
   ```

### Several failures (D-11)

7. The **primary error** is the first failure that is not a cancellation, in order of
   completion. Every later failure that is not a cancellation, and that happens before the join
   ends, is attached to the primary error under `:suppressed`.
8. Cancellation errors are dropped, and do not fail the scope, when they were caused by:
   - the scope's own cancellation; or
   - a combinator that cancels futures it no longer needs: the losers of `first-success`, the
     inner computation of `with-timeout`, and the remaining futures of `all` after a failure
     (section 08).

   Every other `:cancelled` failure of a child fails the scope like any other error.
9. Because the order of completion between racing failures is not deterministic, tests MUST
   accept any of the racing errors as primary (`raises-one-of`) and may check the others with
   `suppressed-includes` (hypothesis H9).

### Cancellation (D-09)

10. `(cancel! scope)` requests the cancellation of a scope: the scope moves to `:cancelling`
    and cancels all its children and its body. A scope is also cancelled when its parent is.
11. Cancellation is **cooperative**. A task observes it only at a **cancellation point**, where
    it raises `:cancelled`. The cancellation points are:
    - `wait-for` (section 06);
    - agent `update` and `transact` (section 09);
    - advancing a stream, and `emit` while it waits for demand (section 10);
    - `sleep`;
    - input and output operations (appendix);
    - remote calls (`spawn-on`, Phase 5);
    - `(check-cancellation)`, which does nothing unless the current task has been cancelled.
12. Code between cancellation points runs to the next point. A long computation without
    cancellation points SHOULD call `(check-cancellation)` periodically.
13. `finally` blocks run when `:cancelled` is raised through them.
14. `(shield-from-cancellation body …)` runs `body` without observing cancellation, so cleanup
    code can finish (for example, telling another node that a resource is released). When it
    ends, a pending cancellation is observed at the next cancellation point.
15. There is no forced cancellation: a task is never killed and its stack is never unwound
    from outside.

### Deadlock (D-14)

16. The runtime keeps track of which task waits for which future and which task will fill that
    future. When a `wait-for` would close a cycle, every future in the cycle fails with
    `:deadlock`, whose `:data` contains `:cycle` with the identifiers of the futures involved.
    The error then propagates through the scopes like any other failure.
17. Detection is guaranteed within one process, including the simulated nodes of Phase 1. A
    cycle that crosses real nodes is not guaranteed to be detected; such programs use
    `with-timeout` (section 08).
18. An agent update that waits for the future of a send to the same agent forms a cycle and is
    detected (section 09).

## Examples

```quux
;; a failing child cancels its sibling, and the scope raises the child's error
(try
  (with-scope [scope]
    (let [fast-failure (future (sleep 50) (raise (error :user "lookup failed" {})))
          slow-work    (future (sleep 10000) :never)]
      [(wait-for fast-failure) (wait-for slow-work)]))
  (catch :user problem (error-kind problem)))
;=> :user, after about 50 ms; slow-work ends :failed with :cancelled

;; explicit cancellation from inside
(with-scope [scope]
  (future (sleep 10000))
  (cancel! scope)
  :done)
;!! :cancelled

;; a pure loop that stays responsive to cancellation
(define-function count-down [n]
  (check-cancellation)
  (if (zero? n) :finished (count-down (decrement n))))

;; deadlock: A waits for B, B waits for A. An agent holds both futures so that each can
;; find the other; the injected sleep lets the holder be filled first.
(with-scope [scope]
  (let [holder (agent {})
        a (future (sleep 10) (wait-for (get (state holder) :b)))
        b (future (sleep 10) (wait-for (get (state holder) :a)))]
    (wait-for (update holder put :a a))
    (wait-for (update holder put :b b))
    [(wait-for a) (wait-for b)]))
;!! :deadlock   ; both a and b end :failed with :deadlock; no hang
```

Reference program 09 is the normative version of the deadlock example. It removes the reliance
on the injected `sleep`.

## Errors

| Kind | When |
|---|---|
| `:cancelled` | the task is cancelled and reaches a cancellation point |
| `:deadlock` | a `wait-for` would close a cycle |
| any kind | the primary error of a failed scope |

## Open points

- H8: keeping deadlock detection cheap in the work-stealing VM. **Open — closed in Phase 3.**
- H9: whether a deterministic choice of primary error is needed for replay. **Open — closed in
  Phase 1 / Phase 7.**
- Cross-node cancellation (scope identity, leases). **Open — closed in Phase 5b** (D-21).

## Used by programs

02 (cancellation of losers), 03 (error propagation), 08 (failure in a subtask), 09 (deadlock),
10 (one scope per connection), 11 (a node fails inside a scope).
