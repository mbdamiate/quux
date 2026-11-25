# Research: quux Blocking Prototype (Phase 1)

**Feature**: [spec.md](./spec.md) | **Plan**: [plan.md](./plan.md) | **Reference date**: 2026-11-25

Decisions continue the numbering of `docs/decisions.md` (D-01..D-28 from feature 001). Each
record has Decision, Rationale, Alternatives and "Inherited by". Claims are labeled
`[FACT: source]`, `[MEMORY]` (may be outdated), `[OPINION]` or `[ESTIMATE]` (constitution VII).
Every fact about versions, packages and runners was checked on the web on the reference date;
anything marked "(to verify)" is confirmed by the smoke test (T001–T008 in tasks) and is not a
fact yet.

## Contradictions with the base block

1. The base block says "x86_64 is NOT a target". Constitution v1.2.0 and D-28 made Linux x86_64 a
   CI-only target. **Adopted:** D-28 (it is newer and was approved by the owner).
2. The base block leaves the Ubuntu LTS version "to be defined in Phase 1". It is defined here
   (D-30).
3. The base block pins the macOS clang major "equal to the LLVM C API major used in Phase 6".
   Phase 6 is far away, so the pin chosen now (D-29) is provisional and is revisited at the
   Phase 2 gate and again when Phase 6 starts.

## Toolchain and environment

### D-29 — Compiler pin: LLVM/clang 22 on every target, gcc in the container

- **Decision**: clang major **22** everywhere: Homebrew formula `llvm@22` on macOS, package
  `clang-22` from the Ubuntu 26.04 archive in the container and on the CI runner. gcc from the
  Ubuntu 26.04
  archive is a second compiler in the container and CI, for warnings and undefined-behavior
  divergences. Apple clang builds the `debug` preset once per milestone as a smoke test. The C
  standard is `-std=gnu17`; GNU extensions are used only through the portability header (D-48).
- **Rationale**:
  - Homebrew's `llvm` formula is already at major 23, and `llvm@22` is at 22.1.8
    `[FACT: https://formulae.brew.sh/formula/llvm, https://formulae.brew.sh/formula/llvm@22]`.
  - The Ubuntu 26.04 archive ships LLVM 17 to 22, with 21 as the default
    `[FACT: https://documentation.ubuntu.com/ubuntu-for-developers/reference/availability/llvm/]`.
  - LLVM 23 for 26.04 exists only as snapshot builds on apt.llvm.org
    `[FACT: https://apt.llvm.org/resolute/dists/llvm-toolchain-resolute-23/main/binary-arm64/Packages]`.
  - Major 22 is therefore the newest major that is a **release** on both macOS and Ubuntu from
    first-party package sources, with no third-party repository in the image `[OPINION]`.
- **Alternatives**: major 23 (newest on Homebrew, but snapshot-only on Ubuntu, and a moving
  target); major 21 (Ubuntu's default, but older on macOS for no gain); different majors per
  platform (rejected: warnings and sanitizer behavior would diverge).
- **Inherited by**: Phase 2 (gate re-verifies), Phase 6 (LLVM C API major).

### D-30 — Linux container: Ubuntu 26.04 LTS arm64, source mounted as a volume

- **Decision**: a `Dockerfile` based on `ubuntu:26.04` (arm64, native on the M2), with
  `clang-22`, `gcc`, `cmake`, `ninja-build`, `valgrind`, `clang-format-22`, `clang-tidy-22`. The
  source is mounted at `/work`, and the build directory is a named volume, so macOS and Linux
  builds never share a build tree. Scripts in `tools/container/` build the image and run any
  preset inside it.
- **Rationale**: 26.04 is the current LTS; its archive has valgrind 3.26.0
  `[FACT: https://packages.ubuntu.com/resolute/amd64/valgrind]` and clang 22 (D-29). The same
  release is the `ubuntu-latest` image on GitHub Actions from November 2026
  `[FACT: https://github.blog/changelog/2026-09-17-ubuntu-26-generally-available-and-latest-migration]`,
  so the container and CI share one distribution.
- **Sanitizer adjustments to verify in the smoke test** (each recorded in
  `docs/toolchain-record.md` with what was actually needed):
  - LeakSanitizer uses ptrace-like operations, and Docker's default seccomp profile can block
    them; the expected adjustment is `--cap-add SYS_PTRACE` `[MEMORY]` (to verify).
  - ThreadSanitizer fails with "unexpected memory mapping" when `vm.mmap_rnd_bits` is above
    what it supports (30 in recent LLVM). The sysctl belongs to the **host kernel** (the Docker
    Desktop VM), not the container; TSan's fallback re-executes without address randomization
    through the `personality` system call, which Docker's default seccomp profile may forbid
    `[FACT: https://bugs.launchpad.net/ubuntu/+source/linux/+bug/2056762]`. Expected
    adjustment: `--security-opt seccomp=` with a profile that allows `personality`, or
    lowering the sysctl in the Docker VM (to verify).
- **Outcome of the smoke test (T012, 2026-11-25)**: none of the expected adjustments was needed.
  Docker Desktop's VM kernel (7.0.14-linuxkit) has `vm.mmap_rnd_bits = 18`, so TSan runs, and
  LSan reports leaks under the default seccomp profile without `SYS_PTRACE`. The image has
  Ubuntu clang 22.1.2, gcc 15.2.0, valgrind 3.26.0 and CMake 4.2.3
  (`docs/toolchain-record.md`). If a Docker Desktop update changes the kernel settings, the
  smoke tests catch it.
- **Alternatives**: Ubuntu 24.04 (older valgrind and clang; CI is moving away from it);
  Debian testing (not an LTS); building inside a Linux VM instead of Docker (heavier, no gain).
- **Inherited by**: Phase 2, Phase 5 (one container per node).

### D-31 — CI on GitHub Actions: Linux x86_64 only

- **Decision** (owner, 2026-11-25): one workflow, `.github/workflows/build-and-test.yml`, on
  every push and pull request, running **only on Linux x86_64**:

  | Runner label | Architecture | Presets | Extra |
  |---|---|---|---|
  | `ubuntu-26.04` | x86_64 | debug, asan-ubsan, tsan, release | valgrind on `debug`; gcc build on `debug` |

  The job runs directly on the runner with the same packages as the container (not inside
  Docker), so that TSan is not affected by Docker's seccomp profile. A failed test uploads its
  log and the harness report (D-46) as an artifact. macOS arm64 and Linux arm64 are not in CI:
  they are built and tested locally, on the M2 and in the container (D-30), with the same
  presets, before each push that closes a story.
- **Rationale**: CI exists for the one target that cannot run locally (D-28). The arm64
  targets already run natively on the development machine. Ubuntu 26.04 x86_64 images are
  generally available
  `[FACT: https://github.blog/changelog/2026-09-17-ubuntu-26-generally-available-and-latest-migration]`;
  the label was confirmed by CI run 45616559024 on 2026-11-25 (`docs/toolchain-record.md`).
- **Reproducing x86_64-only failures** (H7): there is no local x86_64 machine and emulation is
  not a test environment (constitution). Every test is addressable by name, every run prints
  the random seeds it used (scope identifiers, D-21), and a manual `workflow_dispatch` input
  re-runs a single test N times on one runner.
- **Alternatives**: adding `ubuntu-26.04-arm` and `macos-15` runners, which are free for public
  repositories
  `[FACT: https://github.blog/changelog/2025-08-07-arm64-hosted-runners-for-public-repositories-are-now-generally-available/]`
  (rejected by the owner: arm64 is already covered locally); running the job inside the project
  container (more faithful to the local setup, but adds the TSan/seccomp problem to CI).
- **Consequence**: arm64 is not checked by CI, so it is checked before every push by a git
  hook (D-53).
- **Inherited by**: every later phase.

## Code organization

### D-32 — Module boundaries and reuse tags (resolves H1 for Phase 1)

- **Decision**: the code is split into modules under `source/`, each with one public header,
  and each module states in its header whether the Phase 3 VM is expected to **keep** it,
  **adapt** it or **replace** it:

  | Module | Content | Reuse |
  |---|---|---|
  | `portability` | threads, mutexes, condition variables, atomics, clocks, page size, entropy, compiler attributes | keep |
  | `memory` | allocation, root frames, write-barrier hook, root verifier (D-34) | adapt (collector added in Phase 3) |
  | `values` | value representation, symbols, keywords, strings, numbers, errors (D-35) | keep |
  | `collections` | lists, vectors, CHAMP maps and sets (D-36) | keep |
  | `hashing` | value hash and content hash (D-37) | keep |
  | `reader`, `printer` | text ↔ values | keep |
  | `expander` | macro expansion, quasi-quote, destructuring and `:rest` lowering (D-39) | keep |
  | `namespaces` | name → definition hash, content hashing of definitions (D-19) | keep |
  | `encoding` | canonical binary encoding used by content hashing and by the simulator (D-45) | keep |
  | `evaluator` | tree-walking evaluator with tail calls (D-38) | replace (by the VM) |
  | `concurrency` | futures, scopes, cancellation, deadlock graph, agents, streams, written against the executor interface | adapt |
  | `executor_blocking` | one thread per task, blocking waits (D-40) | replace (by the work-stealing scheduler) |
  | `input_output` | files, listeners, connections (D-44) | replace (by Phase 4) |
  | `simulation` | simulated nodes and network (D-45) | keep (Phase 5 test tool) |
  | `harness` | program runner, assertions, example suite (D-46) | keep |
  | `main` | entry points of `quux` (REPL and file runner, D-52), `quux-test` (D-46) and `quux-name-check` (D-47) | keep (grows in Phase 7) |

  The seam that makes reuse explicit is the **executor interface** (`quux_executor_*`): start a
  task, park the current task until a condition, wake a task, read the cancellation flag, and
  sleep. `concurrency` depends only on that interface, never on pthreads, so the VM can supply a
  scheduler with the same contract.
- **Rationale**: H1 asked which boundary avoids rework. The semantics of futures, scopes,
  agents and streams (state machines, propagation, the wait-for graph) are the expensive and
  bug-prone part and are not tied to threads; the threading model is `[OPINION]`. Tagging each
  module records the H1 evidence as the code is written.
- **Alternatives**: one monolithic runtime (fast to start, but H1 would be answered "discard
  everything"); building the VM directly (contradicts Principle IV).
- **Inherited by**: Phase 3 (VM), Phase 4 (input/output), Phase 5 (simulation, encoding).

### D-33 — Internal C conventions and error propagation

- **Decision**:
  - Every exported symbol is `quux_<module>_<verb or noun>` in full words (constitution VIII,
    `docs/naming.md` section H). Types are `quux_<noun>` typedefs of structs.
  - A C function that can raise a quux error returns `bool` (`true` on success), writes its
    result through an out parameter, and on failure stores the error value in the current
    task's context (`quux_context`). No `setjmp`/`longjmp` is used for quux errors.
  - Every runtime function receives the `quux_context *context` explicitly (task, root frame
    stack, cancellation flag, current scope, current node).
  - Internal invariants use `quux_assert` (aborts in debug and sanitizer presets, compiled out
    in release).
- **Rationale**: `longjmp` across mutex-holding frames and across registered root frames breaks
  both the blocking runtime and the root convention (D-34) `[OPINION]`. An explicit context
  replaces thread-local state, which the VM would otherwise have to emulate when tasks migrate
  between threads.
- **Alternatives**: `setjmp`/`longjmp` for raise (simpler call sites, but unsafe with locks and
  roots); thread-local context (hidden coupling to threads, contradicts D-32).
- **Inherited by**: Phase 3.

### D-34 — Minimal root and allocation convention (resolves H2 for Phase 1)

- **Decision**: **precise roots through a per-task shadow stack of root frames**, plus a
  write-barrier hook, plus a verifier that makes violations observable.
  - Every C function that holds a heap value across a call that can allocate opens a root frame
    and registers the **addresses** of its local `quux_value` variables:

    ```c
    quux_value list = quux_nothing, element = quux_nothing;
    QUUX_ROOT_FRAME(context, &list, &element);   /* registered until the frame closes */
    ...
    QUUX_ROOT_FRAME_END(context);
    ```

    Registering addresses (not values) keeps the convention valid for a **moving** collector.
  - Global roots (namespaces, interned symbols and keywords, nodes) are registered once with
    `quux_memory_register_global_root`.
  - Every store into one of the few mutable cells (future result, agent state, stream cell,
    namespace binding) goes through `quux_memory_write_barrier(context, owner, value)`, a no-op
    in Phase 1.
  - **Root verifier** (preset `debug`, option `QUUX_VERIFY_ROOTS`): at every allocation in
    single-task runs (the example suite and the sequential parts of programs), it marks
    everything reachable from the registered roots and poisons every unreachable object (with
    the address sanitizer's manual poisoning when available, with a byte pattern otherwise).
    Any use of an unregistered reference then fails under ASan or on a poison check.
- **Rationale**: H2 says the discipline contaminates all runtime code and is costly to
  retrofit. A shadow stack of addresses is the simplest precise scheme that also supports
  moving and generational collection later `[OPINION]`. Without the verifier the convention
  could not be checked until Phase 3, which FR-027 forbids.
- **Alternatives**: conservative stack scanning (no discipline needed, but blocks moving
  collection and is harder to make safe across threads); handles/indirection table (one extra
  load per access, more invasive API); reference counting (cycles through closures and
  namespaces, and atomic counts on shared immutable data are expensive).
- **Limit**: the verifier runs only while one task is running; concurrent verification needs
  safepoints, which belong to Phase 3 (risk R6 in the plan). FR-027 states this limit.
- **Amendment (after `/speckit-analyze`, finding C1)**: the memory module records every
  allocation and `quux_memory_shutdown` frees all of them when the runtime ends, so LSan and
  valgrind report only real leaks (SC-002) without suppressions. An allocation that fails
  returns `false` with `:resource-exhausted` (FR-017, finding U3).
- **Inherited by**: Phase 3 (collector), Phase 6 (native code must emit the same frames).

## Language core

### D-35 — Value representation

- **Decision**: `quux_value` is a 64-bit tagged word. The low 3 bits select: pointer to a heap
  object (with a header holding kind, flags such as `local-resource`, and the cached value
  hash), small integer (61 bits; larger 64-bit integers are boxed), and constants (`nothing`,
  `true`, `false`). Floating-point numbers are boxed. Heap objects are aligned to 8 bytes on all
  three targets (alignment is checked by a static assertion, H7).
- **Rationale**: simple, portable across the three targets, and keeps integer arithmetic with
  `__builtin_*_overflow` cheap for the common case `[OPINION]`. NaN-boxing is a VM-level
  optimization and can be decided in Phase 3 without changing the C API, because all access
  goes through `quux_value_*` functions.
- **Alternatives**: every value boxed (simplest, slowest); NaN-boxing now (premature; awkward
  with 64-bit integers).
- **Inherited by**: Phase 3 (may revisit), Phase 6.

### D-36 — Collections

- **Decision**:
  - Lists: immutable linked cells with a cached count.
  - Vectors: persistent 32-way bit-partitioned trie with a tail, as in Clojure
    `[MEMORY: Bagwell; Hickey]`. RRB concatenation stays postponed (D-03).
  - Maps and sets: CHAMP with 32-way bitmaps, separate data and node bitmaps, and canonical
    compaction on removal, so equal contents always give the same structure
    `[MEMORY: Steindorfer & Vinju, "Optimizing Hash-Array Mapped Tries for Fast and Lean Immutable JVM Collections", OOPSLA 2015]`.
    Hash collisions beyond the last level use a collision node sorted by the canonical order of
    keys, so the collision node is canonical too.
  - Every node caches its **value hash**, computed from its contents; the content hash
    (D-37) is computed lazily and cached on first request.
  - Printing a map or set iterates in trie order, which is canonical, so equal maps print
    identically (spec story 2, scenario 3).
- **Rationale**: CHAMP's canonical form is what makes structural equality, stable node hashes
  and Phase 5c transfer-by-node-hash possible (Principle II).
- **Alternatives**: HAMT as in Clojure (not canonical after removals); sorted trees (canonical
  but O(log n) and need a total order over all values).
- **Inherited by**: Phase 3, Phase 5c.

### D-37 — Hash functions (provisional until the H10 spike)

- **Decision**:
  - **Value hash**: XXH3 64-bit from xxHash, vendored as a single header, with a **fixed seed**.
  - **Content hash**: BLAKE3 (256 bits), the official C implementation, vendored in its
    **portable** variant only (no per-architecture SIMD files in Phase 1).
  - Both are vendored under `third_party/` with their licenses and exact versions, and are
    called only through `quux_hashing_*`, so the Phase 2 spike (H10) can replace them.
- **Rationale**: BLAKE3's C code is dual-licensed CC0 / Apache-2.0
  `[FACT: https://github.com/BLAKE3-team/BLAKE3]`; XXH3 is xxHash's 64/128-bit variant since
  v0.8.0 `[FACT: https://chromium.googlesource.com/external/github.com/Cyan4973/xxHash/]`, and
  its BSD-2 license is `[MEMORY]` (to verify when vendoring). Writing either by hand is a risk
  with no benefit. A fixed seed makes hashes, and therefore canonical structure and the
  `deterministic` assertion, identical across runs and machines.
- **Risk accepted**: a fixed seed allows hash-flooding of maps built from network input. It is
  acceptable for a prototype; the seed policy is revisited in Phase 4 (servers) `[OPINION]`.
- **Alternatives**: SipHash for the value hash (keyed, slower); SHA-256 for the content hash
  (slower, no tree mode for large structures).
- **Inherited by**: Phase 2 (H10 spike, dependency gate), Phase 5c.

### D-38 — Evaluator: expand fully, then walk with a tail-call loop

- **Decision**: every top-level form is fully macro-expanded before evaluation (this is also
  what D-06 and D-19 hash). The evaluator is a single C function that loops: when the
  expression in tail position is a call, `if`, `sequence`, `let` body or similar, it replaces
  the current expression and environment and continues the loop instead of recursing. Calls in
  non-tail position recurse in C. A depth counter limits C recursion and raises
  `:stack-exhausted` (D-51) before the thread stack overflows.
- **Rationale**: the loop gives constant C stack for every tail call, including mutual
  recursion through calls (FR-007, SC-005), without trampolines allocating per call
  `[OPINION]`.
- **Alternatives**: trampolining with thunks (allocates per tail call); continuation-passing
  style (heavy for a prototype); `musttail` in a C-level dispatch (belongs to the VM).
- **Inherited by**: Phase 3 replaces the evaluator; the tail positions stay as section 03
  defines them.

### D-39 — Rest parameters and destructuring (closes W4 and W5)

- **Decision**:
  - **W4**: the keyword `:rest` before the last parameter collects the remaining arguments in a
    list: `(define-function update [agent function :rest arguments] …)`. It is valid in
    parameter vectors and in vector patterns.
  - **W5**: patterns are allowed wherever a name is bound (`let`, parameters, `let-parallel`):
    - vector pattern: `[first-name second-name :rest others]`, matching any sequence;
    - map pattern: `{name-symbol key …}`, binding each symbol to `(get map key)`, `nothing` if
      absent (for example `{balance :balance owner :owner}`);
    - patterns nest.
  - A sequence shorter than a vector pattern binds `nothing` to the missing names; a value
    that is not a sequence (for a vector pattern) or not a map (for a map pattern) raises
    `:wrong-type`.
  - The expander lowers patterns to plain `let` bindings with `element-at`, `rest`-like slicing
    and `get`, so the evaluator and the content hash see only core forms.
- **Rationale**: owner's answers of 2026-11-25 (Q1: C, Q2: B). The map pattern uses only
  existing syntax (a map literal) and existing names, so it adds no reader rule and no new word
  `[OPINION]`. Lowering in the expander keeps the evaluator unchanged and lets the VM inherit it.
- **Names**: the spelling of the map pattern is a syntax form, not a new name; the owner approved
  it on 2026-11-25 (`docs/naming.md` J2).
- **Alternatives**: `& rest` (Lisp heritage, rejected by the owner); Clojure's `{:keys [a b]}`
  (adds the word `keys` with a special meaning; can be added later as a shorthand).
- **Inherited by**: Phase 3 (unchanged; lowered before the VM).

## Concurrency on the blocking runtime

### D-40 — Blocking executor and deadlock detection

- **Decision**:
  - Each future runs on its own pthread, created with an explicit stack size (a multiple of the
    page size queried at run time). Each task has a mutex, a condition variable, an atomic
    cancellation flag and a "waiting for" field.
  - `wait-for`, `sleep`, agent sends, `emit` and stream advance park the task on its condition
    variable with a timed wait, and re-check the cancellation flag on every wake-up.
  - **Deadlock detection** (D-14): a global wait-for graph protected by one mutex. Before
    parking on future F, a task records "waiting for F" and follows the chain
    F → task computing F → the future that task waits for → …; if the chain reaches the
    current task, every future in the cycle fails with `:deadlock` and `:cycle` lists them.
    Each task waits for at most one thing, so the chain is a path and the check is linear in its
    length.
  - If thread creation fails, the future fails with `:resource-exhausted` (D-51) instead of
    aborting the process.
- **Rationale**: the simplest model that makes the semantics observable and checkable under
  TSan (Principle IV). A global lock for the graph is acceptable at prototype scale; its cost
  in the VM is H8.
- **Alternatives**: a fixed thread pool (deadlocks itself when all threads block, which would
  hide the semantics being tested); green threads with `ucontext` (deprecated on macOS, H7).
- **Inherited by**: replaced in Phase 3; the wait-for graph's semantics are kept (H8).

### D-41 — Agents on the blocking runtime

- **Decision**: each agent has a FIFO queue and at most one worker thread, started when the
  queue becomes non-empty and ended when it is empty. The worker runs updates one at a time,
  commits through the write barrier, and fills the sender's future. `update-once` keeps a map
  from operation identifier to the state produced by the first application. A queued update
  whose sender scope is cancelled is withdrawn; a running one commits (D-16). The agent's own
  scope (section 06 rule 3) is created with the agent.
- **Rationale**: a single queue gives FIFO per sender (and a stronger global order, which the
  specification allows but tests must not rely on) `[OPINION]`.
- **Alternatives**: one permanent thread per agent (wastes threads in program 10).
- **Inherited by**: Phase 3 (semantics), Phase 5d (`update-once` memory, H5).

### D-42 — Streams, ownership and H11

- **Decision**: a stream cell holds `first` and a `rest` future. The producer is a future in
  the creating scope; it blocks in `emit` when it is `:maximum-ahead` cells ahead of the
  furthest cell any consumer has demanded. When the owning scope's body returns, its producers
  are cancelled (D-17 amendment). For **H11**, the runtime counts, per run, how often a stream
  is advanced from a task outside its owner scope's subtree and writes the count into the
  harness report; programs 06, 07 and 10 provide the evidence.
- **Rationale**: follows D-17 exactly and turns H11 into measured data instead of opinion.
- **Alternatives**: transferring ownership explicitly (an API change that H11 has not justified
  yet).
- **Inherited by**: Phase 4, Phase 5e.

### D-43 — Deterministic data parallelism

- **Decision**: `parallel-transform` and `parallel-reduce` split the input into chunks of a
  **fixed size** (by default 512 elements, independent of the number of processors) and combine
  partial results with the fixed reduction tree of D-18: a balanced binary tree over chunk
  indices. Chunks run as futures in a child scope. `fork-join` uses the `:leaf`/`:split`
  markers of section 11.
- **Rationale**: the result, including float rounding, depends only on the input and the chunk
  size, never on timing (SC-001 for program 05) `[OPINION]`.
- **Alternatives**: one chunk per processor (result depends on the machine); work stealing
  (Phase 3, with H12).
- **Inherited by**: Phase 3 (H12), Phase 5e.

### D-44 — Input/output appendix on the blocking runtime

- **Decision**: files, listeners and connections use blocking POSIX calls on the task's own
  thread. To keep them cancellation points, every blocking call goes through `poll` with a
  per-task **wake-up pipe**: `cancel!` writes a byte to it, so a blocked read returns at once
  and raises `:cancelled`. Resources carry the `local-resource` flag (D-20) and are closed by
  `finally` or when their scope ends; using a closed resource raises `:resource-closed`.
- **Rationale**: cancellation of blocked input/output is required by section 07 and program
  10 (a disconnected client); a wake-up pipe gives immediate cancellation without polling
  intervals `[OPINION]`.
- **Alternatives**: `poll` with a short timeout and periodic flag checks (simpler, adds
  latency and CPU use); signals to interrupt system calls (fragile with pthreads).
- **Inherited by**: replaced in Phase 4 (kqueue/epoll, H7).

## Simulation, harness, tooling

### D-45 — Multi-node simulator

- **Decision**:
  - A **simulated node** is a runtime object with a name, a root scope owned by the node, its
    tasks and agents, a crashed flag and a count of completed `spawn-on` tasks.
  - Every interaction between nodes (spawn request, result, agent update, acknowledgement,
    stream pull and cell, cancellation) is a **message** sent through
    `quux_simulation_network_send`. The payload is encoded with the canonical binary encoding
    (module `encoding`) and decoded on delivery, so serialization rules (D-20) are exercised for
    real: local-only values fail at the sender with `:not-serializable`, and futures, agents,
    scopes and nodes travel as remote references (node + identifier).
  - A single **network thread** delivers messages in order of delivery time (send time +
    `:latency-milliseconds`), applies `:drop` rules by counting matching messages per
    (from, to, message kind), and discards messages to or from crashed nodes.
  - Code travels as function hash + captured values; the receiving node looks the hash up in a
    process-wide code store (the fetch protocol is Phase 5c, H4).
  - `:crash-after {:completed n}` marks the node crashed when its n-th `spawn-on` task
    completes, after sending that result; pending futures owned by callers on other nodes then
    fail with `:node-unreachable` immediately (no heartbeats in simulation; deadlines are Phase
    5a/5b, W9).
  - `main` runs on the first node listed (harness convention).
- **Rationale**: running every remote interaction through one encoded message path means the
  simulation tests the same rules a network would, while staying deterministic enough for
  tests (Principle IV; Erlang's distribution and Cap'n Proto's references inform the message
  kinds `[MEMORY]`).
- **Alternatives**: passing pointers between simulated nodes (would hide serialization bugs);
  one process per node now (that is Phase 5's container setup).
- **Inherited by**: Phase 5 (message kinds, encoding), Phase 7 (network-failure simulation
  tool).

### D-46 — Test harness and suites

- **Decision**: one C executable `quux-test` with three modes:
  - `quux-test program programs/NN-name`: reads `conditions.qx` and `expected.qx` with the
    reader, rejects unknown keys and assertion forms, starts a fresh runtime, runs `main`,
    records observations through hooks (tag events, future final states, agent commits, stream
    look-ahead, printed lines, elapsed time), evaluates each assertion, and prints a report
    (contract `harness-report.md`). A watchdog stops the run at
    `:bounds :wall-clock-milliseconds` and reports a failure.
  - `quux-test examples docs/spec`: extracts every ```` ```quux ```` block, pairs each form
    with the `;=>` or `;!!` comment that follows it, and checks it.
  - `quux-test unit`: the C unit tests of each module, written with a minimal set of assertion
    macros in `tests/unit/` (no external framework) and linked into `quux-test`; there is no
    separate unit-test executable (finding I3).
  - CTest registers one test per program, per spec section and per module, so CI runs and
    re-runs tests by name; SC-003's 20 runs use `ctest --repeat until-fail:20`.
- **Rationale**: running each program in its own process isolates crashes and leaks; reusing
  the quux reader for the data files is required by FR-024 `[OPINION]`.
- **Alternatives**: a harness written in quux (not possible before the language works); an
  external C test framework (a dependency the Phase 2 gate has not reviewed).
- **Inherited by**: every later phase.

### D-47 — Name check and formatting gates

- **Decision**: `quux-name-check` (in `tools/`) uses the quux reader to collect every symbol and
  keyword in `programs/**/*.qx` and the quux blocks of `docs/spec/`, and compares them with the
  approved names in `docs/naming.md` (plus names defined by the program itself). For C, the
  `readability-identifier-naming` check of clang-tidy enforces the `quux_` prefix and
  snake_case, and `quux-name-check --c` splits identifiers into words and rejects the
  abbreviations that `docs/naming.md` section H lists as not allowed. clang-format runs as a
  check in CI.
- **Rationale**: SC-009 needs an automated check; reusing the reader keeps it exact.
- **Alternatives**: grep-based scripts (cannot tell symbols from strings and comments).
- **Inherited by**: every later phase.

### D-48 — Portability layer (H7, first part)

- **Decision**: `source/portability/quux_portability.h` is the only file that includes
  platform headers for threads, time and entropy, and the only place with `#if` on the
  platform or architecture. It provides: thread create/join with stack size, mutex, condition
  variable with monotonic timed wait, `<stdatomic.h>` wrappers, monotonic clock, page size
  (`sysconf`), 128-bit random identifiers (`getentropy`), and compiler attributes
  (`QUUX_MUSTTAIL`, overflow builtins, `QUUX_LIKELY`) behind feature tests. No unnamed
  semaphores, no barriers, no `<threads.h>`.
- **Rationale**: macOS lacks unnamed semaphores and pthread barriers and has no
  `<threads.h>` `[MEMORY]`; condition variables cover every need of the blocking runtime. The
  monotonic timed wait differs between platforms (`pthread_condattr_setclock` is missing on
  macOS `[MEMORY]`), so it is wrapped once (to verify in the smoke test).
- **Inherited by**: Phase 3, Phase 4 (kqueue/epoll), Phase 6.

### D-49 — Primary error stays in completion order (H9)

- **Decision**: D-11 is kept unchanged in Phase 1: the first failure in completion order is the
  primary error. Tests for racing failures use `raises-one-of`. The harness report records,
  for each program, whether its primary error was decided by a race; this is the evidence for
  H9, which moves to Phase 7 (replay).
- **Rationale**: owner's answer of 2026-11-25 (Q3: A).
- **Alternatives**: creation order in a test mode or always (rejected by the owner for now).
- **Inherited by**: Phase 7.

### D-50 — Kind predicates (W6)

- **Decision**: the following predicates were approved by the owner on 2026-11-25
  (`docs/naming.md` J3): `list?`, `vector?`, `map?`, `set?`, `keyword?`,
  `symbol?`, `string?`, `integer?`, `floating-point?`, `boolean?`, `function?`, `future?`,
  `agent?`, `scope?`, `stream?`, `error?`. (`nothing?` is already approved.)
- **Rationale**: section 11 needs at least `vector?`; the full family is proposed at once so
  that the names are consistent (R2, full words: `floating-point?` rather than `float?`).
- **Inherited by**: every later phase.

### D-51 — Error kinds for resource limits

- **Decision**: two error kinds, approved by the owner on 2026-11-25 (`docs/naming.md` J4) and
  added to section 12 by task T007: `:stack-exhausted` (non-tail recursion deeper than the limit, D-38) and
  `:resource-exhausted` (thread creation or memory allocation failed, D-40).
- **Rationale**: the spec's edge cases require both conditions to be error values, not
  crashes; no existing kind covers them.
- **Inherited by**: Phase 3 (the VM keeps the kinds).

### D-53 — arm64 verification before every push (git pre-push hook)

- **Decision** (owner, 2026-11-25): a versioned hook `tools/git-hooks/pre-push`, installed by
  `tools/install-git-hooks.sh` (which sets `core.hooksPath`), runs `tools/local-check.sh` and
  blocks the push if it fails. `tools/local-check.sh` builds and runs, for each target:

  | Target | Where | Presets | Tests |
  |---|---|---|---|
  | macOS arm64 | the M2 | debug, asan-ubsan, tsan, release | CTest labels `smoke`, `unit`, `examples`, `programs`, `names` |
  | Linux arm64 | the container (D-30) | debug, asan-ubsan, tsan, release | the same labels, plus valgrind on `debug` |

  Rules:
  - The hook prints a one-line summary per target and preset, and writes the full log to
    `build/local-check.log`.
  - Pushes that change only documentation (`docs/`, `specs/`, `*.md`) skip the build and run
    only the `names` label, because no C code or `.qx` behavior changed.
  - Tests labeled `pending` (example sections and programs that do not pass yet) are skipped by
    the hook and by CI; the task that makes one pass moves it to its active labels (finding I1).
  - A preset that `docs/toolchain-record.md` records as unavailable in the container (for
    example TSan, risk R3) is skipped with a visible warning instead of blocking; TSan on arm64
    still runs on macOS (finding G2).
  - The 20-run TSan loop (SC-003) is not part of the hook; it runs at story checkpoints with
    `tools/local-check.sh --repeat 20`.
  - Bypassing with `git push --no-verify` is allowed only for an emergency, and the reason is
    written in the next commit message.
  - Before the C build exists (the first smoke-test task), the hook runs only the `names` check
    and the preset configuration step.
- **Rationale**: CI runs only on x86_64 (D-31), and arm64 (macOS and Linux) is where
  memory-ordering bugs show first (risk R2). A hook makes the local check automatic instead of a
  habit `[OPINION]`. A versioned hook directory keeps the hook under review like any other code.
- **Alternatives**: a manual checklist at story checkpoints (easy to forget); a self-hosted CI
  runner on the M2 (needs the machine to be on and exposes it to the public repository's
  workflows, rejected `[OPINION]`); a pre-commit hook (too slow for every commit).
- **Inherited by**: every later phase.

### D-52 — REPL

- **Decision**: `quux` with no arguments starts a line-based REPL: it reads lines until the
  reader has a complete form, evaluates it in the `user` namespace, prints the value in
  canonical form or the error as `!! <kind>: <message>`, and continues. `quux file.qx [input]`
  runs a file's `main`. No line-editing library (Phase 7 decides).
- **Rationale**: minimal and dependency-free; enough for FR-013 and for exploring the
  specification `[OPINION]`.
- **Inherited by**: Phase 7.

## Hypotheses after this research

| Hypothesis | Status after planning | Closed by |
|---|---|---|
| H1 | Partly resolved: boundary = executor interface + reuse tags (D-32); confirmed when the prototype is complete | end of 002 |
| H2 | Resolved for the convention (D-34); concurrent verification moves to Phase 3 | Phase 3 |
| H7 | Partly resolved: portability layer (D-48), CI on x86_64 only with a reproduction method (D-31); the smoke test confirms the "to verify" items | end of 002, Phase 2 |
| H9 | Deferred with evidence collection (D-49) | Phase 7 |
| H10 | Provisional hash choice (D-37) | Phase 2 spike |
| H11 | Measured in programs 06, 07, 10 (D-42) | end of 002 |
