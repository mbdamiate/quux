# 09 — Agents

**Question:** How does state that changes over time work?

**Status:** Normative

## Answer

**Decision:** D-15 (agent semantics), D-16 (scope of a send; cancellation of a pending send;
resolves H3)

### Creating and reading

1. `(agent initial-value)` creates an agent: a reference with identity to an immutable value.
2. `(state agent)` returns the last committed value (`Agent.state` in
   [data-model.md](../../specs/001-language-spec/data-model.md)). It never waits and never
   blocks.
3. Agents are **not owned by any scope**. An agent lives while it is reachable and can be used
   from any scope.

### Updating

4. `(update agent function argument …)` enqueues a call `(function current-state argument …)`.
   The returned value becomes the new state. `update` returns a future of the new state.
5. `(transact agent function argument …)` enqueues a call whose function returns a vector
   `[new-state reply]`. The state becomes `new-state`, and `transact` returns a future of
   `reply`. This is the form for "test and act" in one step.
6. Updates run **one at a time**. Updates sent by the same task run in the order they were sent
   (`Agent.queue`). There is no defined order between updates sent by different tasks.
7. An update function that raises leaves the state unchanged and fails only the future of that
   send. The agent keeps accepting and running later updates.
8. An update function may `update` or `transact` other agents or the same agent. These are
   enqueued, never run inline. An update function may `wait-for` a future, but the agent runs
   nothing else while it waits. Waiting for the future of a send to the same agent is a
   deadlock and raises `:deadlock` (section 07).
9. Update functions run in a scope that belongs to the agent, not to the sender.

### The future of a send and cancellation (D-16)

10. The future returned by `update` or `transact` belongs to the **sender's current scope**, so
    it does not outlive that scope.
11. If the sender's scope is cancelled:
    - when the update is still queued, it is withdrawn and never applied;
    - when the update has already started, it runs to completion and commits, so there is
      never a partial effect.

    In both cases the sender's future ends `:failed` with `:cancelled`. A sender that must know
    whether the update was applied reads `(state agent)`. From Phase 5d on, operation
    identifiers make this check exact (H5).

## Examples

```quux
(define counter (agent 0))
(with-scope [scope]
  (wait-for (update counter increment)))          ;=> 1
(state counter)                                   ;=> 1

;; test and act in one step (reference program 04)
(define-function withdraw [balance amount]
  (if (>= balance amount)
    [(- balance amount) :accepted]
    [balance :insufficient-funds]))

(define account (agent 100))
(with-scope [scope]
  (wait-for (transact account withdraw 30)))      ;=> :accepted
(with-scope [scope]
  (wait-for (transact account withdraw 500)))     ;=> :insufficient-funds
(state account)                                   ;=> 70

;; a failing update does not change the state and does not stop the agent
(with-scope [scope]
  (wait-for (update account (function [balance] (raise (error :user "rejected" {}))))))
;!! :user
(state account)                                   ;=> 70
(with-scope [scope]
  (wait-for (update account + 5)))                ;=> 75
```

## Errors

| Kind | When |
|---|---|
| any kind | the update function raised; only that send's future fails |
| `:cancelled` | the sender's scope was cancelled (see rule 11) |
| `:deadlock` | an update waits for the future of a send to the same agent |
| `:wrong-type` | `transact`'s function does not return a two-element vector |

## Open points

- H5: how long a remote agent's owner keeps the identifiers of applied operations.
  **Open — closed in Phase 5d.**
- Remote agents with a single owner and defined delivery semantics. **Open — closed in
  Phase 5d.**
- An optional actor layer (agent + mailbox + address). **Open — evaluated in Phase 4**, decided
  in Phase 5f (D-24).

## Used by programs

04 (bank account), 09 (holding futures), 10 (room state), 12 (remote agent).
