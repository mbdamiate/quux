# Contract: multi-node simulator

Decisions: D-45, D-20 (serialization classes), D-21 (scope identity), D-22 (explicit remote
calls). Harness conventions for `:nodes` and `:network` are in the
[reference-program contract](../../001-language-spec/contracts/reference-program.md).

## Nodes

- With `:nodes` in `conditions.qx`, the harness creates one simulated node per entry, in order,
  and runs `main` on the first. Without `:nodes`, there is one implicit node and `spawn-on` is
  not available (`(node name)` raises `:node-unreachable`).
- Each node has its own root scope, agents and code view; tasks run on the shared blocking
  executor but carry their node in `context->current_node`.

## Messages

Every interaction that crosses nodes is one message:

| Message kind | Sent when | Reply |
|---|---|---|
| `:spawn-request` | `spawn-on` | `:spawn-result` (value or error) |
| `:agent-update` | `update`, `transact`, `update-once` on a remote agent | `:acknowledgement` (new state or error) |
| `:stream-pull` | advancing a remote stream | `:stream-cell` |
| `:cancel` | cancelling a scope whose proxy scope is on another node | none |
| `:state-request` | `state` of a remote agent or future | `:state-reply` |

- The payload is encoded with the canonical encoding; decoding happens on delivery. A local-only
  value (or a closure capturing one) fails **at the sender** with `:not-serializable`, with
  `:data` naming the value kind and its path.
- Futures, agents, scopes and nodes travel as remote references: owner node + identifier.
- A remote expression that raises arrives as `:remote`, with the original error in `:cause`.

## Network

- One network thread delivers messages in order of `delivery_time` = send time +
  `:latency-milliseconds` (default 0); ties keep send order.
- `:drop` rules match on `:from`, `:to`, `:message` and `:occurrence` (1-based count of matching
  messages between the two nodes). A dropped message makes the sender's pending future fail with
  `:transport` immediately (W9, simulation only).
- `:message :acknowledgement` matches acknowledgement messages only.

## Crashes

- `:crash-after {:completed n}`: when the node's n-th `spawn-on` task completes, its
  `:spawn-result` is sent, and then the node is marked crashed.
- After a crash: messages to or from the node are discarded; every future on another node that
  waits for a reply from it fails with `:node-unreachable` at once; the crashed node's tasks are
  cancelled silently (their results never leave the node).

## Code

- Closures travel as function hash + captured values. The receiving node resolves the hash in a
  process-wide code store, and counts each resolution as a "fetch" in the report. The fetch
  protocol itself is Phase 5c (H4).

## Known differences from a real network (for Phase 5a/5b)

- Crashes are detected immediately; real networks need heartbeats and leases.
- Drops fail the sender immediately; real networks need deadlines.
- Delivery order between two nodes is FIFO; real transports may reorder across connections.
