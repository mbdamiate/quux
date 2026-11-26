---

description: "Task list for feature 002: quux blocking prototype (Phase 1)"
---

# Tasks: quux Blocking Prototype (Phase 1)

**Input**: Design documents from `/specs/002-blocking-prototype/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md),
[data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

**Tests**: requested by the spec. The tests are the smoke tests (story 1), C unit tests per
module, the specification example suite (FR-026), and the 12 reference programs (FR-024). Within a
story, the test tasks come first and must fail before the code that makes them pass.

**Organization**: tasks are grouped by user story (US1–US5 = spec stories 1–5) so that each story
ends in something that runs.

**Conventions**:

- C code follows [contracts/module-interface.md](./contracts/module-interface.md) and
  [contracts/root-convention.md](./contracts/root-convention.md). Every quux name comes from
  [docs/naming.md](../../docs/naming.md) with status `approved`. If a task needs a name that is
  not approved, it adds the name as `proposed` and stops until the owner approves it.
- Sizes are relative (S, M, L), per constitution Principle VI. There are no durations.
- Each story checkpoint runs `tools/local-check.sh` (macOS arm64 and the Linux arm64 container)
  and the CI job on Linux x86_64 (D-31, D-53).
- Example sections and programs that do not pass yet carry the CTest label `pending`, which the
  pre-push hook and CI skip. The task that makes one pass moves it from `pending` to its active
  labels (`examples` or `programs`, plus `concurrent`) in the same change (D-53).

## Format: `[ID] [P?] [Story] Description (size)`

- **[P]**: can run in parallel (different files, no dependency on unfinished tasks)
- **[Story]**: the user story the task belongs to (US1–US5)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: the build skeleton that every story uses.

- [X] T001 Create the top-level `CMakeLists.txt` with `-std=gnu17`, warnings as errors in `debug`, `compile_commands.json` export, one static library per `source/<module>/` directory (empty libraries allowed), and the module dependency direction of contracts/module-interface.md rule 3 (M)
- [X] T002 Create `CMakePresets.json` with configure, build and test presets `debug`, `asan-ubsan`, `tsan`, `release` (Ninja generator; compiler taken from `QUUX_C_COMPILER`, default clang 22 per D-29) (S)
- [X] T003 [P] Add `.clang-format` and `.clang-tidy` (including `readability-identifier-naming` with the `quux_` prefix and snake_case, D-47) (S)
- [X] T004 [P] Create `docs/toolchain-record.md` with the table of data-model.md "Toolchain record" (fields target, preset, tool, adjustment, reason, evidence, date) (S)
- [X] T005 [P] Extend `.gitignore` with `build/`, `build-*/` and `build/local-check.log` (S)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: decisions that must be settled before code uses them.

- [X] T006 Owner review of the `proposed` names in `docs/naming.md` section J (J2 map pattern, J3 kind predicates, J4 error kinds, J5 report keys, J6 message kinds, J7 tool names); record each answer as `approved` or `rejected`, and update D-39, D-50 and D-51 in `docs/decisions.md` and research.md (S)
- [X] T007 Update `docs/spec/03-evaluation-and-tail-calls.md` (`:rest`, vector and map patterns, `:stack-exhausted`), `docs/spec/11-data-parallelism.md` (destructuring now specified) and `docs/spec/12-errors.md` (`:stack-exhausted`, `:resource-exhausted`), closing W4, W5 and W6 in `docs/spec/open-questions.md`; depends on T006 (M)

**Checkpoint**: names settled; US1 can start in parallel with T006/T007, and US2 needs them.

---

## Phase 3: User Story 1 — Toolchain smoke test on every target (Priority: P1) 🎯 MVP

**Goal**: identical builds in four presets on macOS arm64, Linux arm64 (container) and Linux x86_64
(CI), with every sanitizer proven to report a deliberate defect.

**Independent Test**: quickstart §1 and §5: `ctest -L smoke` in every preset on each target; each
defect test passes only because its tool reported the defect.

### Tests for User Story 1

- [X] T008 [P] [US1] Write `tests/smoke/clean_program.c` (threads, atomics, a condition variable and a heap allocation, all correct) and register it with label `smoke` in `tests/smoke/CMakeLists.txt` (S)
- [X] T009 [P] [US1] Write `tests/smoke/heap_overflow.c`, `tests/smoke/data_race.c` and `tests/smoke/memory_leak.c`, registered as `smoke` tests that pass only when the expected tool reports the defect (CTest `WILL_FAIL` or a pass-regular-expression on the sanitizer report), each enabled only in the preset or tool that detects it (M)

### Implementation for User Story 1

- [X] T010 [US1] Run the smoke suite in the four presets on macOS arm64 with Homebrew `llvm@22`, plus Apple clang `debug`; record results and any adjustment in `docs/toolchain-record.md` (S)
- [X] T011 [P] [US1] Write `tools/container/Dockerfile` (Ubuntu 26.04 arm64; clang-22, gcc, cmake, ninja-build, valgrind, clang-format-22, clang-tidy-22) and `tools/container/build-image.sh` (D-30) (S)
- [X] T012 [US1] Write `tools/container/run-preset.sh` (source mounted at `/work`, build tree in a named volume) and `tools/ci/run-valgrind.sh`; run the smoke suite in the container, find which adjustments TSan and LSan need (`SYS_PTRACE`, seccomp profile allowing `personality`, `vm.mmap_rnd_bits` in the Docker VM), apply the minimal set, and record each in `docs/toolchain-record.md` (M)
- [X] T013 [P] [US1] Write `.github/workflows/build-and-test.yml`: one `ubuntu-26.04` x86_64 job, four presets, gcc `debug` build, valgrind on `debug`, artifacts on failure, and a `workflow_dispatch` input to re-run one test N times (D-31); verify the runner label (S)
- [X] T014 [US1] Write `tools/local-check.sh` (every preset on macOS and in the container; labels per D-53, excluding `pending`; `--repeat N`; documentation-only fast path; a preset that `docs/toolchain-record.md` records as unavailable in the container is skipped with a visible warning instead of blocking; summary lines and `build/local-check.log`), `tools/git-hooks/pre-push` and `tools/install-git-hooks.sh` (M)
- [X] T015 [US1] Push and confirm CI is green on Linux x86_64; confirm a deliberately failing local test blocks the push (spec story 1 scenarios 5 and 7); record outcomes in `docs/toolchain-record.md` (S)

**Checkpoint**: stopping point 1 — a verified toolchain on three targets.

---

## Phase 4: User Story 2 — Core language and REPL (Priority: P1)

**Goal**: a sequential quux that runs every `;=>` / `;!!` example of `docs/spec/`, with a REPL.

**Independent Test**: quickstart §2: REPL session; `ctest -L "unit|examples"`; the example suite
under `--verify-roots`; one million tail calls with bounded stack.

### Tests for User Story 2

- [X] T016 [P] [US2] Write the minimal assertion macros in `tests/unit/quux_unit_test.h` and the module test table linked into `quux-test`, so that `quux-test unit [MODULE]` runs them (D-46, contracts/command-line.md); register one CTest per module with label `unit` (S)
- [X] T017 [P] [US2] Write `tests/unit/test_portability.c`: thread start/join with stack size, monotonic timed wait, page size at run time, 128-bit random identifiers (S)
- [X] T018 [P] [US2] Write `tests/unit/test_memory.c`: root frames balanced, unclosed frame detected, verifier poisons an unregistered value, allocation failure under a test-only allocation limit returns `:resource-exhausted`, and `quux_memory_shutdown` leaves nothing for LSan to report (S)
- [X] T019 [P] [US2] Write `tests/unit/test_values.c`: small and boxed integers, `:overflow` on 64-bit overflow, float boxing, interned symbols and keywords, error values (S)
- [X] T020 [P] [US2] Write `tests/unit/test_collections.c`: vector trie across level boundaries; CHAMP insertion-order independence of structure, printed form and value hash; canonical form after removals; canonical collision nodes (M)
- [X] T021 [P] [US2] Write `tests/unit/test_reader_printer.c` and `tests/unit/test_expander.c` (round trips; no shorthands accepted; `quasi-quote` qualification; `unique-symbol`; `:rest`; vector and map patterns lowered to core forms) (M)
- [X] T022 [P] [US2] Write `tests/unit/test_evaluator.c`: one million self tail calls and mutual tail calls with bounded stack; `:stack-exhausted` on deep non-tail recursion; content hash unchanged by renaming and by reordering a mutually recursive group (M)

### Implementation for User Story 2

- [X] T023 [US2] Implement `source/portability/quux_portability.h` and `.c` per D-48 (threads with stack size, mutex, condition variable with monotonic timed wait on macOS and Linux, `quux_atomic_*` with explicit orders, page size, `getentropy`, compiler attribute macros) (M)
- [X] T024 [US2] Implement `source/memory/` per contracts/root-convention.md: `quux_memory_allocate`, `QUUX_ROOT_FRAME`/`QUUX_ROOT_FRAME_END` over data-model.md `Root frame.previous`, `Root frame.count`, `Root frame.slots`; global roots; `quux_memory_write_barrier`; the verifier using data-model.md `Heap object header.flags` (`verifier_mark`); allocation failure returns `false` with `:resource-exhausted` (FR-017); `quux_memory_shutdown`, which frees every allocation when the runtime ends so LSan and valgrind report only real leaks (D-34 amendment, SC-002) (M)
- [X] T025 [US2] Implement `source/values/` per D-35: tagged `quux_value`, heap header with data-model.md `Heap object header.kind`, `Heap object header.flags`, `Heap object header.value_hash`, `Heap object header.content_hash`; symbols, keywords, strings, integers with `__builtin_*_overflow`, floats, errors; and `quux_context` with data-model.md `Context.task`, `Context.root_frames`, `Context.current_scope`, `Context.current_namespace`, `Context.current_node`, `Context.pending_error`, `Context.recursion_depth` (M)
- [X] T026 [US2] Vendor XXH3 in `third_party/xxhash/` and portable BLAKE3 in `third_party/blake3/` with LICENSE files and exact versions (verify xxHash's license), and implement `source/hashing/` (`quux_hashing_value`, `quux_hashing_content`) per D-37 (S)
- [X] T027 [US2] Implement `source/collections/`: lists, the 32-way vector trie with tail, CHAMP maps and sets with canonical removal, canonical collision nodes and cached node value hashes (D-36) (L)
- [X] T028 [US2] Implement `source/reader/` and `source/printer/`: section 02 syntax, no reader shorthands, canonical printing of maps and sets (M)
- [X] T029 [US2] Implement `source/expander/`: `define-macro`, `quasi-quote`, `unquote`, `unquote-splicing`, `unique-symbol`, `:rest` and pattern lowering (D-39) (M)
- [X] T030 [US2] Implement `source/evaluator/`: the tail-call loop (D-38), core special forms of `specs/001-language-spec/contracts/core-forms.md`, `try`/`catch`/`finally`, error values with data-model.md `Context.pending_error`, depth limit via `Context.recursion_depth` (L)
- [X] T031 [US2] Implement the sequential core functions (numerics of section 05, collections of section 02, strings, printing, `function-hash`, the kind predicates approved in T006) in `source/evaluator/quux_core_functions.c` (L)
- [X] T032 [US2] Implement `source/namespaces/` (names → content hashes, SCC group hashing per section 13 rules 1–5, global roots) and `source/encoding/` (canonical binary encoding used by `quux_hashing_content`) (M)
- [X] T033 [US2] Implement `source/main/quux.c`: REPL and file runner per contracts/command-line.md (D-52) (S)
- [X] T034 [US2] Write `tests/repl/repl_session_input.txt` and `tests/repl/repl_session_expected.txt`, and register a CTest (label `unit`) that feeds it to `quux` on standard input and checks the printed values, the `!! <kind>: <message>` error line and that the session continues after an error (FR-013) (S)
- [X] T035 [US2] Implement the examples mode of `quux-test` in `source/harness/quux_examples.c` and `source/main/quux_test.c` (extract ```` ```quux ```` blocks, pair forms with `;=>` / `;!!`), register one CTest per section with label `examples`, except sections that need US3–US5, which start with label `pending` (M)
- [X] T036 [US2] Run the sequential examples (sections 01–05, 12 sequential part, 14) with and without `--verify-roots`; fix code or, where an example is wrong, fix `docs/spec/` with a recorded reason in `docs/spec/open-questions.md` (M)
- [X] T037 [P] [US2] Implement `source/main/quux_name_check.c` per D-47 and contracts/command-line.md, registered with label `names`; fix every offending name it finds (M)

**Checkpoint**: stopping point 2 — a working sequential quux. Run `tools/local-check.sh` and CI.

---

## Phase 5: User Story 3 — Futures and scopes (Priority: P2)

**Goal**: structured concurrency on the blocking runtime; programs 01, 02, 03, 08 and 09 pass.

**Independent Test**: quickstart §3 for those five programs, plus 20 TSan runs each.

### Tests for User Story 3

- [X] T038 [P] [US3] Write `tests/unit/test_concurrency.c`: single state transition under a race, scope join, cancellation at `wait-for` and `sleep`, `shield-from-cancellation`, primary and suppressed errors, a two-task deadlock raising `:deadlock` with `:cycle` (M)
- [X] T039 [US3] Implement the program mode of `quux-test` in `source/harness/`: parse `conditions.qx` and `expected.qx` with the reader, reject unknown keys and assertion forms (FR-025), `tag` with delays and failures, watchdog on `:bounds`, report per contracts/harness-report.md with data-model.md `Program run.program`, `Program run.conditions`, `Program run.assertions`, `Program run.outcome`, `Program run.tag_events`, `Program run.elapsed_milliseconds`, `Program run.primary_error_raced`; register the 12 programs with label `pending`; each later task moves a program to `programs` (and `concurrent`) when it passes (L)
  - **Note from feature 003 (research risk R1):** `programs/ergonomics/` holds the ergonomics programs, which have their own CTest registration (`tests/ergonomics/`) and no `conditions.qx`. The loop that registers the reference programs must list the 12 programs or match `programs/[0-9][0-9]-*`, so that it does not pick up `programs/ergonomics/`.
- [X] T040 [P] [US3] Write `tests/unit/test_harness.c`: an unknown key and an unknown assertion are rejected; the watchdog stops a non-terminating program with exit status 3 (S)

### Implementation for User Story 3

- [X] T041 [US3] Define the executor interface in `source/concurrency/quux_executor.h` per contracts/module-interface.md (S)
- [X] T042 [US3] Implement `source/executor_blocking/` (D-40): data-model.md `Task.thread`, `Task.mutex`, `Task.condition`, `Task.cancellation_requested`, `Task.shield_depth`, `Task.waiting_for`, `Task.future`; explicit stack size; `:resource-exhausted` on thread failure (M)
- [X] T043 [US3] Implement futures and scopes in `source/concurrency/`: data-model.md `Future.state` (one transition, compare-and-swap), `Future.value`, `Future.error`, `Future.scope`, `Future.computing_task`, `Future.waiters`, `Future.tag_names`; `Scope.identifier` (128 random bits), `Scope.state`, `Scope.parent`, `Scope.children`, `Scope.primary_error`, `Scope.suppressed`; current scope per section 06 rule 3 (L)
- [X] T044 [US3] Implement cancellation points, `cancel!`, `check-cancellation`, `shield-from-cancellation`, `finally` under cancellation, and the global wait-for graph with `:deadlock` (D-14, D-40) in `source/concurrency/` (M)
- [X] T045 [US3] Implement the combinators `all`, `first-success`, `then`, `with-timeout`, `let-parallel` (section 08; combinator cancellations do not fail the scope, D-11 amendment) in `source/concurrency/quux_combinators.c` (M)
- [ ] T046 [US3] Make programs 01, 02, 03, 08 and 09 pass on macOS; then run each 20 times under TSan locally and in CI; fix races, and record any spec or program correction with its reason (M)

Notes from the implementation of US3:

- Programs 08 and 09 need pieces that this list places in US4: `fork-join` (T054), the
  `same-as-sequential` assertion (T050) and agents (T051). US3 implements them as far as those
  two programs need: `fork-join` in `source/concurrency/quux_parallelism.c`;
  `same-as-sequential` in the harness; `agent`, `state`, `update` and `transact` in
  `source/concurrency/quux_agents.c` with a ticket order instead of D-41's queue and worker, and
  without the agent's own scope, `update-once` or the deadlock of section 07 rule 18. T050, T051
  and T054 do the rest.
- T040's watchdog check is the CTest test `harness_never-ends` (exit status 3 ends the process,
  so a unit test cannot observe it); `tests/unit/test_harness.c` checks the rejections.
- The example suite found a contradiction between section 07 rule 5 and the `first-success`
  example of section 08 (D-80, W29), and the unit tests found that a shielded agent update was
  withdrawn (D-81, W30). Both are recommended decisions for the owner to confirm. Corrected
  examples and their reasons: `docs/spec/open-questions.md`, "Example corrections in feature
  002, User Story 3".

**Checkpoint**: stopping point 3 — structured concurrency demonstrated.

---

## Phase 6: User Story 4 — Agents, streams and data parallelism (Priority: P3)

**Goal**: the complete single-node language; programs 04, 05, 06, 07 and 10 pass, and 08 still
passes.

**Independent Test**: quickstart §3 for those programs, plus the remaining concurrency examples of
sections 09–11 and the appendix.

### Tests for User Story 4

- [ ] T047 [P] [US4] Write `tests/unit/test_agents.c`: one update at a time, failed update keeps state, `transact`, `update-once` applied once, queued update withdrawn on sender cancellation (S)
- [ ] T048 [P] [US4] Write `tests/unit/test_streams.c`: `:maximum-ahead` respected, producers cancelled when the owner scope's body returns, foreign advances counted (S)
- [ ] T049 [P] [US4] Write `tests/unit/test_input_output.c`: a blocked read is cancelled immediately through the wake-up pipe; a closed resource raises `:resource-closed` (S)

### Implementation for User Story 4

- [ ] T050 [US4] Extend the harness with `agent-final`, `never`, `maximum-ahead`, `output-lines`, `same-as-sequential`, `deterministic` and `:files` generation, using data-model.md `Program run.agent_commits`, `Program run.stream_ahead`, `Program run.printed_lines` (M)
- [ ] T051 [US4] Implement agents in `source/concurrency/quux_agents.c` (D-41): data-model.md `Agent.state`, `Agent.queue`, `Agent.worker_running`, `Agent.applied_operations`, `Agent.agent_scope`, `Agent.owner_node`; program 04 passes (M)
- [ ] T052 [US4] Implement streams in `source/concurrency/quux_streams.c` (D-42): data-model.md `Stream cell and producer.first`, `Stream cell and producer.rest`, `Stream cell and producer.producer`, `Stream cell and producer.produced_count`, `Stream cell and producer.demanded_count`, `Stream cell and producer.foreign_advances`; data-model.md `Scope.owned_streams`; `stream-transform`, `stream-filter`, `stream-take`, `stream-reduce`; program 07 passes (M)
- [ ] T053 [US4] Implement `source/input_output/` (D-44): files with `file-lines`, `open-file`, `read-line`, `write-line`; `listen`, `accept`, `connect`; data-model.md `Task.wake_up_pipe`; `local-resource` flag; program 06 passes (M)
- [ ] T054 [US4] Implement `parallel-transform`, `parallel-reduce` and `fork-join` with the fixed chunk size and reduction tree (D-43) in `source/concurrency/quux_parallelism.c`; program 05 passes, program 08 still passes (S)
- [ ] T055 [US4] Make program 10 pass; run programs 04–07 and 10 20 times under TSan locally and in CI; record the H11 counts from the reports in `docs/hypotheses.md` (M)

**Checkpoint**: stopping point 4 — the complete single-node language.

---

## Phase 7: User Story 5 — Multi-node simulation (Priority: P4)

**Goal**: programs 11 and 12 pass against simulated nodes, with serialization exercised for real.

**Independent Test**: quickstart §3 for programs 11 and 12; the `:not-serializable` scenario.

### Tests for User Story 5

- [ ] T056 [P] [US5] Write `tests/unit/test_encoding.c`: round trip of every by-value kind; remote references encoded as node + identifier; a captured file fails with `:not-serializable` naming the kind and path (S)
- [ ] T057 [P] [US5] Write `tests/unit/test_simulation.c`: latency ordering, the n-th matching message dropped, `:transport` on drop, `:node-unreachable` after a crash, crashed node's results never delivered (M)

### Implementation for User Story 5

- [ ] T058 [US5] Extend `source/encoding/` with serialization classes (D-20): by value, by remote reference, local-only; `serializable?` (M)
- [ ] T059 [US5] Implement simulated nodes in `source/simulation/` per contracts/simulator.md: data-model.md `Simulated node.name`, `Simulated node.root_scope`, `Simulated node.crashed`, `Simulated node.completed_spawn_count`, `Simulated node.crash_after_completed`, `Simulated node.remote_table`; data-model.md `Scope.owner_node` (M)
- [ ] T060 [US5] Implement the network thread and messages: data-model.md `Network message.from`, `Network message.to`, `Network message.message_kind`, `Network message.payload`, `Network message.delivery_time`, `Network message.correlation`; latency and drop rules from `conditions.qx` (M)
- [ ] T061 [US5] Implement `spawn-on`, `node`, remote futures and remote agents (including `update-once` across nodes), `:remote` with `:cause`, the code store with fetch counting, and data-model.md `Future.remote` (L)
- [ ] T062 [US5] Extend the harness with `:nodes`, `:network`, `applied-once` and data-model.md `Program run.applied_operations`; make programs 11 and 12 pass, and run both 20 times under TSan locally and in CI (M)

**Checkpoint**: stopping point 5 — distribution decisions validated in simulation.

---

## Phase 8: Polish & Cross-Cutting Concerns

- [ ] T063 Run the whole example suite (all sections, including concurrency) and all 12 programs on all three targets in all presets; zero sanitizer reports and zero leaks (SC-001, SC-002, SC-004); sum the `:bounds` of the 12 programs and compare with the measured total per target and preset (SC-006); no test is left with label `pending` (M)
- [ ] T064 [P] Review every sanitizer suppression and container or CI adjustment in `docs/toolchain-record.md`; none unexplained (SC-008) (S)
- [ ] T065 [P] Write the reuse evidence for H1 (which modules kept their reuse tag, which changed) and close H2 and H7 with evidence in `docs/hypotheses.md`; record H9 race counts and H11 counts (SC-007) (S)
- [ ] T066 [P] Update `docs/decisions.md` for any decision changed during implementation, and `docs/spec/open-questions.md` for anything found while running the programs (S)
- [ ] T067 Fill the "Starting reality" of feature 003 in `docs/speckit-roadmap.md`: smoke-test results, container adjustments, components kept or discarded, where it hurt, libraries tried, compilers used, divergences from this plan; mark 002 done (S)
- [ ] T068 Run [quickstart.md](./quickstart.md) end to end and record the results in `specs/002-blocking-prototype/checklists/requirements.md` (S)

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (T001–T005)**: no dependencies.
- **Foundational (T006–T007)**: T007 depends on T006. Neither blocks US1.
- **US1 (T008–T015)**: depends on Setup only.
- **US2 (T016–T037)**: depends on Setup and on T006 (names). T036 depends on T007.
- **US3 (T038–T046)**: depends on US2 (values, evaluator, examples mode for the harness reader).
- **US4 (T047–T055)**: depends on US3 (futures, scopes, harness program mode).
- **US5 (T056–T062)**: depends on US3; T061 also needs agents from T051.
- **Polish (T063–T068)**: depends on every story it reports on.

### Story completion order

```text
Setup → US1 ─────────────────────────────┐
Setup → T006 → US2 → US3 → US4 ──────────┼→ Polish
                     US3 → (T051) → US5 ─┘
```

US1 is independent of the rest and goes first because every later checkpoint relies on its scripts
and CI. US4 and US5 can proceed in parallel once US3 is done, except T061, which waits for T051.

### Within each story

Tests first (they must fail), then the module in dependency order of
contracts/module-interface.md rule 3, then the programs, then the TSan runs.

## Parallel Examples

### User Story 1

```text
T008 clean_program.c            T009 three defect programs
T011 Dockerfile                 T013 CI workflow
```

### User Story 2

```text
T017 test_portability.c   T018 test_memory.c   T019 test_values.c
T020 test_collections.c   T021 reader/printer/expander tests   T022 test_evaluator.c
T037 quux-name-check (independent of the evaluator)
```

### User Story 4

```text
T047 test_agents.c   T048 test_streams.c   T049 test_input_output.c
```

### User Story 5

```text
T056 test_encoding.c   T057 test_simulation.c
```

## Implementation Strategy

### MVP first

1. Setup (T001–T005), then US1 (T008–T015). **Stop and validate**: three targets, four presets,
   sanitizers proven, pre-push hook working. This is the MVP: a verified toolchain.
2. T006 (owner name review), then US2. **Stop and validate**: the sequential language and its
   examples.

### Incremental delivery

3. US3: programs 01, 02, 03, 08, 09.
4. US4: programs 04, 05, 06, 07, 10.
5. US5: programs 11, 12.
6. Polish: evidence for the hypotheses and the starting reality of feature 004 (dependency gate).

Each stopping point leaves something that runs (plan §2), and each ends with `tools/local-check.sh`
and a green CI job.
