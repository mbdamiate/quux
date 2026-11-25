---

description: "Task list for feature 003: core ergonomics study"
---

# Tasks: Core Ergonomics Study

**Input**: Design documents from `/specs/003-core-ergonomics/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md),
[data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

**Tests**: requested by the spec. Each program is its own test (FR-006). Its expected output is
written **before** the program is first run, so it is a failing test until the program is
correct (D-60). Bug fixes come with a regression test (FR-009).

**Organization**: tasks are grouped by user story. US1–US4 match spec User Stories 1–4; User
Story 5 is out of scope (clarification Q1).

**Conventions**:

- Programs follow [contracts/program-layout.md](./contracts/program-layout.md). They use only
  names with status `approved` in [docs/naming.md](../../docs/naming.md), and no task uses a
  `proposed` name (FR-004, FR-012).
- Tests follow [contracts/test-driver.md](./contracts/test-driver.md). The report follows
  [contracts/friction-report.md](./contracts/friction-report.md).
- Friction notes are written **while** a program is written, in its README, with `Review:
  unreviewed`. Mistakes that produce an interpreter error are logged as they happen, as `M-NN`
  entries in the **Mistakes** section of a working file `programs/ergonomics/working-notes.md` (which
  also collects bugs and timings), which T034 moves into the
  report (FR-010a).
- Sizes are relative (S, M, L), with no durations (Principle VI).
- The checkpoints run `tools/local-check.sh` (macOS arm64 and the Linux arm64 container) and CI on
  Linux x86_64 (D-53, D-31).
- The language runtime changes only for a bug found by a program (FR-009, task T024). No library
  addition is made (FR-014).

## Format: `[ID] [P?] [Story] Description (size)`

- **[P]**: can run in parallel (different files, no dependency on unfinished tasks)
- **[Story]**: the user story the task belongs to (US1–US4)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: the directories, the index and the templates every program uses.

- [X] T001 Create `programs/ergonomics/README.md`: the index table (number, directory, area, variant and partner, status, number of friction notes) with the ten rows of D-59 marked `planned`, and the note that these programs are not the phase acceptance suite (contracts/program-layout.md) (S)
- [X] T002 [P] Create `programs/ergonomics/working-notes.md`, a working file with three sections: **Mistakes** (the columns of data-model.md `Mistake.identifier`, `Mistake.code`, `Mistake.kind and message`, `Mistake.pointed to the cause`, `Mistake.program`, `Mistake.load time`), **Bugs found** (T024) and **Timings** (T023) (S)
- [X] T003 [P] Create `tests/ergonomics/CMakeLists.txt` with empty lists `quux_ergonomics_ready` and `quux_ergonomics_pending`, and add `add_subdirectory(tests/ergonomics)` to `CMakeLists.txt` (D-56) (S)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: running one program as a test, with the plain runner and with the verifier. Every
program task depends on this phase.

**⚠️ CRITICAL**: no program can be checked automatically until this phase is complete.

- [X] T004 Move `run_file` and `file_run` from `source/main/quux.c` into `source/harness/quux_session.c` as `quux_session_run_file(path, input_text)`, declared in `source/harness/quux_session.h`; `quux` calls it, and the behavior stays identical. The existing `repl_session` test and a manual run of a file with and without input confirm that the output and the exit statuses 0, 1 and 2 are unchanged (D-56) (S)
- [X] T005 Add the `run FILE.qx [INPUT]` mode to `source/main/quux_test.c`. It accepts `--verify-roots` and `--verify-roots-every-allocation`, calls `quux_session_run_file`, and extends the usage text. Add the mode to `specs/002-blocking-prototype/contracts/command-line.md`. Add a unit-level check in `tests/repl/` (or a CTest in `tests/ergonomics/`) that `quux-test run` and `quux` give identical output for one small file (D-56; contracts/test-driver.md) (S)
- [X] T006 Write `tests/ergonomics/run_program_test.cmake`. It runs `RUNNER <dir>/main.qx [input.txt contents]`, compares the output byte for byte with `expected-output.txt` and the exit status with `expected-exit-status.txt` (default 0), and on a failure prints the expected and actual output and both statuses (contracts/test-driver.md) (S)
- [X] T007 In `tests/ergonomics/CMakeLists.txt`, register for each ready program `ergonomics_<dir>` (`quux`, label `ergonomics`) and `ergonomics_<dir>_verified` (`quux-test --verify-roots-every-allocation run`, labels `ergonomics;verified`, or `--verify-roots` when the program's README says `Verifier: amortized`, per data-model.md `Ergonomics program.verifier mode`). Register each pending program as `ergonomics_<dir>` with label `pending`. Every test gets `TIMEOUT 30` (D-56, D-57) (S)

**Checkpoint**: an empty ready list configures cleanly in every preset; `quux-test run` works.

---

## Phase 3: User Story 1 — Everyday programs that run today (Priority: P1) 🎯 MVP

**Goal**: ten programs over the eight areas of FR-001. Eight are runnable with exact expected
output; two are pending with named missing pieces. Each has its "Friction observed" notes.

**Independent Test**: `ctest --preset debug -L ergonomics` passes for every ready program;
`quux-name-check` passes; each pending program, run by hand, stops at a `missing:` stub
(quickstart scenarios 1–3, 5).

**Order of work inside each program task** (D-60):

1. Write the README sections "Area", "What it evaluates" and "How to run", and the `Verifier` line of "Status" (default `every allocation`).
2. Write `expected-output.txt` (and `expected-exit-status.txt` or `input.txt` if needed) **from the
   specification**.
3. Write `main.qx`, with comments explaining intent, recording friction notes and mistakes as
   they happen.
4. Run the program. On a mismatch, decide whether it is a bug (T024), a spec ambiguity (a W-xx
   entry, added in T037) or a program error, and record the decision.
5. Add the directory to the ready or pending list in `tests/ergonomics/CMakeLists.txt`, and update
   the index row.

### First program (end-to-end check of the pipeline)

- [X] T008 [US1] Program 01, nested data, in `programs/ergonomics/01-nested-data/`. It updates maps inside vectors inside maps (for example an inventory by warehouse), with `put`, `get`, `transform`, `reduce`, `thread-first` and `thread-last`, and prints the results with `print-line` and `to-string` (FR-001 area 1) (M)

**Checkpoint (stopping point 1)**: `ergonomics_01-nested-data` and its `_verified` variant pass in
every preset locally; the name check passes.

### Remaining programs (each in its own directory; any order)

- [X] T009 [P] [US1] Program 02, text with prepared input, in `programs/ergonomics/02-text-prepared-input/`, with `input.txt`: a vector of words and a vector of comma-separated rows already split into fields. It does a word count with a map, a "CSV" table from the rows, and aligned output built only with `to-string`. Variant: prepared input; partner: 03 (FR-001 area 2; FR-005a) (M)
- [X] T010 [P] [US1] Program 03, raw text, in `programs/ergonomics/03-text-raw/`, with `input.txt` holding one quux string (the same text as 02). It has the same logic as 02 over raw text, with local stubs for each missing primitive (splitting on whitespace, splitting on a separator, trimming, padding). `expected-output.txt` is the intended final output, the same as 02's. Status: pending, listing each stub. Variant: raw text; partner: 02 (FR-005a, D-58) (status per data-model.md `Ergonomics program.status` and `Ergonomics program.pending reason`; variant per `Ergonomics program.variant`) (M)
- [X] T011 [P] [US1] Program 04, tree and parser, in `programs/ergonomics/04-tree-and-parser/`. It has a persistent binary search tree (insert, lookup, in-order walk) built from maps, and a recursive-descent parser for arithmetic over token vectors such as `[1 :plus 2 :times 3]`, with precedence and parentheses as nested vectors. It uses vector patterns, map patterns and `:rest` (FR-001 area 3; D-59) (M)
- [X] T012 [P] [US1] Program 05, errors with prepared input, in `programs/ergonomics/05-errors-prepared-input/`, with `input.txt`: a vector of records already read as data, some of them invalid. It validates them with user errors carrying `:data`, uses `try`/`catch`/`finally`, catches by kind, collects the error reports, and re-raises one error, so that `expected-exit-status.txt` is `1` and the output ends with the `!!` line. Variant: prepared input; partner: 06 (FR-001 area 4; FR-005a) (exit status per data-model.md `Ergonomics program.expected exit status`) (M)
- [X] T013 [P] [US1] Program 06, reading text, in `programs/ergonomics/06-errors-reading-text/`, with `input.txt` holding one quux string with several records written as quux text, one malformed. It reads each record with a local stub for "read data from a string", handles `:syntax` for the malformed one, and otherwise follows 05. Status: pending, listing the stub. Variant: raw text; partner: 05 (FR-005a, D-58) (status per data-model.md `Ergonomics program.status` and `Ergonomics program.pending reason`; variant per `Ergonomics program.variant`) (M)
- [X] T014 [P] [US1] Program 07, macros, in `programs/ergonomics/07-macros/`. It contains a counted loop macro and a small table-checking domain-specific language (for example rules over a map), built with `define-macro`, `quasi-quote`, `unquote`, `unquote-splicing` written out in full, and `unique-symbol` for hygiene. No approved name expands a macro from quux code, so seeing an expansion is recorded as friction rather than worked around (FR-001 area 5) (M)
- [X] T015 [P] [US1] Program 08, namespaces, in `programs/ergonomics/08-namespaces/`. Three `namespace` forms in one file (for example `geometry`, `report` and the main one) with `:import`, qualified and unqualified calls, and one name defined in two namespaces to show qualification. `main` is defined in the last namespace (FR-001 area 6; research risk R2) (S)
- [X] T016 [P] [US1] Program 09, numerics, in `programs/ergonomics/09-numerics/`. Simple statistics (mean, median, variance) over integers and floats, and fraction-like arithmetic with `quotient` and `remainder`. It meets `:inexact-division` and `:overflow` inside `try` and reports them, and compares with `numerically-equal?` versus `equal?` (FR-001 area 7) (S)
- [X] T017 [P] [US1] Program 10, common mistakes, in `programs/ergonomics/10-common-mistakes/`. About ten mistakes, each in its own `try` that prints the kind and message: wrong argument count, undefined name, a pattern that does not match its value, `get` on a non-map, calling a non-function, arithmetic on a non-number, `element-at` out of range, `put` with a bad key on a vector, deep non-tail recursion (`:stack-exhausted`), and a `raise` of a non-error. Mistakes that happen at load time (for example `unquote` outside `quasi-quote`, or a malformed `let`) are run as separate scratch files and logged as `M-NN` entries with "load time: yes", not as part of the program (FR-001 area 8; FR-010a) (M)

### Story completion

- [X] T018 [US1] Run program 10 on macOS, in the container and (after a push) in CI, and confirm that its messages are identical on all three targets. Any difference is a bug for T024 (research risk R5) (S)
- [X] T019 [US1] Update `programs/ergonomics/README.md`: every row has its final status, its pair and its friction-note count. Confirm that `quux-name-check` passes and that every pending program's README lists exactly the stubs in its `main.qx` (SC-001, SC-003; FR-007) (data-model.md `Ergonomics program.status`, `Ergonomics program.pending reason`, `Friction note.identifier`) (S)

**Checkpoint (stopping point 2)**: eight programs pass under `ergonomics`, two are `pending`, the
name check passes, and the local check passes.

---

## Phase 4: User Story 2 — Programs are checked automatically (Priority: P1)

**Goal**: the programs run on every target, preset and push, under the verifier and valgrind,
and a changed expectation is caught.

**Independent Test**: quickstart scenarios 2, 4 and 6.

- [X] T020 [US2] In `tests/ergonomics/CMakeLists.txt`, add `ergonomics_<dir>_valgrind` for each ready program (`valgrind --error-exitcode=1 --leak-check=full quux`, labels `ergonomics;valgrind`) when valgrind is found, following `tests/smoke/CMakeLists.txt`. Run `tools/ci/run-valgrind.sh` in the container and confirm the programs pass (D-57) (S)
- [X] T021 [US2] Check that changes are detected (SC-007). For one runnable program, change one character of `expected-output.txt` and confirm that only that test fails, then restore it. Do the same for `expected-exit-status.txt`. Record the result in the PR description (S)
- [X] T022 [US2] Run `tools/local-check.sh` and confirm that `build/local-check.log` lists the `ergonomics_*`, `_verified` and (in the container) `_valgrind` tests for every preset, including the gcc build. After a push, confirm the same in the CI log (FR-008; D-57) (S)
- [X] T023 [US2] Run `ctest --preset release -L ergonomics` on macOS arm64 twice. Confirm identical pass results (SC-007), and record each program's time and data size in the **Timings** section of `programs/ergonomics/working-notes.md` for the report (D-63) (S)

**Checkpoint**: CI is green with the `ergonomics` tests on Linux x86_64; the local check passes.

---

## Bug fixes (any time during US1 and US2)

- [ ] T024 [US1] For each interpreter bug a program exposes: write a failing regression test first (a unit test in `tests/unit/` or a specification example), fix it in `source/`, and record the program, the symptom, the fix and the test in the **Bugs found** section of `programs/ergonomics/working-notes.md`. A fix larger than size M is not done here: the program stays `pending` with the bug as its reason, and the bug is written down as a task for feature 002 (FR-009; research risk R3) (S–M per bug)

---

## Phase 5: User Story 3 — A classified friction report (Priority: P2)

**Goal**: the owner confirms the friction notes, then gets `docs/ergonomics-report.md` with every
note classified and recommended.

**Independent Test**: quickstart scenario 8. Every note that is `kept` or `disputed` maps to
exactly one `E-NN`, and every `E-NN` has a source.

- [ ] T025 [US3] **Owner review checkpoint** (FR-009a). Present the ten programs and their friction tables to the owner. Record the owner's additions (`Origin: owner`), disputes (`Review: disputed: <reason>`) and withdrawals (`Review: withdrawn`), and set every other note to `kept`. No classification starts before this task is done (data-model.md `Friction note.origin`, `Friction note.review status`) (S)
- [ ] T026 [US3] Create `docs/ergonomics-report.md` with the six sections of contracts/friction-report.md. Fill **Method**: the program list and status, how the expected outputs were derived (D-60), the review checkpoint's results, and placeholders for the counts (S)
- [ ] T027 [US3] Merge the reviewed notes into `E-NN` entries: one per distinct friction, listing all its `PNN-Fk` (and `M-NN`) sources. Write **Observed** with a minimal quux excerpt for each (SC-004) (M)
- [ ] T028 [US3] Classify each entry as exactly one of: missing function, awkward name, missing syntax or pattern, error message quality, or performance surprise. Mistakes marked "partly" or "no" become `error message quality` entries (FR-010, FR-010a) (data-model.md `Report entry.class`, `Mistake.pointed to the cause`) (S)
- [ ] T029 [US3] Write each entry's **Recommendation** (add, rename, document, keep as is) and **Rationale**. Label every claim. For comparisons with Clojure, Scheme or Racket, check the source documentation on the web when writing and link it as `[FACT: link]`. Proposed names are new full-word names, never copied foreign names unless they meet Principle VIII (D-64) (data-model.md `Report entry.recommendation`, `Report entry.rationale`, `Report entry.record`) (L)
- [ ] T030 [P] [US3] Fill the **Mistakes log** from the **Mistakes** section of `programs/ergonomics/working-notes.md` (FR-010a) (S)
- [ ] T031 [P] [US3] Fill **Bugs found** from T024's list, with the fix commits and regression tests (S)
- [ ] T032 [P] [US3] Fill **Performance** from T023's table, flagging anything over 1 second or any `:stack-exhausted` under 10,000 elements (D-63) (S)
- [ ] T033 [US3] Complete the counts in **Method**, and run quickstart scenario 8. Every `kept`/`disputed` note is in exactly one entry and every entry has a source (SC-004) (S)
- [ ] T034 [US3] Delete `programs/ergonomics/working-notes.md`, now that its content is in the report, and update the program READMEs' "Mistakes made while writing" to point to the report's `M-NN` IDs (S)

**Checkpoint (stopping point 4, first half)**: the report is complete except for the decision
list.

---

## Phase 6: User Story 4 — A short decision list for the owner (Priority: P2)

**Goal**: at most 15 decision items, each answerable from the list alone, with every proposed name
in `docs/naming.md` section K and every language change tracked as a W-xx entry.

**Independent Test**: quickstart scenarios 8 (decision list part) and 9. After the owner's
answers, no section K row is `proposed`.

- [ ] T035 [US4] Add section K to `docs/naming.md` ("Proposed in feature 003"), with one row per proposed new or renamed name (group `K<n>`, the names, `replaces or adds`, the decision placeholder, status `proposed`). The names are used nowhere else (FR-012; data-model.md Proposed name) (data-model.md `Proposed name.row`, `Proposed name.name`, `Proposed name.status`) (S)
- [ ] T036 [US4] Write the **Owner decision list** in `docs/ergonomics-report.md`: one `K-NN` item per add or rename recommendation. Merge related additions into one item where one decision covers them (for example a small set of string functions), so that the list has at most 15 items. Each item has the proposal, signature and example, the friction removed, the programs affected, and the alternatives including "keep as is" (FR-013; SC-006) (data-model.md `Decision item.identifier`, at most 15) (M)
- [ ] T037 [US4] Add W-xx entries from W12 to `docs/spec/open-questions.md`, one per `K-NN` item and one per spec ambiguity found in T008–T017, each with its status "owner decision, feature 003", and link them from the report entries (FR-011; D-62; SC-005) (S)
- [ ] T038 [US4] Apply every **document** recommendation as a change to the specification text (sections 01–05, 12 or 14) and record the section in the entry's **Record**. Re-run the example suite (S)
- [ ] T039 [US4] Run quickstart scenario 9: no `proposed` name appears outside `docs/naming.md`, the report and `docs/spec/open-questions.md`. Run `quux-name-check` (FR-012) (S)
- [ ] T040 [US4] **Owner answers**. Present the decision list and record each answer in the item. For each `approve`, add a D-xx decision after D-64 to `research.md` and `docs/decisions.md`, marked "inherited by: Phase 3" when the VM inherits it, set the section K row to `approved`, and close the W-xx entry. For each `reject`, set the row to `rejected`, close the W-xx entry as "keep as is" with the reason, and change the report entry's recommendation. For each `defer`, keep the W-xx entry open with its reason (D-62; spec US4 scenario 3) (data-model.md `Decision item.owner answer`, `Proposed name.status`) (M)

**Checkpoint (stopping point 5)**: every section K row is `approved` or `rejected` (or deferred
with an open W-xx entry), and every report entry with add or rename links its record.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [ ] T041 [P] Update `docs/speckit-roadmap.md`: mark 003 done, and add to feature 004's input ("Starting reality") a pointer to the decision list and the approved additions that a later feature will implement (S)
- [ ] T042 [P] Add a note to `specs/002-blocking-prototype/tasks.md`, at the task that adds the reference-program test loop, that the loop must match `programs/[0-9][0-9]-*` or list the programs, so that it does not pick up `programs/ergonomics/` (research risk R1) (S)
- [ ] T043 Run all of quickstart.md (scenarios 1–9) and `tools/local-check.sh`. Push, confirm CI green, and open the pull request to `main` with the SC-001..SC-007 evidence in its description (FR-015) (S)

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (Phase 1)**: no dependencies.
- **Foundational (Phase 2)**: depends on Setup; it blocks every program.
- **US1 (Phase 3)**: depends on Foundational. T008 comes first, as the end-to-end check. T009–T017 depend only on T008 and can be done in any order.
- **US2 (Phase 4)**: T020–T023 depend on the ready programs of US1. T020 can start once T008 is done and be extended as programs arrive.
- **Bug fixes (T024)**: they happen inside US1 and US2 as bugs are found.
- **US3 (Phase 5)**: depends on all of US1 (every program written) and on T023. T025 (owner review) blocks T026–T034.
- **US4 (Phase 6)**: depends on US3's T029. T040 depends on T035–T039 and on the owner.
- **Polish (Phase 7)**: after US4. T042 can be done any time.

### Story dependencies

- US1 and US2 are both P1. US2's checks need US1's programs, but US1 is useful without US2's extra
  variants.
- US3 needs US1 complete. US4 needs US3's recommendations.

### Within each program task

The expected output comes before `main.qx`, `main.qx` comes before the run, and the run comes
before registration (D-60).

---

## Parallel Example: User Story 1

After T008, the program tasks touch different directories:

```text
Task: "T009 Program 02, text with prepared input, in programs/ergonomics/02-text-prepared-input/"
Task: "T011 Program 04, tree and parser, in programs/ergonomics/04-tree-and-parser/"
Task: "T015 Program 08, namespaces, in programs/ergonomics/08-namespaces/"
Task: "T016 Program 09, numerics, in programs/ergonomics/09-numerics/"
```

These shared files are edited one task at a time:

- `tests/ergonomics/CMakeLists.txt` and `programs/ergonomics/README.md` (step 5 of each program task);
- `programs/ergonomics/working-notes.md` (append only).

## Parallel Example: User Story 3

After T029:

```text
Task: "T030 Mistakes log"
Task: "T031 Bugs found"
Task: "T032 Performance"
```

---

## Implementation Strategy

### MVP first (stopping point 1, then 2)

1. Phases 1 and 2: the infrastructure.
2. T008, program 01. **Stop and validate**: the pipeline works on all targets.
3. T009–T019: all programs. **Stop and validate**: everyday quux is documented and tested, which
   is useful even if the study stops here.

### Incremental delivery

1. Stopping point 2: the programs and their raw friction notes.
2. US2: hardening on every target, under valgrind and the verifier.
3. Owner review checkpoint, then the report (US3).
4. Decision list and owner answers (US4).

Each step can be merged on its own. Every merge leaves the label `ergonomics` green and the
`pending` programs named.

---

## Notes

- Total: 43 tasks.
  - Setup: 3.
  - Foundational: 4.
  - US1: 12, plus T024 (bug fixes).
  - US2: 4.
  - US3: 10.
  - US4: 6.
  - Polish: 3.
- `[P]` marks tasks in different directories or files. Shared registration files are edited one
  task at a time.
- No task uses a `proposed` name. If a program cannot be finished without one, it stays `pending`.
- Commit after each program or each logical group, and run the pre-push check on every push.
