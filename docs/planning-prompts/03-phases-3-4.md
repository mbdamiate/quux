# 03 — Phases 3 and 4: VM, scheduler, GC, I/O and optional actors

Paste `00-base.md` first and, right after it, the block below. Run this module when Phase 2 is closed (libraries decided and spikes done). The GC and scheduler decisions depend on what the spikes showed.

```text
<input>
<previous_state>
[FILL IN: paste here the decision log, the open hypotheses and the state summary produced by module 02]
</previous_state>

<spike_results>
[FILL IN: what the Phase 2 spikes showed (multi-threaded GC, work-stealing deque, hash and serialization, transport), which libraries were actually adopted and which components you decided to write]
</spike_results>

<plan_divergences>
[FILL IN: what changed relative to the previous plan. Write "none" if nothing changed]
</plan_divergences>
</input>

<module_task>
Turn Phases 3 and 4 into a detailed plan that one person can execute alone, in spare time, starting from the reality recorded in <input>. This module covers only these two phases. Use web search to verify the current state of any technique or library that the strategy comparison cites.
</module_task>

<specific_content>
- Phase 3: format of a task's state; scheduler loop with work-stealing; deref semantics (lock-free fast path for filled futures, suspension otherwise); implementation of scopes and cancellation; GC strategy for the shared heap, comparing the options from the decisions already made (concurrent or incremental mark-sweep, generational, per-thread allocation with global collection, atomic reference counting with cycle detection) and the root convention inherited from Phase 1 (including safepoints and root map in C); evaluation of per-task areas for data that never escapes; handling of blocking native calls and of the separate pool for blocking operations; comparison, without discarding, with CPS with trampoline, stackful coroutines, delimited continuations and state machines, using what the prototype showed; and what to leave prepared so that a future can be filled by a network message (a decision that Phase 5 inherits).
- Phase 4: timers, asynchronous I/O (kqueue on macOS, epoll on Linux, and the poller layer that unites them), cooperative cancellation of I/O operations, integration with the pool of blocking operations. Evaluate when and how to add actors on top of agents and futures, and what this would require from the runtime and the GC; state clearly under which conditions the actor layer is not worth it.
- All decisions that Phase 5 will inherit must be marked in the log as "inherited by: Phase 5".
- Revisit the hypotheses H1 to H7 that <previous_state> left open and that touch these phases.
</specific_content>

<delivery_protocol>
Deliver in a single part, in Markdown: Phase 3 and Phase 4 following the template, and the module closing, per <module_closing>. If the response gets too long, stop after Phase 3 and wait for me to write "continue" before Phase 4.
</delivery_protocol>
```
