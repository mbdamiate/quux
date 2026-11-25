# Agents and futures vs. actors — side-by-side comparison

Decision D-24 keeps actors outside the core. This document compares the agent/future version
(`main.qx`) of programs 04, 10 and 12 with a version written against a **hypothetical** actor
library (`actors.qx`). The library is described at the top of
[04-bank-account-agent/actors.qx](./04-bank-account-agent/actors.qx): `spawn-actor`,
`receive`, `send-message`, `reply`, `link` and `supervise`. Like everything else, actors there
are children of the current scope.

The rows are the same for every pair. Line counts exclude comments and blank lines.

## Program 04 — bank account

| Aspect | Agents and futures (`main.qx`) | Actors (`actors.qx`) |
|---|---|---|
| Lines of code | 17 | 29 |
| Where atomicity lives | In the update function given to `transact`: the agent runs one update at a time | In the actor's loop: one message at a time; the check and the debit are in the same branch |
| How the caller observes a failure | The future of the send fails with the update's error; the state is unchanged and the agent keeps working | The future of `send-message` is never filled if the actor dies; the caller needs `link`, a timeout, or a supervisor to notice |
| What supervision with restart adds | Nothing needed: a failed update cannot corrupt or stop the agent | Restarting the actor after a crash, but with its **initial** state unless the program persists it |
| What is lost without actors | Nothing for this program | — |
| Other observations | The agent needs no shutdown | The actor is a child of the scope and loops forever, so it needs an explicit `:stop` message, or the scope would wait for it |

## Program 10 — chat server

Line counts cover the server only. The clients and the helpers that write to connections (12
lines) are shared: the actor version imports them.

| Aspect | Agents and futures (`main.qx`) | Actors (`actors.qx`) |
|---|---|---|
| Lines of code | 41 | 46 |
| Where atomicity lives | In the room's update functions (`register`, `deliver`): the commit order is the delivery order | In the room actor's loop: the mailbox order is the delivery order |
| How the caller observes a failure | A failing room update fails only that message's future; the room keeps its state | If the room actor crashes, `link` stops every connection actor; with `supervise`, the room restarts **empty** and the clients must reconnect |
| What supervision with restart adds | Restarting a failing component, but an agent update never kills the agent, so there is little to restart | Automatic restart of the room (at most 3 times) instead of failing the whole server scope |
| What is lost without actors | Supervision trees as a ready-made pattern; the program must decide what to do when a scope fails | — |
| Other observations | A disconnection is observed as the connection scope ending in `:cancelled` | A disconnection is the connection actor returning normally; the outcome is `:disconnected` |

## Program 12 — remote agent

Only part 1 differs. The stream between nodes (part 2) is the same in both versions.

| Aspect | Agents and futures (`main.qx`) | Actors (`actors.qx`) |
|---|---|---|
| Lines of code | 31 | 34 |
| Where atomicity lives | In `update-once`: the runtime applies each operation identifier at most once | In the actor's loop, which keeps the set of applied identifiers in its own state |
| How the caller observes a failure | The send fails with `:transport`; the program resends with the same identifier | Same: the future of `send-message` fails with `:transport`, and the program resends |
| What supervision with restart adds | — | A restarted actor loses the set of applied identifiers, so a resend after a restart is applied **twice**, unless the set is persisted |
| What is lost without actors | Nothing for this program | — |
| Other observations | The agent outlives the `spawn-on` call that created it, because agents are outside the scope tree (D-15) | **Does not work as written:** the actor is a child of the `spawn-on` proxy scope, which must join it before the call returns, and the actor never ends |

## Conclusions for the Phase 4 evaluation and Phase 5f

1. **No reference program needs actors.** Every behavior shown by the actor versions is already
   available with agents, futures and scopes, in the same number of lines or fewer.
2. **Actors and structured scopes conflict.** A long-lived actor cannot be a child of a
   short-lived scope (program 12). An actor layer would need the same exception that agents
   have (life outside the scope tree), or a long-lived scope owned by each node. This must be
   decided before Phase 5f.
3. **Supervision with restart is the one real gain**, and it is only useful when the restarted
   state can be rebuilt. Programs 10 and 12 show that a naive restart loses state: room
   membership in 10, applied identifiers in 12. In 12 this is worse than losing data: a lost
   identifier set breaks idempotence, which Phase 5d depends on.
4. **What would justify the layer:** a reference program, or a real use case, with a component
   that fails for transient reasons, can rebuild its state, and must be restarted without
   failing its parent scope. None of the 12 programs has one.

These conclusions feed the actor evaluation of Phase 4 and the decision on Phase 5f (D-24). They
are recorded in [docs/spec/open-questions.md](../docs/spec/open-questions.md) as W8.
