<!--
Sync Impact Report
- Version change: 1.2.2 → 1.2.3 (PATCH: CI scope narrowed to Linux x86_64 only, D-31;
  macOS arm64 and Linux arm64 are checked locally before every push by a pre-push hook, D-53)

Previous report (1.2.2):
- Version change: 1.2.1 → 1.2.2 (PATCH: Principle VIII examples aligned with the approved
  glossary in docs/naming.md)

Previous report (1.2.1):
- Version change: 1.2.0 → 1.2.1 (PATCH: Principle VIII clarified; quux-name exceptions
  allowed only when approved in docs/naming.md; first exception: `.qx` extension)

Previous report (1.2.0):
- Version change: 1.1.0 → 1.2.0 (MINOR: target platforms expanded)
- Modified section: Platform & Toolchain Constraints. Linux x86_64 added as a target,
  built and tested in GitHub Actions CI. The "x86_64 is NOT a target" risk is replaced by
  the risk that x86_64 has no local runtime (CI only). Recorded as D-28.
- Modified principle V: the sanitizer gates also run in CI on Linux x86_64.

Previous report (1.1.0):
- Version change: 1.0.0 → 1.1.0 (MINOR: new principle VIII)
- Added principle: VIII. Readable Names, No Abbreviations (quux names + C identifiers;
  name-by-name approval in docs/naming.md; recorded as D-27)
- Governance: Constitution Check now covers Principles I–VIII
- Follow-up: rename existing names in specs/001-language-spec once docs/naming.md is approved

Previous report (1.0.0):
- Version change: (template, unversioned) → 1.0.0
- Modified principles: all template placeholders replaced (initial ratification)
  - [PRINCIPLE_1] → I. C Is the Only Implementation Language
  - [PRINCIPLE_2] → II. Immutable Values, Canonical Persistent Structures
  - [PRINCIPLE_3] → III. Structured Concurrency Centered on Futures
  - [PRINCIPLE_4] → IV. Validate the Design Before Performance
  - [PRINCIPLE_5] → V. Verifiable Small Milestones
  - added → VI. Solo, Spare-Time Planning (No Durations)
  - added → VII. Evidence and Traceability
- Added sections: Platform & Toolchain Constraints; Development Workflow
- Removed sections: none
- Templates reviewed (not modified, read constitution at runtime):
  ✅ .specify/templates/plan-template.md (Constitution Check gate applies)
  ✅ .specify/templates/spec-template.md
  ✅ .specify/templates/tasks-template.md (tasks MUST carry S/M/L size, no durations)
- Deferred TODOs: none
- Source: quux planning prompts 00-base.md … 06-consolidation.md (2026-11-24)
-->

# quux Constitution

quux is a Lisp dialect implemented entirely in C: immutable by default, with mandatory
TCO, persistent canonical data structures, and concurrency centered on futures inside
structured scopes, designed so that distribution remains viable.

## Core Principles

### I. C Is the Only Implementation Language

- The prototype, the VM, the runtime and the final version MUST be written in C
  (`-std=gnu17`). GNU extensions (computed goto, `__attribute__((musttail))`,
  `__builtin_*_overflow`, `typeof`) MUST be isolated behind macros in a single portability
  header. C23 features MAY be used only behind feature tests.
- The native backend MUST be generated through the LLVM C API, from the project's own IR.
  The VM MUST remain after the native backend exists (REPL, debugging, code reloading,
  test oracle).
- Rust, Cranelift, or any other implementation language or backend MUST NOT be proposed
  as alternatives. Discussions are limited to the risks and practices specific to C and
  the LLVM C API.
- Consolidated C libraries MAY be adopted for specific tasks, decided in the dependency
  gate (Phase 2).

Rationale: the decision is closed; reopening it wastes a solo developer's limited time.

### II. Immutable Values, Canonical Persistent Structures

- All values are immutable by default. Lists, vectors, maps and sets MUST be persistent
  with structural sharing (HAMT/CHAMP, trie-based or RRB vectors).
- Maps and sets MUST use canonical forms (CHAMP-style): the same collection always
  produces the same tree, and each node has a stable hash.
- Only entities with identity — futures, agents and scopes — MAY carry mutable state or
  require remote references; everything else is freely copyable and serializable unless
  explicitly marked as a local resource (files, sockets, handles).
- Tail-call optimization is mandatory from the first evaluator onward.

Rationale: immutability enables a shared heap without concurrent writes, transfer of
structures by node hash, and recomputation of pure work after failures.

### III. Structured Concurrency Centered on Futures

- Every future MUST be born inside a scope (`with-scope`) and MUST NOT outlive it. On scope
  exit, every child has finished or been cancelled. An error in a child cancels its
  siblings and propagates to the parent.
- Scopes are first-class values with identity, so that cancellation can cross the network.
- A future has exactly three states: empty, filled, failed (with the error). The failed
  state MUST account from the start for network errors and timeouts.
- Suspension belongs to the runtime: any function can suspend; there is no async/await
  contagion.
- The concurrency surface is: futures, combinators (`all`, `any`, `then`, `timeout`,
  sequential-looking async macro), agents (serialized updates, each send returns a
  future), streams of futures (pull-based backpressure), and deterministic data
  parallelism (`pmap`, fork/join, reducers) for pure functions.
- Actors are NOT part of the core. They MAY be added later as a layer (agent + mailbox +
  address) only if the reference programs demonstrate a need for supervision with restart.

### IV. Validate the Design Before Performance

- The first runtime MUST be blocking (one OS thread per future, mutex and condition
  variable). Performance is irrelevant until the design is validated by the reference
  programs.
- Only then are runtime techniques evaluated (preferred: VM with explicit per-task stack,
  work-stealing pool, suspended-task lists per future, separate blocking pool), comparing
  without discarding CPS+trampoline, stackful coroutines, effect handlers and state
  machines.
- Decisions that would make distribution unviable if taken late MUST be taken early:
  function identity (content addressing), which values are serializable, the place of
  network errors in the error model, scope identity, and the GC root discipline in C.

### V. Verifiable Small Milestones

- Every task MUST end in something that runs, with an observable acceptance criterion
  (a test, a command, an assertion), not "implement X and test it".
- Decisions SHOULD be reversible; risks MUST be discovered early (spikes before
  commitments).
- At the end of every phase the project MUST be stoppable with a usable artifact
  (a documented stopping point).
- Sanitizers are gates:
  - ASan+UBSan and TSan on macOS;
  - LeakSanitizer and valgrind in the Linux arm64 container;
  - the same presets in CI on Linux x86_64.

  A concurrency milestone is not accepted with TSan errors on any target.
- When a phase does not fit these constraints, the plan MUST say what to cut or postpone.

### VI. Solo, Spare-Time Planning (No Durations)

- This is a personal, unpaid project with no deadline, executed by one person.
- Plans and tasks MUST NOT contain durations, dates, weeks, days, or hour estimates.
  Only dependency order and relative size (S, M, L) are allowed.
- Each task MUST be small enough to finish in a few work sessions.

### VII. Evidence and Traceability

- Design decisions get continuous IDs `D-xx` across the whole project (never restarting).
  Each decision is closed in exactly one feature/phase, recorded in that feature's
  `research.md` and in the project-wide log `docs/decisions.md`, with alternatives and
  trade-offs. Later phases reference the ID instead of repeating it. Decisions later
  phases depend on are marked `inherited by: Phase N`.
- Design tensions are tracked as hypotheses `H-n` (continuing from H8 for new ones) in
  `docs/hypotheses.md` until confirmed, refuted or transferred, each with the phase where it
  is revisited.
- Relevant claims MUST be labeled `[FACT: source]`, `[MEMORY]` (may be outdated),
  `[OPINION]` or `[ESTIMATE]` (only for risk probabilities and relative sizes).
- Anything that depends on the current state of the world (versions, maintenance,
  package availability, tool support) MUST be researched on the web at decision time and
  cited with links; what cannot be verified is labeled `[MEMORY]`. Items marked
  "(to verify)" are not facts.
- Ambiguity and contradictions MUST be flagged with a proposed resolution
  (`[NEEDS CLARIFICATION]`), never silently resolved by invention.

### VIII. Readable Names, No Abbreviations

- **quux names**: every name visible to a quux programmer MUST be written in full English
  words. This covers special forms, functions, macros, keywords, error kinds, file extensions
  and reader shorthands. Names inherited from Lisp tradition that are abbreviations or
  historical artifacts MUST NOT be used. Examples:
  - `cond` → `conditional`
  - `fn` → `function`
  - `def` → `define`
  - `eq` / `=` → `equal?`
  - `deref` → `wait-for`
  - `conj` / `assoc` → `add` / `put`

  Mathematical operators (`+ - * / < > <= >=`) are allowed, because they are notation rather
  than abbreviation. Any other exception MUST be approved explicitly and listed under
  "Approved exceptions" in `docs/naming.md`. The first such exception is the canonical file
  extension `.qx`, which is the only accepted extension.
- **Name-by-name review**: the canonical list of quux names is `docs/naming.md`. The project
  owner evaluates and approves each name individually. A public name MUST NOT appear in the
  specification, the reference programs or the code until its entry is approved. Adding or
  renaming a public name means updating `docs/naming.md` first.
- **C code**: identifiers MUST be full words in `snake_case`, with the project prefix `quux_`
  on exported symbols. For example, `quux_value_allocate`, never `qx_val_alloc`. Allowed
  exceptions:
  - loop counters and indices (`i`, `j`, `k`, `index`);
  - names imposed by the C standard library, POSIX or external libraries;
  - the acronyms listed as approved in `docs/naming.md` (for example `champ`, `utf8`,
    `llvm`).

  Code review, and `clang-tidy` naming checks where feasible, enforce this rule.

Rationale: the code and the language are read far more often than they are written, by one
person, across long gaps between work sessions. Full words remove the need to remember
historical conventions.

## Platform & Toolchain Constraints

- Development: macOS on Apple M2 (arm64, 16 KiB pages, weak memory model).
- Targets:

  | Target | Where it is built and tested |
  |---|---|
  | macOS arm64 | locally |
  | Linux arm64 | locally, in Ubuntu LTS Docker, native on the M2 |
  | Linux x86_64 | GitHub Actions CI (D-28) |

  x86_64 is not run locally; CI is its only test environment. Emulation on the M2 is not a
  test environment.
- CI (GitHub Actions) runs the same CMake presets **only on Linux x86_64** (D-31). macOS
  arm64 and Linux arm64 are not run in CI; a versioned pre-push git hook runs the same presets
  on both before every push and blocks the push on failure (D-53). The runner label is verified when the workflow is
  written (to verify).
- Portability consequences, kept in every risk table that touches concurrency or code
  generation:
  - x86_64 has a stronger memory model (TSO) than arm64. Ordering bugs therefore show up first
    on arm64 (local), while x86_64 adds a different ABI, a different code generator and a
    different alignment behavior.
  - Inline assembly and architecture-specific intrinsics MUST sit behind the portability
    header, with a version for each target architecture.
  - The page size (16 KiB on macOS arm64, 4 KiB on x86_64 Linux) MUST be queried at run time,
    never hard-coded.
  - Whether the Phase 6 native backend also emits x86_64 code is decided in Phase 6. The VM
    MUST run on every target.
- Compilers: Homebrew clang with the major version pinned and equal to the LLVM C API
  major used in Phase 6; Apple clang only as a smoke test at each milestone; gcc and clang
  in the Linux container for warning and UB divergences.
- Threads and atomics: pthreads with `<stdatomic.h>`; `<threads.h>` is not used.
- Build: CMake + Ninja with presets `debug`, `asan-ubsan`, `tsan`, `release`; the same
  presets run on macOS and in the container. `compile_commands.json`, clang-format and
  clang-tidy are mandatory.
- The base toolchain is a starting point: it changes only with evidence, recorded as a
  `D-xx` decision.

## Development Workflow

- Project phases map to Spec Kit features, specified only when reached, because library
  and version research ages:
  - Phases 0–1: language specification by example + blocking prototype.
  - Phase 2: dependency gate, spikes and toolchain validation.
  - Phases 3–4: VM, work-stealing scheduler, shared-heap GC; timers, async I/O,
    optional actors evaluation.
  - Phase 5 (5a–5f): distribution, one feature per sub-stage.
  - Phases 6–7: own IR and LLVM backend; tooling.
- Each feature runs `/speckit-specify` → `/speckit-clarify` → `/speckit-plan` →
  `/speckit-tasks` → `/speckit-analyze` → `/speckit-implement`. A feature starts from
  reality: divergences from earlier plans are recorded in its spec before planning.
- The 12 reference programs (all/any, timeout, bank agent, pmap, file stream,
  backpressure, fork/join, deadlock, server, remote futures with node crash, remote
  idempotent agent + cross-machine stream) are the acceptance suite shared by all phases.
  Programs 11 and 12 are first validated with simulated nodes in one process with
  injected failures and delays.
- Study references (Clojure, Trio, Kotlin/Java StructuredTaskScope, Erlang/OTP, Unison,
  E/Cap'n Proto, Spark, Java RMI leases, Git/Merkle trees, CHAMP) are cited where relevant.

## Governance

- This constitution supersedes other practices. Every `/speckit-plan` MUST pass the
  Constitution Check against Principles I–VIII; violations require a justified entry in
  the plan's Complexity Tracking table.
- Amendments are recorded as a `D-xx` decision with rationale in `docs/decisions.md` and
  bump the version: MAJOR for removing or redefining a principle, MINOR for a new
  principle or materially expanded guidance, PATCH for clarifications.
- `/speckit-analyze` is run before implementation of each feature to check
  spec/plan/tasks consistency with this constitution.

**Version**: 1.2.3 | **Ratified**: 2026-11-24 | **Last Amended**: 2026-11-25
