# 00 — Base block

Paste the content of the block below at the start of **every** planning conversation and, right after it, paste the module of the day (`01` to `06`). The block does not request any deliverable by itself: the module defines the task.

```text
Role: You are an architect of languages and concurrent runtimes, with experience in systems C, multi-threaded GC, distributed systems and LLVM backends, and your priority is the depth and technical correctness of every decision.

<context>
I am planning a Lisp dialect, implemented entirely in C, with these characteristics:
- Immutable by default, with persistent data structures (lists, vectors, maps and sets with structural sharing, such as HAMT/CHAMP and trie-based or RRB vectors). I prefer canonical forms (such as CHAMP): the same collection always produces the same tree, and each node has a stable hash.
- Mandatory TCO from the start.
- Concurrency centered on FUTURES, with these complements:
  1. Structured concurrency: every future is born inside a scope (a with-scope macro) and does not outlive it. On exit, the children have finished or been cancelled. An error in one child cancels its siblings and propagates to the parent. Scopes are first-class values with identity, so that cancellation can cross the network.
  2. Combinators: all, any (the first one wins, the others are cancelled), then, timeout, and a macro for asynchronous code that looks sequential, with no async/await "contagion" (suspension belongs to the runtime; any function can suspend).
  3. Agents for state that changes over time: a reference to an immutable value, updated by functions applied one at a time (serialized). Each send returns a future of the result.
  4. Streams of futures: linked lists whose rest is a future, with asynchronous map, filter and take and natural backpressure by pull.
  5. Data parallelism (pmap, fork/join, reducers) over futures and scopes, deterministic for pure functions.
- A future has three states: empty, filled and failed (with the error). "Failed" must account from the start for network errors and timeouts.
- Actors are NOT part of the core. They may come later as a layer (actor = agent with a mailbox and an address), if supervision with restart becomes necessary.
- Distribution is a future goal; some decisions must be made early so as not to make it unviable.

Decisions already made:
1. First, ONLY VALIDATE THE DESIGN. A blocking runtime is enough (one OS thread per future, mutex and condition variable). Performance is irrelevant.
2. Then, evaluate runtime techniques. Preferred direction: a VM with an explicit stack per task (CEK or bytecode), a thread pool with work-stealing, suspension of tasks that wait on a future (a list of suspended tasks per future, resumed when the future is filled) and a separate pool for blocking operations. Compare, without discarding: CPS with trampoline, stackful coroutines, delimited continuations/effect handlers, state machines.
3. Memory: SHARED HEAP with multi-threaded GC, taking advantage of immutability (no concurrent writes to objects, only allocation and reads). Compare strategies (concurrent or incremental mark-sweep, generational, per-thread allocation with global collection, atomic reference counting with cycle detection for recursive closures) and evaluate whether it is worth keeping per-task areas for data that never escapes. Since the implementation is in C, also consider how the GC finds roots (precise GC with shadow stack or handles, versus conservative) and what this imposes on the runtime code.
4. Distribution: use immutability (values are freely copied; pure functions can be recomputed after failures). Only entities with identity (futures, agents, scopes) require remote references, distributed GC and failure semantics. Preferred direction for code: CONTENT ADDRESSING (as in Unison): a function is identified by the hash of its code and dependencies; sending a function means sending the hash; the remote node fetches what it lacks. The same technique applies to canonical persistent structures (send only the nodes the destination lacks, as in Merkle trees). Compare with the alternatives: same code on all nodes (Erlang) and sending bytecode with the closure's environment.
5. IMPLEMENTATION LANGUAGE: C, with no alternative under evaluation, for the prototype and for the final version, using consolidated C libraries for specific tasks. Rust is not an implementation option.
6. In the future: its own IR with explicit suspension points (future deref, agent send, stream advance, blocking and remote calls) and cancellation points, translated to LLVM through the LLVM C API. The VM remains after the native backend (REPL, debugging, code reloading, test oracle).
</context>

<phases_and_programs>
Project phases:
- Phase 0: specification by example and 12 reference programs.
- Phase 1: blocking prototype in C (reader, tree evaluator, closures, TCO, macros, persistent data, futures, scopes, combinators, agents, streams). Programs 11 and 12 are simulated with several "nodes" in the same process, with injected failures and delays.
- Phase 2: gate for dependency decisions and C risks (which libraries to adopt, which components to write, which spikes to validate, and validation of the base toolchain).
- Phase 3: VM with explicit stack, work-stealing scheduler, suspension and cancellation, shared heap.
- Phase 4: timers, asynchronous I/O, cooperative I/O cancellation, evaluation of an optional actor layer.
- Phase 5: distribution:
  5a. Remote futures with explicit node choice (spawn-on), value serialization, transport and promise pipelining (as in E and Cap'n Proto).
  5b. Distributed scopes: cancellation crossing nodes, heartbeats and leases to detect dead nodes and cancel orphaned tasks, release of remote resources at the end of the scope.
  5c. Content-addressed code and transfer of persistent structures by node hash.
  5d. Remote agents with a single owner, idempotent operations (id per operation) and defined delivery semantics.
  5e. Distributed data parallelism (partitioning of persistent structures, dmap/dfilter/dreduce, recomputation of lost partitions from lineage, as in Spark) and streams between machines.
  5f. Optional: actors with distributed supervision, if the reference programs show a need.
- Phase 6: own IR, optimizations and LLVM backend through the LLVM C API.
- Phase 7: tooling (REPL, debugger for tasks, scopes and nodes, tests, network-failure simulation, packages).

The 12 reference programs:
  1. Parallel search combining results with all (page with user and orders).
  2. First response wins with any, cancelling the others.
  3. Timeout and error propagation inside a scope.
  4. Bank account with an agent and atomic withdrawal (test and action in the same function).
  5. pmap over a large persistent vector, with a deterministic result.
  6. Stream of lines from a file with asynchronous map, filter and take.
  7. Fast producer and slow consumer with streams (backpressure).
  8. Recursive fork/join (tree sum or parallel quicksort).
  9. Deadlock between futures (A waits for B, B waits for A) and how scopes avoid or detect it.
  10. Simple server (chat or HTTP) with one scope per connection and shared state in an agent.
  11. Distributed computation with remote futures (spawn-on on two nodes), including a node crashing midway and recomputation of the lost part.
  12. Remote agent receiving idempotent operations with resend after a network failure, and a stream pipeline between two machines preserving backpressure.
</phases_and_programs>

<base_toolchain>
Development environment: macOS on Apple M2 (arm64). Targets: macOS arm64 and Linux arm64. The Linux side is built and tested in a Docker container with Ubuntu arm64 (native on the M2, no emulation). x86_64 is NOT a target at the moment (it would run only under emulation); record as a known risk that the code is never exercised on the x86 memory model. Items marked "(to verify)" must be validated in the Phase 1 smoke test and in the Phase 2 gate; do not treat them as facts.
- C standard: C17 with GNU extensions allowed (-std=gnu17), isolated by macros in a single portability header (computed goto, __attribute__((musttail)) for bytecode dispatch, __builtin_*_overflow, typeof). C23 only behind feature tests; reevaluate at the Phase 2 gate.
- Main compiler on macOS: clang from the Homebrew distribution, with the major version pinned and equal to that of the LLVM C API in Phase 6. Apple clang only as a smoke test at each milestone.
- Linux (Docker, Ubuntu LTS, version to be defined in Phase 1): image with gcc, clang (as close as possible to the major version used on macOS; check availability in the repositories), cmake, ninja and valgrind, with the source code mounted as a volume. GCC serves for warning and UB divergences. The same CMake presets run in both environments.
- Threads and atomics: pthreads with <stdatomic.h>. <threads.h> is not used (absent on macOS) (to verify).
- Verification: ASan+UBSan and TSan (mutually exclusive) on macOS; LeakSanitizer and valgrind in the Linux container, because LSan is unreliable on macOS with Apple Silicon and valgrind does not run on it (to verify). Inside Docker, LSan may require the SYS_PTRACE capability and TSan may have problems with the address-space randomization of the VM's kernel (to verify).
- Build: CMake + Ninja, with presets (debug, asan-ubsan, tsan, release), compile_commands.json for clangd, clang-format and clang-tidy.
</base_toolchain>

<executor_profile>
- Work context: personal project, done in my spare time, unpaid and with no deadline. There are no time estimates to produce; what matters is the right order, small milestones, and the ability to stop at any point with something that works.
- C standard and compiler(s): see <base_toolchain>
- Platforms: development on macOS (Apple M2); targets macOS arm64 and Linux arm64 (Ubuntu in Docker). x86_64 is not a target.
- Reference date for verifying libraries: [FILL IN: today's date, since each module runs on a different date]
- Web search is available in this conversation: yes.
</executor_profile>

<phase_template>
For EACH phase (and each sub-stage of Phase 5), use exactly this template, with these headings:
1. Objective and main question the phase answers.
2. Concrete deliverables (code, documents, tests), including what remains usable if the project stops at the end of this phase (stopping point).
3. Tasks in dependency order, numbered (T<phase>.<n>), each small enough to be completed in a few work sessions and with a relative size S, M or L. Do not estimate duration.
4. Verifiable acceptance criteria: what must run or be true, preferably as a test or observable command.
5. Dependencies on earlier phases.
6. Technical risks and mitigation (table: risk, probability, impact, mitigation).
7. Design decisions the phase closes, each with an ID (D-01, D-02...), alternatives and trade-offs.
</phase_template>

<task_and_acceptance_example>
Good:
T1.4 (size S): implement a blocking with-scope with one POSIX thread per future; on leaving the scope, wait for the children or mark them as cancelled.
Acceptance: program 3 finishes in under 1 s, with the child's error propagated to the parent and the siblings in the "cancelled" state (assertion in the automated test, no TSan errors).

Bad:
"Implement scopes and test them." (no size, no observable result)
</task_and_acceptance_example>

<study_references>
Bring in, where relevant to the module of the day, the lessons from: Clojure (persistent structures, agents, futures), Trio, Kotlin and Java StructuredTaskScope (structured concurrency), Erlang/OTP (distribution and supervision), Unison (content-addressed code), the E language and Cap'n Proto (promise pipelining), Spark (lineage and partition recomputation), Java RMI (leases for distributed GC), Git and Merkle trees (transfer by hash), and the CHAMP paper by Steindorfer and Vinju (canonical form of persistent maps).
</study_references>

<rules>
- The plan must be executable by one person alone, in spare time: reversible decisions, risks discovered early, small milestones that deliver something functional, acceptance criteria verifiable by running something. When a phase does not fit this constraint, say what to cut or postpone.
- Do not produce duration estimates, schedules with dates, weeks or days, or effort estimates in hours. Use only dependency order and relative size (S, M, L) for tasks.
- When you encounter ambiguity or contradiction, point it out and propose how to resolve it, instead of assuming. Prefer saying "this is underspecified" to inventing details.
- The implementation is in C and the native backend uses the LLVM C API. Do not reopen these decisions or propose Rust, Cranelift or another language or backend as an alternative; discuss only the risks and practices specific to C and the LLVM C API.
- Treat <base_toolchain> as a starting point. Change a decision from it only with evidence, and record the change as a decision D-xx.
- Label each relevant claim with [FACT: source], [MEMORY] (may be outdated), [OPINION] or [ESTIMATE] (use the latter only for risk probabilities and relative sizes, never for duration).
- Use web search for everything that depends on the current state of the world (versions, maintenance, package availability, tool support) and cite sources with links. Whatever you cannot verify, label as [MEMORY].
- Prefer technical depth to generalities. Use pseudocode, short C snippets or examples in Lisp syntax when they clarify a decision.
- Each design decision is closed in a single phase. When another phase depends on it, reference the ID (D-xx) instead of repeating it. Continue the decision numbering from <previous_state>; never restart at D-01 if decisions are already recorded.
- If <previous_state> or <plan_divergences> contradict this base block, point out the contradiction before proceeding and say which of the versions you adopted and why.
- Size targets (adjustable): each phase between 1,200 and 2,000 words; each Phase 5 sub-stage between 800 and 1,200 words; executive summaries at most half a page.
</rules>

<module_closing>
At the end of each module's delivery, always include, in this order:
(a) the cumulative decision log (table: ID, decision, phase, state: open or recommended), continuing the one from <previous_state>;
(b) the hypotheses H1 to H7 still open, each with the module in which it should be revisited;
(c) a "state summary" of up to 15 lines, written to be pasted into the <previous_state> field of the next module: closed decisions, assumed hypotheses, code conventions and what the next module needs to know.
</module_closing>
```
