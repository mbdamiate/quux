# Implementation Plan: quux Blocking Prototype (Phase 1)

**Branch**: `002-blocking-prototype` | **Date**: 2026-11-25 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/002-blocking-prototype/spec.md`

## Summary

This feature builds the first runnable quux: a C prototype with a tree-walking evaluator and a
**blocking runtime** (one thread per future). It runs the 12 reference programs of feature 001
as automated tests through a harness that reads `conditions.qx` and `expected.qx` as quux data,
and it runs every `;=>` / `;!!` example of `docs/spec/`. The prototype runs on macOS arm64
(locally), on Linux arm64 (in an Ubuntu 26.04 container) and on Linux x86_64 (GitHub Actions),
in four build presets, with sanitizers as gates.

The plan's main technical choices are recorded in [research.md](./research.md) as D-29..D-53:

- toolchain pin and CI matrix (D-29..D-31);
- module boundaries with reuse tags and an executor interface (D-32, H1);
- precise roots through a shadow stack with a verifier (D-34, H2);
- canonical collections and provisional hashes (D-36, D-37);
- the `:rest` keyword and vector and map patterns (D-39, W4/W5);
- a simulator that sends every inter-node interaction as an encoded message (D-45).

## Technical Context

| Field | Value |
|---|---|
| **Language/Version** | C17 with GNU extensions (`-std=gnu17`) isolated in the portability header; clang 22 on every target, gcc in the container and CI (D-29). |
| **Primary Dependencies** | pthreads and `<stdatomic.h>`; vendored XXH3 (value hash) and portable BLAKE3 (content hash), provisional until the Phase 2 gate (D-37). No other library. |
| **Storage** | Files only: the `programs/` and `docs/spec/` inputs, generated test files in a temporary directory, harness reports. |
| **Testing** | CTest driving `quux-test` (program, examples and unit modes) and the smoke tests; sanitizer presets; valgrind in the container and on the CI runner; `ctest --repeat until-fail:20` under TSan (D-46). |
| **Target Platform** | macOS arm64 (local), Linux arm64 (Ubuntu 26.04 container, local), Linux x86_64 (CI only; CI runs nothing else) (D-28, D-30, D-31). |
| **Project Type** | Language implementation: interpreter, REPL and test tools (command-line executables). |
| **Performance Goals** | None beyond finishing every program within its declared wall-clock bound (spec Assumptions). |
| **Constraints** | Constitution I–VIII; no garbage collection yet, but the root convention is followed and checked (D-34); full-word identifiers with the `quux_` prefix; every public quux name approved in `docs/naming.md`. |
| **Scale/Scope** | 12 programs, about 200 specification examples `[ESTIMATE]`, 17 module directories under `source/`, 3 targets × 4 presets. |
| **Depends on** | Feature 001 (D-01..D-28, `docs/spec/`, `programs/`, contracts), constitution v1.2.3. |

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Status |
|---|---|---|
| I. C only | All code in C (gnu17); GNU extensions behind the portability header (D-48); container and CI scripts are shell, which is build tooling, not implementation. | PASS |
| II. Immutability and canonical structures | Canonical CHAMP and vector tries with cached node hashes (D-36); mutable cells only in futures, agents, streams and namespaces, behind the write barrier (D-34). | PASS |
| III. Structured concurrency | Futures, scopes, cancellation and propagation implemented as specified (D-40..D-43); actors not implemented (D-24). | PASS |
| IV. Design before performance | Blocking runtime first; the simulator validates distribution decisions before any network code (D-45). | PASS |
| V. Small verifiable milestones; sanitizers as gates | Toolchain smoke test is the first task; every story ends in tests that run; ASan+UBSan, TSan, LSan and valgrind are CI gates; the project can stop after any story. | PASS |
| VI. No durations | Only S/M/L sizes and dependency order. | PASS |
| VII. Evidence and traceability | D-29..D-53 recorded; version facts checked on the web with links on 2026-11-25; "to verify" items are confirmed by the smoke test and written to `docs/toolchain-record.md`. | PASS |
| VIII. Readable names | C identifiers in full words with `quux_`, checked by clang-tidy and `quux-name-check` (D-47); new quux names (D-39 map pattern, D-50, D-51, report keys, message kinds, tool names) were approved by the owner in `docs/naming.md` section J on 2026-11-25. | PASS |

Post-design re-check: PASS. The design adds two vendored libraries (D-37). They are provisional,
are isolated behind `quux_hashing_*`, and are confirmed or replaced at the Phase 2 dependency
gate, so the gate keeps its authority.

## Phase Template (from `docs/planning-prompts/00-base.md`)

### 1. Objective and main question

Can the specification of feature 001 be implemented exactly as written, so that all 12
reference programs pass as automated tests on all three targets, without sanitizer errors, while
following a root convention and module boundaries that the Phase 3 VM can inherit?

### 2. Deliverables and stopping point

**Deliverables**

- The C source tree (`source/`), the tools (`quux`, `quux-test`, `quux-name-check`), CMake presets,
  the container (`tools/container/`) and the CI workflow (`.github/workflows/`).
- Test suites: smoke tests, C unit tests, the specification example suite, and the 12 programs.
- `docs/toolchain-record.md`: every container, CI and sanitizer adjustment with its reason
  (FR-004, SC-008).
- Specification updates: `:rest` and destructuring in sections 03 and 11, kind predicates, the
  error kinds `:stack-exhausted` and `:resource-exhausted`, and any example corrected by the
  example suite (with a recorded reason).
- Updated `docs/decisions.md` (D-29..D-53) and `docs/hypotheses.md` (H1, H2, H7, H9, H10, H11).

**Stopping points** (one per story, in order):

1. After the smoke test: a verified, reproducible toolchain on three targets.
2. After the core: a working sequential quux with a REPL. It is useful as an executable form of
   the specification.
3. After futures and scopes: structured concurrency demonstrated by programs 01, 02, 03, 08 and
   09.
4. After agents, streams and data parallelism: the complete single-node language.
5. After the simulator: distribution decisions validated by programs 11 and 12.

### 3. Task outline (dependency order)

[tasks.md](./tasks.md) (generated by `/speckit-tasks`) is authoritative. This outline fixes the
order and the sizes.

| ID | Size | Task |
|---|---|---|
| T1.1 | M | Toolchain smoke test: CMake presets, a clean program, and three deliberate defects; run on macOS, in the container and in CI; write `docs/toolchain-record.md` (D-29..D-31). |
| T1.1b | S | `tools/local-check.sh`, the pre-push hook and `tools/install-git-hooks.sh` (D-53). |
| T1.2 | S | Portability layer and its unit tests (D-48), including the monotonic timed wait on macOS. |
| T1.3 | M | Memory module: allocation, root frames, global roots, write barrier, root verifier (D-34). |
| T1.4 | M | Values, symbols, keywords, strings, numbers with overflow checks, errors (D-35, D-51). |
| T1.5 | L | Collections: lists, vector trie, CHAMP maps and sets with canonical removal and cached node hashes; vendored XXH3 and BLAKE3 behind `quux_hashing_*` (D-36, D-37). |
| T1.6 | M | Reader and printer (no shorthands, canonical printing). |
| T1.7 | M | Expander: `define-macro`, `quasi-quote`, `unique-symbol`, `:rest`, vector and map patterns (D-39). |
| T1.8 | L | Evaluator with the tail-call loop, core special forms, core functions, `try`/`catch`/`finally`, namespaces and content hashes (D-38, D-19). |
| T1.9 | S | REPL and file runner (D-52). |
| T1.10 | M | Example suite (`quux-test examples`) and fixes to examples that the suite finds wrong. |
| T1.11 | M | Harness: program mode, conditions, assertions, watchdog, report (D-46); name check (D-47). |
| T1.12 | L | Executor interface and blocking executor; futures, scopes, cancellation, propagation, deadlock graph (D-32, D-40). |
| T1.13 | M | Combinators and `let-parallel` (section 08); programs 01, 02, 03, 08, 09 pass. |
| T1.14 | M | Agents with `update`, `transact`, `update-once` (D-41); program 04 passes. |
| T1.15 | M | Streams with `:maximum-ahead`, ownership counters for H11 (D-42); programs 06 and 07 pass. |
| T1.16 | M | Input/output appendix with the wake-up pipe (D-44); program 10 passes. |
| T1.17 | S | `parallel-transform`, `parallel-reduce`, `fork-join` with the fixed tree (D-43); program 05 passes. |
| T1.18 | M | Encoding module (canonical binary encoding, serialization classes, `:not-serializable`). |
| T1.19 | L | Simulator: nodes, network thread, drops, crashes, remote references, `spawn-on` (D-45); programs 11 and 12 pass. |
| T1.20 | S | Closing: 20-run TSan loop for every concurrent program on all targets; update decisions, hypotheses and the roadmap's "Starting reality" for 003. |

### 4. Acceptance criteria

Each criterion is an observable command (see [quickstart.md](./quickstart.md)).

- Smoke test: `ctest -L smoke` passes in every preset on macOS, in the container and in CI;
  each deliberate defect is reported by its tool (spec story 1).
- `quux-test examples docs/spec` reports 100% of examples passing (SC-004).
- `quux-test program programs/NN-name` reports every assertion as held for all 12 programs on
  all three targets (SC-001).
- `ctest --preset tsan -L concurrent --repeat until-fail:20` passes (SC-003).
- With the hooks installed, a push with a failing arm64 test is blocked (D-53).
- No sanitizer report and no leak in any CI job (SC-002); every suppression listed in
  `docs/toolchain-record.md` (SC-008).
- `quux-name-check` passes (SC-009).
- `docs/hypotheses.md` has a status with evidence for H1, H2, H7, H9, H10 and H11 (SC-007).

### 5. Dependencies on earlier phases

- Feature 001: D-01..D-28, `docs/spec/`, `programs/`, the reference-program, core-forms and
  spec-section contracts, `docs/naming.md`.
- Constitution v1.2.3 (platform table, presets, sanitizers).

### 6. Technical risks and mitigation

| # | Risk | Probability | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **x86_64 is tested only in CI, with no local reproduction** (D-28) | High `[ESTIMATE]` that at least one CI-only failure occurs | Medium | Tests addressable by name; seeds printed; `workflow_dispatch` re-runs one test N times on one runner; failing runs upload logs and harness reports (D-31). |
| R2 | **Memory-ordering bugs masked on x86 TSO**: code that is wrong on arm64 passes on x86_64, and code that is wrong on both only shows on the M2 | Medium `[ESTIMATE]` | High | Develop and run TSan on arm64 first (locally, on the M2 and in the container); all shared state through `<stdatomic.h>` with explicit orders behind the portability layer; no hand-written fences outside it; 20-run TSan loops locally on arm64 and in CI on x86_64. |
| R3 | TSan or LSan do not work inside Docker on the M2 (address randomization, seccomp, ptrace) | Medium `[ESTIMATE]` | Medium | Adjustments of D-30 tried in order and recorded; the CI job runs on the runner without Docker; if the container cannot run a sanitizer, Linux arm64 keeps the other presets plus valgrind, and the gap is recorded. |
| R4 | Thread-per-future exhausts threads or memory in programs 05 and 10 | Medium `[ESTIMATE]` | Medium | Fixed chunk size (D-43) bounds futures; explicit small stacks; `:resource-exhausted` instead of a crash; the harness reports it. |
| R5 | A specification example or program turns out wrong or ambiguous when executed | High `[ESTIMATE]` | Low | Fix the document with a recorded decision; never weaken a test silently (spec Assumptions). |
| R6 | The root verifier cannot check concurrent code, so a missing root in concurrent paths survives until Phase 3 | Medium `[ESTIMATE]` | High | Verify every sequential path; keep the concurrency module's heap handling small and reviewed; plan concurrent verification with safepoints in Phase 3 (D-34). |
| R7 | Timing-based assertions are flaky on slow CI runners | Medium `[ESTIMATE]` | Medium | Assertions use generous bounds with 2× margin (contract); CI failures re-run by name; if a bound is still flaky, widen it in `conditions.qx` with a recorded reason. |
| R8 | Deadlock-detection lock becomes a contention point or causes lock-order inversions | Low `[ESTIMATE]` | Medium | One global lock taken before any task lock, never the reverse; documented lock order; TSan catches inversions. |
| R9 | The simulator diverges from real network behavior and gives false confidence | Medium `[ESTIMATE]` | Medium | All interactions are encoded messages (D-45); the differences (immediate crash detection, no deadlines) are listed for Phase 5a/5b. |
| R10 | Vendored hash libraries hide a portability or license problem | Low `[ESTIMATE]` | Low | Portable BLAKE3 only; licenses and versions recorded in `third_party/`; confirmed at the Phase 2 gate. |
| R12 | arm64 regressions reach `main` because CI runs only on x86_64 (D-31) | Low `[ESTIMATE]` with the hook | Medium | A pre-push git hook runs every preset on macOS and in the container and blocks the push on failure (D-53); the 20-run TSan loop runs at story checkpoints. |
| R13 | The pre-push check becomes slow enough that it is bypassed | Medium `[ESTIMATE]` | Medium | Documentation-only pushes skip the build; builds are incremental; the log shows the slowest preset so the suite can be split if needed (D-53). |
| R11 | Pending name approvals (D-39, D-50, D-51) block tasks | Medium `[ESTIMATE]` | Low | Proposals are entered early (first task after the smoke test), so the owner reviews them in one batch. |

### 7. Design decisions closed

D-29..D-53, recorded in [research.md](./research.md) and indexed in `docs/decisions.md`. Inherited
by later phases: D-32, D-33, D-34, D-35, D-38 (tail positions), D-40 (wait-for graph semantics),
D-41, D-42 by Phase 3; D-44 and D-48 by Phase 4; D-45 and D-37 by Phase 5; D-46, D-47, D-52 by
Phase 7.

## Project Structure

### Documentation (this feature)

```text
specs/002-blocking-prototype/
├── plan.md              # This file
├── research.md          # D-29..D-53
├── data-model.md        # Runtime entities and their C-level fields
├── quickstart.md        # How to build and run each acceptance check
├── contracts/
│   ├── command-line.md      # quux, quux-test, quux-name-check
│   ├── module-interface.md  # module headers, reuse tags, executor interface, C conventions
│   ├── root-convention.md   # root frames, global roots, write barrier, verifier
│   ├── simulator.md         # nodes, messages, drops, crashes
│   └── harness-report.md    # report format of quux-test
├── checklists/requirements.md
└── tasks.md             # Generated by /speckit-tasks
```

### Source Code (repository root)

```text
CMakeLists.txt
CMakePresets.json            # debug, asan-ubsan, tsan, release
.clang-format
.clang-tidy
source/
├── portability/             # quux_portability.h/.c (D-48)
├── memory/                  # allocation, roots, write barrier, verifier (D-34)
├── values/                  # tagged values, symbols, numbers, errors (D-35)
├── collections/             # list, vector trie, champ map and set (D-36)
├── hashing/                 # value and content hash wrappers (D-37)
├── reader/
├── printer/
├── expander/                # macros, quasi-quote, :rest, patterns (D-39)
├── evaluator/               # tree walker with tail-call loop (D-38)
├── namespaces/              # names → content hashes (D-19, D-23)
├── encoding/                # canonical binary encoding (D-45)
├── concurrency/             # futures, scopes, agents, streams, parallelism (D-40..D-43)
├── executor_blocking/       # thread per task (D-40)
├── input_output/            # files, listeners, connections (D-44)
├── simulation/              # nodes and network (D-45)
├── harness/                 # program runner, assertions, example extractor (D-46)
└── main/                    # quux.c, quux_test.c, quux_name_check.c
third_party/
├── xxhash/                  # vendored, with LICENSE and version
└── blake3/                  # portable C only, with LICENSE and version
tests/
├── smoke/                   # clean program + heap overflow, data race, leak
└── unit/                    # one file per module
tools/
├── container/               # Dockerfile, build-image.sh, run-preset.sh
├── git-hooks/pre-push       # runs local-check.sh before every push (D-53)
├── install-git-hooks.sh     # sets core.hooksPath to tools/git-hooks
├── local-check.sh           # every preset on macOS arm64 and in the Linux arm64 container
└── ci/                      # helpers called by the workflow
.github/workflows/build-and-test.yml
docs/toolchain-record.md
```

**Structure Decision**: a single CMake project. Each directory under `source/` is a static library
with one public header, so module dependencies are explicit in CMake and the reuse tags of D-32
are visible in the build graph. The three executables link only the libraries they need, and
`quux-test` adds `harness` and `simulation`.

## Complexity Tracking

No constitution violation needs justification. The one point to watch: two vendored libraries
before the Phase 2 dependency gate (D-37). They are reversible, isolated behind one module, and
the gate confirms or replaces them.
