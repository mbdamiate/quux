# 10 — Chat server

**What this tests in the design:**

- **One scope per connection.** When client 3 disconnects abruptly, its reader cancels its own
  connection scope. The outcome `:cancelled` is recorded. The other connections keep working.
- **Room state in an agent.** Every message is delivered inside a room update. Updates run one
  at a time, so every client sees the same total order: the agent's commit order. Writing to the
  closed connection of client 3 is skipped and does not fail the update.
- **Shutdown by structure.** `first-success` returns when the clients are done and cancels the
  server. Cancelling the server cancels every connection scope below it, and the listener is
  closed when its scope ends.

The clients run in the same process and use real connections (`connect`). The injected waits
`:others-send` and `:settle` only order the start and the end of the run.

**Specification sections:** 07 scopes and cancellation, 08 combinators (`first-success`,
`all`), 09 agents, appendix input and output.

**Decisions:** D-08, D-09, D-12, D-15, D-16.

**Hypotheses informed:** H11 (no stream needs to cross scopes here: each connection is read by
its own task). An actor version is in `actors.qx` (User Story 4).
