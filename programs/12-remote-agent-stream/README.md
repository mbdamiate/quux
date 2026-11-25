# 12 — Remote agent and stream between nodes

**What this tests in the design:**

- **Part 1:** an agent created on node `:b` receives ten deposits from `:a`, each with an
  operation identifier. The acknowledgement of operation 7 is lost, so its send fails with
  `:transport`. The program resends it explicitly with the same identifier. `update-once`
  applies it only once, so the final balance equals a run without failures.
- **Part 2:** a stream produced on `:a` is consumed on `:b`. The stream cell travels by value
  and its rest by remote reference, so each advance on `:b` asks `:a` for the next value. The
  producer never runs more than one item ahead, even across the network.

**Specification sections:** 13 serialization and identity (`update-once`, streams across nodes),
09 agents, 10 streams, 12 errors.

**Decisions:** D-15, D-16, D-17, D-20, D-22, D-25.

**Hypotheses informed:** H5 (how long operation identifiers are kept; Phase 5d). An actor
version is in `actors.qx` (User Story 4).
