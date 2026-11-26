# quux — Spec Kit roadmap

How the original modular planning prompts (`docs/planning-prompts/00…06`) map onto the
Spec Kit workflow, and the ready-to-use input for each feature.

## Concept mapping

| Original prompt set | Spec Kit equivalent |
|---|---|
| `00-base.md` (role, context, decisions made, toolchain, rules) | `.specify/memory/constitution.md` (Principles I–VII + toolchain + workflow) |
| One module per phase, run "when you reach it" | One feature per phase (or per Phase 5 sub-stage), specified only when reached |
| `<previous_state>` copy-paste between modules | `docs/decisions.md` + `docs/hypotheses.md` + earlier `specs/*/research.md` (read from disk) |
| `<plan_divergences>` / `<prototype_results>` / `<spike_results>` | A **"Starting reality"** section at the top of the next feature's input to `/speckit-specify` |
| Phase template §1 Objective / main question | `spec.md` → summary + user stories |
| §2 Deliverables + stopping point | `spec.md` → success criteria; `plan.md` → project structure |
| §3 Tasks `T<phase>.<n>` with S/M/L | `tasks.md` (`T001…`, size S/M/L in each task, no durations) |
| §4 Acceptance criteria | `spec.md` acceptance scenarios + `tasks.md` test tasks |
| §5 Dependencies on earlier phases | `plan.md` Technical Context → "Depends on: D-xx, feature NNN" |
| §6 Risk table | `plan.md` → section "Risks" (risk, probability [ESTIMATE], impact, mitigation) |
| §7 Decisions D-xx | `research.md` (Decision / Rationale / Alternatives) + row in `docs/decisions.md` |
| Hypotheses H1–H7 | `docs/hypotheses.md`; open ones enter the spec as `[NEEDS CLARIFICATION]` |
| "Clarifying questions (max 5)" | `/speckit-clarify` (asks up to 5) |
| Web research `<mandatory_checks>` | `/speckit-plan` Phase 0 research → `research.md` with links and labels |
| Part A / Part B "continue" pauses | Not needed: specify → plan → tasks are already separate steps |
| Module closing (log, hypotheses, 15-line summary) | Update `docs/decisions.md` and `docs/hypotheses.md` at the end of `/speckit-plan` |
| `06-consolidation.md` | `/speckit-analyze` per feature + periodic review of this roadmap |

## Per-feature flow

```
/speckit-specify <input below>      → specs/NNN-name/spec.md
/speckit-clarify                    → resolves up to 5 [NEEDS CLARIFICATION]
/speckit-plan <plan input below>    → plan.md, research.md (D-xx), data-model.md, contracts/, quickstart.md
/speckit-tasks                      → tasks.md (S/M/L, dependency order)
/speckit-analyze                    → consistency check against the constitution
/speckit-implement                  → code
```

After `/speckit-plan`, append the new D-xx rows to `docs/decisions.md` and update
`docs/hypotheses.md`.

## Features

| # | Feature | Phase | Run when | Source prompt |
|---|---|---|---|---|
| 001 | language-spec | 0 | **done** (branch `001-language-spec`, merged to `main`) | `01-phases-0-1.md` (Part A) |
| 002 | blocking-prototype | 1 | **in progress** (spec + plan done; branch `002-blocking-prototype`) | `01-phases-0-1.md` (Part B) |
| 003 | core-ergonomics | between 1 and 3 | **done** (branch `003-core-ergonomics`, pull request to `main`): report `docs/ergonomics-report.md`, decisions D-65..D-79 | — (added 2026-11-25; not in the original prompt set) |
| 004 | dependency-gate | 2 | prototype runs programs 1–10 + simulated 11–12 | `02-phase-2.md` |
| 005 | vm-scheduler-gc | 3 | Phase 2 spikes done | `03-phases-3-4.md` |
| 006 | async-io-timers | 4 | VM runs programs 1–9 | `03-phases-3-4.md` |
| 007 | remote-futures | 5a | VM runs programs 1–10 | `04-phase-5.md` |
| 008 | distributed-scopes | 5b | after 007 | `04-phase-5.md` |
| 009 | content-addressed-code | 5c | after 007 | `04-phase-5.md` |
| 010 | remote-agents | 5d | after 008 | `04-phase-5.md` |
| 011 | distributed-data-parallelism | 5e | after 009 | `04-phase-5.md` |
| 012 | distributed-actors (optional) | 5f | only if programs show the need | `04-phase-5.md` |
| 013 | llvm-backend | 6 | close to Phase 6 | `05-phases-6-7.md` |
| 014 | tooling | 7 | any time after 002 (embryonic tools grow along) | `05-phases-6-7.md` |

Phase 0 and Phase 1 are split: Phase 0's deliverable (specification by example + the 12
programs + distribution-affecting decisions) is a document feature whose output becomes the
reference for Phase 1's implementation.

---

### 001 — language-spec (Phase 0) — done

Delivered: `docs/spec/` (index, 14 sections, I/O appendix, open-questions register),
`programs/01…12` with `expected.qx`/`conditions.qx`, actor comparison, decisions D-01..D-28 and
the approved naming glossary. The inputs below are kept as a record.

**`/speckit-specify` input:**

```text
Language specification by example for quux (Phase 0). quux is a Lisp dialect: immutable by default, persistent canonical collections, mandatory TCO, concurrency centered on futures inside structured scopes, agents, streams of futures, deterministic data parallelism; actors are not in the core; distribution is a future goal.

Deliverables (documents and runnable-later example programs, no runtime code):
1. A specification skeleton: its sections and the question each one answers (values and equality, collections, evaluation and TCO, macros and hygiene, numerics, futures and their three states, scopes and cancellation, combinators all/any/then/timeout and the sequential-looking async macro, agents, streams, data parallelism, error model, serialization and identity, modules).
2. The 12 reference programs written in quux syntax with their expected observable behavior, and for each one what it tests in the design: (1) parallel search with all; (2) first response wins with any, cancelling the others; (3) timeout and error propagation in a scope; (4) bank account with an agent and atomic withdrawal; (5) pmap over a large persistent vector, deterministic; (6) file line stream with async map/filter/take; (7) fast producer, slow consumer (backpressure); (8) recursive fork/join; (9) deadlock between futures and how scopes avoid or detect it; (10) simple server with a scope per connection and shared state in an agent; (11) remote futures on two nodes with a node crash and recomputation; (12) remote agent with idempotent operations and resend after network failure, plus a cross-machine stream preserving backpressure.
3. Two or three of these programs also written with actors, side by side, to compare.
4. The decisions that affect distribution: function identity (by content or not), which values are serializable and how non-serializable ones (files, sockets, local resources) are marked, and the place of network errors and timeouts in the error model.

Open hypotheses to resolve or transfer (see docs/hypotheses.md): H3 (scope of the future returned by agent send), H4 (function hash vs captured environment, mutual recursion and cycles). Also decide or mark: cancellation semantics, fate of a future whose scope ended, whether futures escape scopes, propagation with several failing children, agent semantics (ordering, errors, reading current state), scope identity and serialization, equality and hash of functions and futures, modules compatible with content addressing, macro hygiene, numerics.

Success = each of the 12 programs has written expected behavior precise enough to become an automated test in Phase 1; every open question has a recommendation or is flagged with the phase that closes it.
```

**`/speckit-plan` input:**

```text
Document feature: output goes to docs/spec/ (specification sections) and programs/ (the 12 reference programs, .qx files + expected outputs). Record every design decision as D-01, D-02… in research.md with alternatives and trade-offs, marking "inherited by: Phase 3" or "inherited by: Phase 5" where applicable (error model, cancellation semantics, scope identity, serialization). Bring in Clojure, Trio, Kotlin/Java StructuredTaskScope, Unison, E/Cap'n Proto and CHAMP where relevant. Use the phase template sections (objective, deliverables + stopping point, tasks with S/M/L, acceptance, dependencies, risk table, decisions) from docs/planning-prompts/00-base.md. Update docs/decisions.md and docs/hypotheses.md at the end.
```

### 002 — blocking-prototype (Phase 1)

**`/speckit-specify` input:**

```text
Blocking prototype of quux in C (Phase 1) that runs the reference programs in programs/ against the specification in docs/spec/ (feature 001). The test harness reads each program's conditions.qx and expected.qx as quux data, following specs/001-language-spec/contracts/reference-program.md, and every public name comes from docs/naming.md.

User stories, in priority order (each independently demonstrable):
P1 Toolchain smoke test: a minimal C program builds in every CMake preset (debug, asan-ubsan, tsan, release) on macOS, in the Ubuntu arm64 Docker container and in GitHub Actions CI on Linux x86_64; ASan catches a deliberate heap overflow, TSan a deliberate race, LSan/valgrind in the container a deliberate leak; record which Docker capabilities had to be adjusted.
P1 Core language: reader, tree-walking evaluator, closures, mandatory TCO, macros, numerics, persistent canonical collections (lists, vectors, CHAMP maps and sets) with stable node hashes; a REPL.
P2 Futures and scopes: blocking runtime (one OS thread per future, mutex + condition variable); with-scope, cancellation, error propagation; programs 1, 2, 3, 8, 9 pass.
P3 Agents with update, transact and update-once (programs 4, 10), streams with backpressure and the I/O appendix (programs 6, 7, 10), deterministic parallel-transform, parallel-reduce and fork-join (programs 5, 8).
P4 Multi-node simulation in one process with injected failures, delays, crashes and message loss as described by conditions.qx; spawn-on, :node-unreachable, :transport and update-once; programs 11 and 12 pass against simulated nodes.

Constraints: the minimal GC root and allocation convention (H2) must be followed by all runtime code even though the real GC arrives in Phase 3; module boundaries must make reuse in the VM explicit (H1); a minimal portability layer for macOS arm64, Linux arm64 and Linux x86_64 (H7, D-28); C identifiers in full words with the quux_ prefix (constitution VIII). Open points to close in this phase: variadic parameters and destructuring syntax (W4, W5), kind predicates such as vector? (W6, to be approved in docs/naming.md), deterministic primary error for replay (H9), stream ownership across scopes (H11).

Success = all 12 programs run as automated tests on macOS arm64, Linux arm64 and Linux x86_64 (CI) with no ASan/UBSan/TSan errors.
```

**`/speckit-plan` input:**

```text
C17 with GNU extensions (gnu17), CMake + Ninja presets, pthreads + stdatomic, Homebrew clang pinned, Docker Ubuntu LTS arm64 image (gcc, clang, cmake, ninja, valgrind) with source mounted as a volume; GitHub Actions CI on Linux x86_64 only, running the same presets (D-28, D-31). Define: C project structure (modules, internal API conventions, portability header, testing policy), Dockerfile and scripts, minimal special forms and primitives, minimal root/allocation convention (shadow stack or handles, decide and justify), the multi-node simulator design. First task is the toolchain smoke test. Continue D-xx numbering from docs/decisions.md. Risks table must include "x86_64 tested only in CI, no local reproduction" and "memory-ordering bugs masked on x86 TSO".
```

### 003 — core-ergonomics (between Phase 1 and Phase 3)

Added by the owner on 2026-11-25, after feature 002 User Story 2 (the sequential core) reached
`main`. Ten everyday programs over the sequential core, a classified friction report and an owner
decision list on names and library additions before the Phase 3 VM inherits the vocabulary. See
`specs/003-core-ergonomics/` (spec, plan, research D-55..D-79). Feature 002 continues afterwards
with its User Stories 3–5.

**Result:** eight programs run under the `ergonomics` label and two stay `pending` until the
string and reading functions exist. The report classifies 55 friction notes and 8 mistakes into
40 entries. The owner approved all fifteen decision items (D-65..D-79, `docs/naming.md` section
K): string, path, search, ordering, formatting, numeric, error and macro-expansion additions, a
short map pattern, and three behavior changes (`case` without a match raises
`:no-matching-case`, malformed special forms raise `:syntax` with a line, a name defined in two
imports raises `:name-clash`). None is implemented yet. Five interpreter bugs found by the
programs were fixed.

### 004 — dependency-gate (Phase 2)

**`/speckit-specify` input:**

```text
Dependency gate and toolchain validation (Phase 2).
Starting reality: the core vocabulary additions approved in feature 003 (D-65..D-79, the owner decision list in docs/ergonomics-report.md, names in docs/naming.md section K) are not implemented yet; a feature before or with the Phase 3 VM implements them, and the two pending ergonomics programs (03, 06) turn on when the string and reading functions exist. <FILL IN: smoke test results (ASan, TSan, LSan, valgrind, Docker adjustments), prototype components kept/discarded, where it hurt (allocation, node hashing, errors, concurrency), libraries already tried, C standard and compilers actually used, divergences from 002's plan>.

Decide, for each need, adopt library / write / postpone: persistent structures, multi-threaded GC for a shared heap, work-stealing scheduler, lexer/parser, REPL line editing, async I/O over kqueue and epoll, TCP/QUIC transport, binary serialization, cryptographic hash for content addressing, node-to-node authentication, LLVM via C API (only: enter now or postpone, and which major to pin). Run 3 to 5 bounded spikes (e.g. minimal multi-threaded GC, work-stealing deque, hash + serialization of persistent nodes, echo over the chosen transport), each with an acceptance criterion. Revise the base toolchain with the smoke-test data and say what evidence would change each adopt-or-write decision.
```

**`/speckit-plan` input:** see `docs/planning-prompts/02-phase-2.md` `<mandatory_checks>` — all of it is the research phase of `/speckit-plan` (web research with links and dates; criteria matrix with weights).

### 005 — vm-scheduler-gc (Phase 3) and 006 — async-io-timers (Phase 4)

**`/speckit-specify` input (005):**

```text
VM with explicit per-task stack, work-stealing scheduler, task suspension and cancellation, shared-heap GC (Phase 3).
Starting reality: <FILL IN: spike results, libraries adopted, components written, divergences>.
Programs 1–10 must run on the VM with the same results as the blocking prototype (the prototype is the test oracle). Cover: task state format, scheduler loop, deref (lock-free fast path when filled, suspension otherwise), scopes and cancellation, separate pool for blocking native calls, GC strategy for the shared heap using the root convention inherited from Phase 1 (safepoints, root map), per-task areas for non-escaping data (evaluate), and what is left ready so a future can be filled by a network message (inherited by Phase 5). Compare without discarding: CPS + trampoline, stackful coroutines, delimited continuations, state machines.
```

**`/speckit-specify` input (006):**

```text
Timers, asynchronous I/O (kqueue on macOS, epoll on Linux, one poller layer), cooperative cancellation of I/O operations, integration with the blocking pool (Phase 4). Programs 6 and 10 run over real async I/O. Evaluate when and how an actor layer on top of agents and futures would be added, what it requires of the runtime and GC, and under which conditions it is not worth it.
```

### 007–012 — distribution (Phase 5a–5f)

One feature per sub-stage. Each `/speckit-specify` input starts with
`Starting reality: <decisions/divergences since 005–006>` and then the sub-stage goal from
`docs/planning-prompts/00-base.md` (Phase 5 list). Each `/speckit-plan` must cover, or say in
one line why it does not apply: protocol (message format, ids of nodes/tasks/scopes/agents,
remote future lifecycle), failure semantics (at-most / at-least / effectively-once),
distributed GC (leases vs distributed refcount; how much structured concurrency avoids it),
content addressing, transfer of persistent structures by node hash, task placement,
security (node authentication, executing received code, insecure deserialization in C),
tests (deterministic simulation reusing the Phase 1 node simulator, Docker containers on a
virtual network with injected failures). Never hide that a call is remote.

- 007 remote-futures (5a): `spawn-on`, value serialization, transport, promise pipelining. Examine H6.
- 008 distributed-scopes (5b): cross-node cancellation, heartbeats and leases, orphan-task cancellation, remote resource release. Examine H6.
- 009 content-addressed-code (5c): function hash with dependencies, per-node code cache, fetch-missing protocol, reloading and macros. Revisit H4.
- 010 remote-agents (5d): single owner, idempotent operations with op ids, delivery semantics. Examine H5.
- 011 distributed-data-parallelism (5e): partitioning persistent structures, dmap/dfilter/dreduce, lineage recomputation, streams between machines. Program 11 and 12 on real nodes.
- 012 distributed-actors (5f, optional): only if the reference programs show the need; spec first states what would decide it.

### 013 — llvm-backend (Phase 6) and 014 — tooling (Phase 7)

**`/speckit-specify` input (013):**

```text
Own IR and LLVM backend through the LLVM C API (Phase 6).
Starting reality: <FILL IN>.
First user story: a minimal LLVM C API spike that generates and runs a simple function. Then: IR design (SSA/CPS/ANF; representation of suspension points, cancellation, remote calls, safepoints), optimizations, native task suspension strategy (own state machines, LLVM coroutine intrinsics, separate stacks, or native only for non-suspending functions) and its GC trade-offs, native TCO with musttail on arm64 macOS and Linux, AOT vs JIT under macOS Apple Silicon restrictions, libLLVM linking vs emitting textual IR/bitcode for an external clang/llc, and native code as a node-local artifact derived from the content hash. The VM remains as REPL, debugger, reloading and test oracle.
```

**`/speckit-specify` input (014):**

```text
Tooling (Phase 7): REPL, debugger for tasks/scopes/nodes, test runner, network-failure simulation, packages. Prioritize what would have made earlier phases easier and promote the embryonic tools that already exist (REPL from 002, node simulator from 002, sanitizer presets).
```

### Consolidation (old module 06)

Not a feature. Run `/speckit-analyze` inside each feature, and periodically review this file,
`docs/decisions.md` and `docs/hypotheses.md` for contradictions, unanswered "inherited by"
decisions, stopping points and the top-5 risks.
