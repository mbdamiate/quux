---

description: "Task list for feature 001: quux language specification by example (Phase 0)"
---

# Tasks: quux Language Specification by Example (Phase 0)

**Input**: Design documents from `/specs/001-language-spec/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md),
[data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

**Tests**: this is a documents-only feature, so there are no code tests. Each phase ends with
the matching [quickstart.md](./quickstart.md) check, and the US2/US3 programs carry their own
machine-readable assertions (`expected.qx`), which feature 002 will run.

**Organization**: tasks are grouped by user story so that each story can be written and checked
on its own.

**Conventions**:

- Every public name MUST come from [docs/naming.md](../../docs/naming.md) (constitution
  Principle VIII, D-27). If a task needs a name that is not there, add it as `proposed` and stop
  until the owner approves it.
- Sizes are relative (S, M, L), per constitution Principle VI. There are no durations.
- Section files follow [contracts/spec-section.md](./contracts/spec-section.md). Program
  directories follow [contracts/reference-program.md](./contracts/reference-program.md) and use
  only the names in [contracts/core-forms.md](./contracts/core-forms.md).

## Format: `[ID] [P?] [Story] Description (size)`

- **[P]**: can run in parallel (different files, no dependency on unfinished tasks)
- **[Story]**: the user story the task belongs to (US1–US5)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: create the directory skeleton and the templates that every later task fills in.

- [X] T001 Create the directories `docs/spec/` and `programs/` at the repository root, each with a placeholder `.gitkeep` (S)
- [X] T002 [P] Create `docs/spec/_section-template.md`, copied from the skeleton in [contracts/spec-section.md](./contracts/spec-section.md), with every name taken from `docs/naming.md` (S)
- [X] T003 [P] Create `programs/_template/` with empty `main.qx`, `conditions.qx`, `expected.qx` and a `README.md` skeleton, following [contracts/reference-program.md](./contracts/reference-program.md) (S)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: the index and the register that every story links to.

**⚠️ CRITICAL**: no story work starts until this phase is complete.

- [X] T004 Write `docs/spec/00-index.md` (S). It lists the 14 topics, the question each one answers, the planned file name of each section (`01-values-and-equality.md` … `14-modules.md`), the example conventions (`;=>` and `;!! :kind`) and a pointer to `docs/naming.md`.
- [X] T005 Create `docs/spec/open-questions.md` (S). It has one row per item of spec FR-026, with the columns question, recommendation, decision or hypothesis link, and closing phase. Rows are left empty until US5 fills them.

**Checkpoint**: the skeleton is ready; US1 and US2 can proceed in parallel.

---

## Phase 3: User Story 1 — Specification skeleton with core semantics answered (Priority: P1) 🎯 MVP

**Goal**: 14 normative sections. Each one states its question, cites its D-xx and has examples.

**Independent Test**: [quickstart.md](./quickstart.md) step 1 prints nothing, and there are 14
section files.

- [X] T006 [P] [US1] Write `docs/spec/01-values-and-equality.md` (M).
  - Content: D-01 (literals; no reader shorthands), D-02 (`equal?`, `numerically-equal?`, `same-identity?`, `hash`; reflexive `not-a-number`).
  - Cites the table "Equality, hash and serialization" in `data-model.md` and data-model.md `Value` invariants.
- [X] T007 [P] [US1] Write `docs/spec/02-collections.md` (M).
  - Content: D-03 (canonical CHAMP maps and sets, trie vectors, the two hashes); constructors `list`, `vector`, `map-of`, `set-of`; operations `add`, `put`, `remove-key`, `transform`, `filter`, `reject`, `reduce`, `pour-into`.
  - Example: two equal maps built in a different order have the same hash.
- [X] T008 [P] [US1] Write `docs/spec/03-evaluation-and-tail-calls.md` (S).
  - Content: D-05 (tail positions for every special form in contracts/core-forms.md).
  - Example: 1,000,000 mutually recursive calls (`even?`/`odd?` defined with `let-functions`).
- [X] T009 [P] [US1] Write `docs/spec/04-macros.md` (S).
  - Content: D-06 (`define-macro`, `quasi-quote`/`unquote`/`unquote-splicing`, `unique-symbol`, hashing after expansion).
  - Includes one example of variable capture and how `unique-symbol` avoids it.
- [X] T010 [P] [US1] Write `docs/spec/05-numerics.md` (S).
  - Content: D-04 (64-bit integers, `:overflow`, `:inexact-division`, `quotient`/`modulo`/`remainder`, IEEE double, `not-a-number`/`infinity`).
  - Explains the fixed reduction tree, with a forward reference to section 11.
- [X] T011 [P] [US1] Write `docs/spec/06-futures.md` (M).
  - Content: D-07 and D-10 (the three states, `wait-for`, `state`, settled-only escape).
  - Cites data-model.md `Future.state` and its state machine, and data-model.md `Future.scope`
    (every future belongs to exactly one scope, which stays live while the future is `:empty`).
- [X] T012 [P] [US1] Write `docs/spec/07-scopes-and-cancellation.md` (M).
  - Content: D-08, D-09, D-11 and D-14 (`with-scope`, `spawn`, root scope, `cancel!`, the cancellation points, `check-cancellation`, `shield-from-cancellation`, first failure plus suppressed, `:deadlock`).
  - Cites data-model.md `Scope.status`, `Scope.primary-error`, `Scope.suppressed`.
- [X] T013 [P] [US1] Write `docs/spec/08-combinators.md` (S).
  - Content: D-12 (`all`, `first-success` with `:all-failed`, `then`, `with-timeout` with a race rule, `let-parallel`).
  - Each combinator gets one success example and one failure example.
- [X] T014 [P] [US1] Write `docs/spec/09-agents.md` (M).
  - Content: D-15 and D-16 (`agent`, `update`, `transact`, `state`, per-sender FIFO, a failed update keeps the state, re-entrancy, cancellation of a pending send).
  - Cites data-model.md `Agent.state` and `Agent.queue`, and the update lifecycle.
- [X] T015 [P] [US1] Write `docs/spec/10-streams.md` (M).
  - Content: D-17 (`make-stream`, `emit`, `stream-transform`, `stream-filter`, `stream-take`, `stream-reduce`, `close!`, `file-lines`).
  - Cites data-model.md `Stream.maximum-ahead` and `Stream.owner-scope`.
- [X] T016 [P] [US1] Write `docs/spec/11-data-parallelism.md` (S).
  - Content: D-18 (`parallel-transform`, `parallel-reduce`, `fork-join`, the fixed reduction tree, child scope); H12 marked open for Phase 3.
- [X] T017 [P] [US1] Write `docs/spec/12-errors.md` (M).
  - Content: D-13 (`error`, `raise`, `try`/`catch`/`finally`, `error-kind`, `error-data`).
  - A kinds table with the phase that introduces each kind.
  - Cites data-model.md `Error.kind`, `Error.origin`, `Error.cause`, `Error.suppressed`.
- [X] T018 [P] [US1] Write `docs/spec/14-modules.md` (S).
  - Content: D-23 (`namespace`, `:import`, names bound to hashes, redefinition).
  - The reloading policy is marked "Open — closed in Phase 5c / Phase 7".
- [X] T019 [US1] Create the stub `docs/spec/13-serialization-and-identity.md` (S). It has the Question line, `**Status:** Partially open` and `**Decision:** D-19, D-20, D-21, D-22`, so that step 1 passes. US3 writes the full content.
- [X] T020 [US1] Run [quickstart.md](./quickstart.md) step 1 and fix every reported section (S).

**Checkpoint**: US1 is complete; the specification can be read on its own (the MVP).

---

## Phase 4: User Story 2 — Local reference programs 1–10 (Priority: P1)

**Goal**: 10 program directories. Each has source, injected conditions, assertions written as
data, and a design note.

**Independent Test**: [quickstart.md](./quickstart.md) steps 2 and 3 (limited to 01–10). A
reader can write each test without asking a question.

- [X] T021 [P] [US2] Write `programs/01-parallel-search/` (M).
  - `main.qx`: `let-parallel` or `all` over a user lookup and an orders lookup.
  - `conditions.qx`: delays of 100 and 150 ms.
  - `expected.qx`: `returns-satisfying`, `(elapsed-at-least 150)`, `(elapsed-under 250)`, `future-state :filled` for both.
- [X] T022 [P] [US2] Write `programs/02-first-response-wins/` (S).
  - `main.qx`: `first-success` over three replicas.
  - `conditions.qx`: delays of 300, 50 and 200 ms.
  - `expected.qx`: the 50 ms answer is returned; the other two end with `future-error … :cancelled`.
- [X] T023 [P] [US2] Write `programs/03-timeout-and-errors/` (S).
  - Two cases: the example in contracts/reference-program.md, and a `with-timeout` of 100 ms over 5 s that raises `:timeout`.
  - Use `main.qx` with one entry per case, or two documented directories.
- [X] T024 [P] [US2] Write `programs/04-bank-account-agent/` (M).
  - `main.qx`: `transact` with a check-and-debit function.
  - `conditions.qx`: 10 withdrawals of 20 against a balance of 100.
  - `expected.qx`: exactly 5 successes, the final balance from `agent-final` is 0, and `(never (negative? balance))`.
  - Cites data-model.md `Agent.queue`.
- [X] T025 [P] [US2] Write `programs/05-parallel-transform/` (S).
  - A vector of 1,000,000 integers with a pure function.
  - `expected.qx`: `same-as-sequential`, `(deterministic 5)`.
  - Includes a float reduction case for D-18.
- [X] T026 [P] [US2] Write `programs/06-file-line-stream/` (M).
  - `file-lines` → `stream-transform` → `stream-filter` → `stream-take` 5.
  - `conditions.qx`: a generated `lines.txt` of 10,000 lines.
  - `expected.qx`: `output-lines` with the first 5 matches, and a bound on the lines read: at most `:maximum-ahead` + 1 lines past the 5th match.
  - Also write `docs/spec/appendix-input-output.md`. It defines `open-file`, `file-lines`, `listen`, `accept`, `read-line`, `write-line` and `print-line` (the cancellation points, and the errors on a closed resource). The appendix is not one of the 14 topics.
- [X] T027 [P] [US2] Write `programs/07-backpressure/` (S).
  - A producer at 1 ms per item and a consumer at 50 ms per item.
  - `expected.qx`: `(maximum-ahead :producer 1)`.
  - Cites data-model.md `Stream.maximum-ahead`.
- [X] T028 [P] [US2] Write `programs/08-fork-join/` (M).
  - A tree sum over 2^16 leaves and a parallel quicksort with `fork-join`.
  - `expected.qx`: `same-as-sequential`.
  - A failure case: a subtask fails, the root raises, and the pending subtasks end `:cancelled`.
- [X] T029 [P] [US2] Write `programs/09-deadlock/` (S).
  - A waits for B and B waits for A, in one scope.
  - `expected.qx`: `(raises :deadlock)`, both futures with `:deadlock`, `elapsed-under`.
  - Cites data-model.md `Future.state`.
- [X] T030 [US2] Write `programs/10-chat-server/` (L).
  - One scope per connection, with the room state in an agent.
  - `conditions.qx`: 3 simulated clients, one of which disconnects abruptly.
  - `expected.qx`:
    - each remaining client receives every message;
    - all clients see the same total order, which is the room agent's commit order;
    - the disconnected client's scope tasks end `:cancelled`.
  - Depends on T014 and T015 (agents and streams), and on T026 (I/O appendix).
- [X] T031 [US2] Run [quickstart.md](./quickstart.md) steps 2 and 3 for programs 01–10, and fix the gaps (S).

**Checkpoint**: US1 and US2 together form the local acceptance suite for feature 002.

---

## Phase 5: User Story 3 — Distribution-affecting decisions and programs 11–12 (Priority: P2)

**Goal**: the serialization and identity section is complete, and programs 11 and 12 run
unchanged on simulated nodes.

**Independent Test**: the reader can state the serialization class of every value kind, and the
error seen when its node is unreachable, from section 13 alone. Programs 11 and 12 pass
quickstart steps 2 and 3.

- [X] T032 [US3] Complete `docs/spec/13-serialization-and-identity.md` (M).
  - Content:
    - D-19: content hashing, de Bruijn indices, hashing per strongly connected component, closure = hash plus environment.
    - D-20: the three serialization classes, `serializable?`, `:not-serializable` at the sender.
    - D-21: scope identifier and cancellation capability.
    - D-22: explicit `spawn-on`; no transparent remote calls.
  - Copies the table "Equality, hash and serialization" from data-model.md.
  - Cites data-model.md `Function / Closure.function-hash`, `Function / Closure.name` (metadata, never hashed), `Function / Closure.captured-environment`, `Scope.identifier` and `Local resource` (flag `local-resource`).
  - Marks H4's remaining part as "Open — closed in Phase 5c".
- [X] T033 [US3] Add the distribution kinds (`:node-unreachable`, `:transport`, `:remote`, `:not-serializable`) with examples to `docs/spec/12-errors.md` (S). Depends on T017.
- [X] T034 [P] [US3] Write `programs/11-remote-futures-crash/` (L).
  - `main.qx`: work split with `spawn-on` across `(node :a)` and `(node :b)`, plus a recomputation path when a partition fails with `:node-unreachable`.
  - `conditions.qx`: `:nodes [{:name :a} {:name :b :crash-after {:completed 4}}]`.
  - `expected.qx`: `returns` equal to the result of a run with no failure, and the futures for node b fail with `:node-unreachable`.
- [X] T035 [P] [US3] Write `programs/12-remote-agent-stream/` (L).
  - Part 1: a remote agent with an operation identifier on every `update`; `conditions.qx` drops the acknowledgement of operation 7; `expected.qx` has `(applied-once 7)` and the `agent-final` of a run with no failure.
  - Part 2: a stream from node a to node b with a slow consumer; `expected.qx` has `(maximum-ahead :producer 1)` across the network.
  - Records that how operation identifiers are stored is H5 (Phase 5d).
- [X] T036 [US3] Run [quickstart.md](./quickstart.md) steps 2 and 3 for programs 11 and 12, and confirm that the source contains no fault-injection code (D-25) (S).

**Checkpoint**: all 12 programs exist. The decisions that Phase 5 inherits are written down
normatively.

---

## Phase 6: User Story 5 — Register of open questions and hypotheses (Priority: P2)

**Goal**: every item of spec FR-026 has a recommendation or a closing phase, linked to a D-xx
or H-n.

**Independent Test**: [quickstart.md](./quickstart.md) steps 4 and 5 pass.

- [ ] T037 [US5] Fill `docs/spec/open-questions.md` (M).
  - One row per FR-026 item:
    - cancellation → D-09
    - scope-ended future → D-10
    - escape → D-10
    - multiple failures → D-11
    - agents → D-15/D-16
    - error model → D-13
    - scope identity → D-21
    - function and future equality → D-02/D-19
    - modules → D-23
    - hygiene → D-06
    - numerics → D-04
    - deadlock → D-14
    - H3 → resolved
    - H4 → partly resolved, rest in 5c
  - H1, H2 and H7 point to feature 002; H5 and H6 to Phase 5; H8–H12 to the phases given in `docs/hypotheses.md`.
- [ ] T038 [US5] Check `docs/decisions.md` and `docs/hypotheses.md` against `research.md` (S).
  - There are 28 decision rows, and the "Inherited by" column is filled for D-07, D-09, D-10, D-11, D-13, D-14, D-19, D-20 and D-21.
  - H3 is resolved, H4 is partly resolved, and H8–H12 are present.
  - Run [quickstart.md](./quickstart.md) step 4.

---

## Phase 7: User Story 4 — Actor versions side by side (Priority: P3)

**Goal**: actor versions of programs 4, 10 and 12 (D-24), with one comparison table.

**Independent Test**: the three `actors.qx` files exist, and the comparison table has identical
rows for each pair.

- [ ] T039 [P] [US4] Write `programs/04-bank-account-agent/actors.qx` using `spawn-actor`, `receive`, `send-message` and `reply` (S)
- [ ] T040 [P] [US4] Write `programs/10-chat-server/actors.qx`, with a room actor and connection actors joined by `link` and `supervise` (M)
- [ ] T041 [P] [US4] Write `programs/12-remote-agent-stream/actors.qx`, with a remote actor that deduplicates operation identifiers (M)
- [ ] T042 [US4] Write `programs/actors-comparison.md` (M).
  - One table per pair, with the same rows: lines of code, where atomicity lives, how the caller observes a failure, what supervision with restart adds, what is lost without actors.
  - A closing paragraph feeds the Phase 4 evaluation and Phase 5f.

---

## Phase 8: Polish & Cross-Cutting Concerns

- [ ] T043 Check names (S): every symbol used in `docs/spec/*.md` and `programs/**/*.qx` must be in `docs/naming.md` with status `approved`. Add any missing name as `proposed` and ask the owner. Run the naming step (4b) of [quickstart.md](./quickstart.md).
- [ ] T044 Do the self-review pass for SC-007 (M). For each of the 12 programs, draft the Phase 1 test outline in a scratch file using only the documents. Record every question in `docs/spec/open-questions.md` and resolve it. Repeat until a full pass adds no new question.
- [ ] T045 [P] Update `docs/speckit-roadmap.md` (S): mark feature 001 complete, and point the `/speckit-specify` input of feature 002 at `docs/spec/` and `programs/`.
- [ ] T046 Run [quickstart.md](./quickstart.md) steps 1–6 end to end and record the result at the end of `specs/001-language-spec/checklists/requirements.md` (S).

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (T001–T003)**: no dependencies.
- **Foundational (T004–T005)**: depends on Setup, and blocks all stories.
- **US1 (T006–T020)**: depends on Foundational.
- **US2 (T021–T031)**: depends on Foundational. Its programs reference sections of US1, so they
  are best written after the matching US1 section:

  | Programs | US1 sections |
  |---|---|
  | 01–03, 08 | T011–T013 |
  | 04 | T014 |
  | 05 | T016 |
  | 06–07 | T015 |
  | 09 | T012 |
  | 10 | T014, T015, T026 (I/O appendix) |

- **US3 (T032–T036)**: depends on US1 (T017, T019). The programs depend on T032.
- **US5 (T037–T038)**: depends on US1 and US3, because the register links to their decisions.
- **US4 (T039–T042)**: depends on US2 (programs 04 and 10) and US3 (program 12).
- **Polish (T043–T046)**: depends on every story.

### Story completion order

```text
Setup → Foundational → US1 ─┬─► US2 ──┬─► US4 ─┐
                            └─► US3 ──┴─► US5 ─┴─► Polish
```

### Parallel opportunities

- T002 and T003 in parallel.
- T006–T018 (13 section files) are all independent: any order, or in parallel.
- T021–T029 (programs 01–09) can proceed in parallel once the sections they reference exist.
- T034 and T035 in parallel after T032.
- T039–T041 in parallel.

#### Example: US1 in parallel

```text
T006 01-values-and-equality.md   T011 06-futures.md   T015 10-streams.md
T007 02-collections.md           T012 07-scopes…md    T016 11-data-parallelism.md
…                                T013 08-combinators  T017 12-errors.md
```

#### Example: US2 in parallel

```text
T021 programs/01-…  T022 programs/02-…  T023 programs/03-…  T028 programs/08-…
```

---

## Implementation Strategy

### MVP first (US1 only)

1. Setup, then Foundational.
2. US1: the 14 sections, with section 13 as a stub.
3. **Stop and validate** with quickstart step 1. This already gives a readable specification.

### Incremental delivery

1. Add US2 to get the local acceptance suite. Feature 002 could start here.
2. Add US3 to write down the distribution-affecting decisions and programs 11 and 12.
3. Add US5 to fill the register and close the inheritance into later phases.
4. Add US4 for the actor comparison (optional for feature 002).
5. Polish: name check, self-review, final quickstart run.

### Stopping points

The project can stop after any story checkpoint with a coherent artifact:

| After | What exists |
|---|---|
| US1 | the specification |
| US2 | the specification and the local suite |
| US3 | everything that Phase 5 inherits |

---

## Notes

- [P] = different files, no dependency on unfinished tasks.
- Commit after each task or logical group, once git is initialized (the repository is not a git
  repository yet).
- No task may introduce a public name that is not approved in `docs/naming.md` (Principle VIII).
- No task contains durations; the sizes (S/M/L) are relative only (Principle VI).
