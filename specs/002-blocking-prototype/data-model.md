# Data Model: quux Blocking Prototype (Phase 1)

The quux-level entities (Value, Future, Scope, Agent, Stream cell, Error, Function / Closure,
Node) are defined in [001 data-model.md](../001-language-spec/data-model.md). This document adds
the **runtime records** that the prototype implements, with their fields, invariants and state
transitions. Field names are the C field names (full words, constitution VIII). Each record names
the decision that defines it.

## Context (`quux_context`) — D-33

One per running task. Passed explicitly to every runtime function.

| Field | Meaning | Rules |
|---|---|---|
| `task` | the task that owns this context | never null while running |
| `root_frames` | top of the shadow stack of root frames | balanced: every frame opened is closed in the same function |
| `current_scope` | the scope used by `future` | follows section 06 rule 3 |
| `current_namespace` | the namespace of the code being run | — |
| `current_node` | the simulated node the task runs on | the first node when there are no `:nodes` |
| `pending_error` | the error value after a failed call | set only when a function returns `false` |
| `recursion_depth` | non-tail evaluation depth | above the limit → `:stack-exhausted` (D-51) |
| `task_function`, `task_arguments`, `task_future` | a spawned task's work and the future it fills (added in US3) | roots of the task, registered by its creator before it starts; `nothing` for the session's task |

## Heap object header — D-35

| Field | Meaning | Rules |
|---|---|---|
| `kind` | value kind (list cell, vector node, champ node, string, symbol, keyword, boxed integer, float, error, closure, future, agent, scope, stream cell, node, resource) | immutable |
| `flags` | `local_resource`, `has_identity`, `verifier_mark` | `local_resource` set only on files, listeners, connections |
| `value_hash` | cached 64-bit value hash | computed once, never changes (D-36, D-37) |
| `content_hash` | pointer to a lazily computed 256-bit content hash | null until requested |

Identity kinds (future, agent, scope, node, resource) hash and compare by identity (D-02).

## Root frame (`quux_root_frame`) — D-34

| Field | Meaning |
|---|---|
| `previous` | the enclosing frame |
| `count` | number of registered slots |
| `slots` | addresses of `quux_value` local variables |

Invariant: while a frame is open, every heap value that its function will use after a possible
allocation is reachable from a slot, a global root, or an object reachable from those. The
verifier checks this invariant in single-task runs.

## Task (`quux_task`) — D-40

| Field | Meaning | Rules |
|---|---|---|
| `thread` | the OS thread (blocking executor only) | hidden behind the executor interface |
| `mutex`, `condition` | park and wake | lock order: global wait-for lock before any task lock |
| `cancellation_requested` | atomic flag | set by `cancel!` or the scope; read at every cancellation point |
| `shield_depth` | nesting of `shield-from-cancellation` | while > 0, cancellation is recorded but not raised |
| `waiting_for` | the future this task is parked on, or null | used by deadlock detection |
| `wake_up_pipe` | two file descriptors | written by `cancel!` to interrupt blocked input/output (D-44) |
| `future` | the future this task computes | — |

## Future (runtime record) — sections 06–07, D-07

| Field | Meaning |
|---|---|
| `state` | `:empty`, `:filled`, `:failed` (atomic; one transition only) |
| `value` / `error` | the result, written through the write barrier |
| `scope` | owning scope |
| `computing_task` | the task computing it (for the wait-for graph) |
| `waiters` | tasks parked on it |
| `tag_names` | harness tags evaluated inside it (for `future-state`, `future-error`). US3 keeps them in the harness instead (`Program run.tag_events`: tag → future) |
| `remote` | for a remote reference: node and identifier, else null |

State transitions: `:empty → :filled` or `:empty → :failed`; the first transition wins (compare
and swap); waiters are woken after the transition.

## Scope (runtime record) — section 07, D-08, D-11, D-21

| Field | Meaning |
|---|---|
| `identifier` | 128 random bits (`getentropy`, D-48) |
| `state` | `:running → :cancelling → :joined-successfully` or `:joined-failed` |
| `parent` | enclosing scope (null for a node's root scope) |
| `children` | futures and child scopes not yet joined |
| `primary_error`, `suppressed` | first failure in completion order and the later ones (D-11, D-49) |
| `owned_streams` | producers to cancel when the body returns (D-17, D-42) |
| `owner_node` | the simulated node (D-45) |

## Agent (runtime record) — section 09, D-41

| Field | Meaning |
|---|---|
| `state` | the committed value |
| `queue` | FIFO of pending updates: function, arguments, sender future, sender scope, operation identifier |
| `worker_running` | whether a worker thread is draining the queue |
| `applied_operations` | map: operation identifier → resulting state (`update-once`) |
| `agent_scope` | the agent's own scope (section 06 rule 3) |
| `owner_node` | the node that owns it |

Invariant: at most one update runs at a time; a failed update leaves `state` unchanged.

## Stream cell and producer — section 10, D-42

| Field | Meaning |
|---|---|
| `first` | the value |
| `rest` | a future of the next cell |
| `producer` | the producing task and its owner scope |
| `produced_count`, `demanded_count` | for the `:maximum-ahead` bound and the `maximum-ahead` assertion |
| `foreign_advances` | advances from outside the owner scope's subtree (H11 evidence) |

## Simulated node — D-45

| Field | Meaning |
|---|---|
| `name` | keyword from `conditions.qx` |
| `root_scope` | node-owned scope |
| `crashed` | set at `:crash-after`; after that, messages to or from it are discarded |
| `completed_spawn_count` | counts completed `spawn-on` tasks |
| `crash_after_completed` | from conditions, or none |
| `remote_table` | identifier → local future, agent, scope (for remote references) |

## Network message — D-45, contract [simulator.md](./contracts/simulator.md)

| Field | Meaning |
|---|---|
| `from`, `to` | node names |
| `message_kind` | `:spawn-request`, `:spawn-result`, `:agent-update`, `:acknowledgement`, `:stream-pull`, `:stream-cell`, `:cancel`, `:state-request`, `:state-reply` |
| `payload` | bytes in the canonical encoding |
| `delivery_time` | send time + latency |
| `correlation` | identifier of the future waiting for the reply |

## Program run — D-46, contract [harness-report.md](./contracts/harness-report.md)

| Field | Meaning |
|---|---|
| `program` | directory name |
| `conditions` | parsed `conditions.qx` (unknown keys rejected) |
| `assertions` | parsed `expected.qx` (unknown forms rejected) |
| `outcome` | returned value, or raised error |
| `tag_events` | per tag: futures created, final states, errors |
| `agent_commits` | per tagged agent: committed states in order |
| `stream_ahead` | per tagged stream: maximum produced-but-undemanded |
| `printed_lines` | output of `print-line` |
| `elapsed_milliseconds` | wall clock of `main` |
| `primary_error_raced` | whether the primary error was chosen by a race (H9 evidence) |
| `applied_operations` | operation identifier → number of applications |

## Toolchain record — FR-004

One row per adjustment in `docs/toolchain-record.md`: target, preset, tool, adjustment, reason,
evidence (log excerpt or link), date.
