# Implementation Plan: Core Ergonomics Study

**Branch**: `003-core-ergonomics` | **Date**: 2026-11-25 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/003-core-ergonomics/spec.md`

## Summary

This feature writes ten small quux programs that use the Phase 1 sequential core the way a user
would. They cover:

- nested data;
- text;
- recursion and patterns;
- errors;
- macros;
- namespaces;
- numerics;
- common mistakes.

Each program runs as an automated test under a new `ergonomics` label. Where the core cannot
express something, the program is `pending` and names the missing piece. While writing the
programs, the writer records every friction point. The owner then reviews them, and only after
that review is a classified report written, ending in a short decision list for the owner.

The technical approach is recorded in [research.md](./research.md) as D-55..D-64:

- one directory per program, checked by the existing name check (D-55);
- a byte-for-byte output test, plus a verified run through a new `quux-test run` mode (D-56);
- no CI changes, because the label is picked up automatically (D-57);
- pending variants with local stubs that raise (D-58);
- expected outputs derived from the specification before the first run, so that the tests can
  find interpreter bugs (D-60);
- stable IDs from the friction note through the report entry to the decision (D-61, D-62).

## Technical Context

| Field | Value |
|---|---|
| **Language/Version** | quux (the Phase 1 sequential core on `main`) for the programs; C17 (`-std=gnu17`) only for the `quux-test run` mode and for bug fixes; CMake for the test driver. |
| **Primary Dependencies** | The existing build: the `quux` file runner, `quux-test`, `quux-name-check`. No new library. |
| **Storage** | Files: `programs/ergonomics/`, `docs/ergonomics-report.md`, `docs/naming.md` section K, `docs/spec/open-questions.md`. |
| **Testing** | CTest with the labels `ergonomics`, `ergonomics;verified`, `valgrind` (Linux debug) and `pending`; the existing presets `debug`, `asan-ubsan`, `tsan` and `release`, plus the gcc build. |
| **Target Platform** | macOS arm64 and the Linux arm64 container (local check), and Linux x86_64 (CI), as for feature 002. |
| **Project Type** | Language study: example programs, test integration and design documents. |
| **Performance Goals** | No goal. A program over 1 second in `release`, or `:stack-exhausted` under 10,000 elements, is recorded as a finding (D-63). |
| **Constraints** | Constitution I–VIII. Programs use only approved names (FR-004). The language runtime changes only for bug fixes, each with a regression test (FR-009). No library additions (FR-014). The branch starts from `main` and does not depend on unmerged branches (FR-015). |
| **Scale/Scope** | 10 programs (8 runnable, 2 pending) `[ESTIMATE]` of 30–120 lines each, 1 report, at most 15 decision items. |
| **Depends on** | Features 001 and 002 as merged on `main` (`c8b54a6`): the specification, the Phase 1 core, D-01..D-54, and constitution v1.2.3. |

No item is marked NEEDS CLARIFICATION. The three spec clarifications (Q1–Q3) and the
research above resolve the open points.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Status |
|---|---|---|
| I. C only | The only implementation code is the `quux-test run` mode and bug fixes, both in C. The test driver is a CMake script, which is build tooling. | PASS |
| II. Immutability and canonical structures | The programs use persistent collections only. Expected output relies on the canonical print order (section 01). | PASS |
| III. Structured concurrency | Not exercised: the study is sequential by design. The 12 reference programs keep that role. | PASS (not applicable) |
| IV. Design before performance | The study collects vocabulary evidence before the Phase 3 VM inherits it. Performance is only recorded (D-63). | PASS |
| V. Small verifiable milestones; sanitizers as gates | Every program is a test on every preset, with sanitizers, the verifier and valgrind. Each stage below ends in something runnable or reviewable, and the feature can stop after any stage. | PASS |
| VI. No durations | Only S/M/L sizes and dependency order. | PASS |
| VII. Evidence and traceability | D-55..D-64 recorded. Friction IDs trace to report entries, then to W-xx or D-xx (D-61, D-62). Claims carry evidence labels. External idioms are checked when the report cites them (D-64). | PASS |
| VIII. Readable names | Only approved names are used. Local helpers and stubs use full words. New names enter section K as `proposed` and are not used in this feature (FR-012, FR-014). `quux-name-check` covers `programs/ergonomics/` (D-55). | PASS |

Post-design re-check: PASS. The one design point that touches code is the move of the file runner
into the session module (D-56). It is behavior-preserving, covered by the existing REPL and runner
tests, and needed so that the verifier can reach the programs.

## Phase Template (from `docs/planning-prompts/00-base.md`)

### 1. Objective and main question

Which parts of the core vocabulary are awkward, missing or misleading in everyday sequential code?
The study answers this with evidence that is concrete enough for the owner to accept or reject
each change before Phase 3 freezes the vocabulary into the VM.

### 2. Deliverables and stopping point

**Deliverables**

- `programs/ergonomics/`: the index README and ten program directories. Each has `main.qx`, an
  optional `input.txt`, `expected-output.txt`, an optional `expected-exit-status.txt`, and a
  `README.md` (D-55, D-59).
- `tests/ergonomics/`: the test driver script and the registration of ready and pending programs
  (D-56, D-57).
- The `quux-test run` mode, and `quux_session_run_file` shared with `quux` (D-56).
- Bug fixes found by the programs, each with a regression test (FR-009).
- `docs/ergonomics-report.md` with the friction entries, the mistakes log, the bugs found, the
  timings and the owner decision list (D-61).
- New entries in `docs/naming.md` section K, `docs/spec/open-questions.md` (W12 onward),
  `docs/decisions.md` and this feature's research (D-62).

**Stopping points** (in order; the feature can stop at any of them with something useful):

1. After the infrastructure and program 01: the pipeline works end to end. The program runs,
   its test passes, its verified run passes, and the name check passes.
2. After the ten programs: executable documentation of the core, with the raw friction notes.
3. After the owner review checkpoint: the friction notes are confirmed by the owner (FR-009a).
4. After the report: classified evidence and the decision list.
5. After the owner's answers: decisions recorded, and W-xx entries closed or kept open with
   reasons.

### 3. Task outline (dependency order)

`tasks.md` (generated by `/speckit-tasks`) is authoritative. This outline fixes the order and the
sizes.

| ID | Size | Task |
|---|---|---|
| T3.1 | S | Move the file runner into `quux_session_run_file`; add `quux-test run` with the verifier options; add the mode to `specs/002-blocking-prototype/contracts/command-line.md` (D-56). |
| T3.2 | S | `tests/ergonomics/` driver and registration lists; `programs/ergonomics/README.md` index; program README template (D-55..D-57). |
| T3.3 | M | Program 01, nested data, end to end. Expected output derived first (D-60). This is stopping point 1. |
| T3.4 | M | Programs 02 and 03, the text pair, with stubs in 03 (D-58). |
| T3.5 | M | Program 04, binary search tree and token-vector parser. |
| T3.6 | M | Programs 05 and 06, the error-handling pair. |
| T3.7 | M | Program 07, macros. |
| T3.8 | S | Program 08, namespaces. |
| T3.9 | S | Program 09, numerics. |
| T3.10 | M | Program 10, common mistakes, including the load-time mistakes recorded for the report. |
| T3.11 | S–M per bug | Bug fixes found by T3.3–T3.10, each with a regression test (FR-009). A fix larger than M becomes a feature 002 task, and the program stays `pending` (risk R3). |
| T3.12 | S | Owner review checkpoint: the owner reads the programs and friction notes; their additions, disputes and withdrawals are recorded (FR-009a). |
| T3.13 | L | `docs/ergonomics-report.md`: merge and classify, write the recommendations and rationales (checking cited idioms on the web, D-64), the mistakes log, the bugs and the timings. |
| T3.14 | S | Section K proposals and W12+ entries for every add or rename; the decision list (at most 15 items). |
| T3.15 | S | Record the owner's answers: D-xx for approved items, close or keep each W-xx entry, set the section K statuses (D-62); apply "document" recommendations to the specification text. |

T3.4–T3.10 depend only on T3.1–T3.3 and can be done in any order. Each is independent of the
others.

### 4. Acceptance criteria

Each criterion is an observable command or check (see [quickstart.md](./quickstart.md)).

- `ctest --preset debug -L ergonomics` runs 16 tests (8 programs × plain and verified), plus
  8 valgrind tests in the Linux debug build, and all pass. `ctest -N -L pending`
  lists programs 03 and 06 (SC-001, SC-002).
- The same label passes in `asan-ubsan`, `tsan`, `release` and the gcc build, on macOS, in the
  container and in CI. The `verified` variants pass. Valgrind reports no errors on Linux (SC-002).
- `quux-name-check` passes with `programs/ergonomics/` present (SC-003).
- Editing one character of any `expected-output.txt` makes exactly that test fail (SC-007).
- Every `PNN-Fk` with status `kept` appears in exactly one `E-NN` entry, and every `E-NN` has at
  least one source (SC-004). This is checked by a listing script in the quickstart.
- Every `E-NN` with recommendation add or rename links a W-xx or D-xx entry (SC-005).
- The decision list has at most 15 `K-NN` items (SC-006).

### 5. Dependencies on earlier phases

- Feature 001: specification sections 01–05, 12 and 14, `docs/naming.md`, D-01..D-28.
- Feature 002 as merged: the Phase 1 core, `quux`, `quux-test`, `quux-name-check`, the presets,
  the local check and CI (D-29..D-54).
- Not depended on: feature 002's User Stories 3–5, which are unmerged (FR-015).

### 6. Technical risks and mitigation

The risks R1–R5 in [research.md](./research.md#risks-found-during-research) apply. In summary:

| # | Risk | Mitigation |
|---|---|---|
| R1 | A future feature 002 program loop picks up `programs/ergonomics/` | Match `programs/[0-9][0-9]-*` or list the programs explicitly; recorded for feature 002. |
| R2 | `main` must be in the last namespace of the file | Record it as friction in program 08. |
| R3 | A bug fix grows the feature | Fixes up to size M only; otherwise the program stays `pending` and the bug becomes a feature 002 task. |
| R4 | The friction reflects the writer's habits | Owner review checkpoint before classification; origin recorded. |
| R5 | Error messages differ between platforms | Checked on three targets when program 10 is first run; instability is a bug. |

### 7. Design decisions closed

D-55..D-64, recorded in [research.md](./research.md) and indexed in `docs/decisions.md`.
Inherited by Phase 3:

- D-55 and D-60: the programs and their expected outputs become a regression suite for the VM;
- D-56: the VM sits behind `quux_session_run_file`;
- D-62: approved vocabulary changes.

## Project Structure

### Documentation (this feature)

```text
specs/003-core-ergonomics/
├── spec.md
├── plan.md              # this file
├── research.md          # D-55..D-64 and risks
├── data-model.md        # program, friction note, report entry, mistake, proposed name, decision item
├── quickstart.md        # how to run and check everything
├── contracts/
│   ├── program-layout.md     # program directory, README sections, stubs
│   ├── test-driver.md        # quux-test run, the CMake driver, labels
│   └── friction-report.md    # report structure, IDs, decision list
├── checklists/requirements.md
└── tasks.md             # /speckit-tasks output (not created here)
```

### Source Code (repository root)

```text
programs/ergonomics/
├── README.md                     # index: area, status, pair, one line each
├── 01-nested-data/               # main.qx, expected-output.txt, README.md
├── 02-text-prepared-input/       # + input.txt
├── 03-text-raw/                  # pending; + input.txt (raw text as a quux string)
├── 04-tree-and-parser/
├── 05-errors-prepared-input/     # + input.txt
├── 06-errors-reading-text/       # pending; + input.txt
├── 07-macros/
├── 08-namespaces/
├── 09-numerics/
└── 10-common-mistakes/

tests/ergonomics/
├── CMakeLists.txt                # ready and pending lists; labels; TIMEOUT 30
└── run_program_test.cmake        # runs quux or quux-test run; compares output and status

source/harness/quux_session.{h,c} # + quux_session_run_file (moved from source/main/quux.c)
source/main/quux.c                # calls quux_session_run_file
source/main/quux_test.c           # + run mode with verifier options

docs/ergonomics-report.md         # new
docs/naming.md                    # + section K (proposed names)
docs/spec/open-questions.md       # + W12 onward
docs/decisions.md                 # + D-55..D-64, then the owner's D-xx
CMakeLists.txt                    # + add_subdirectory(tests/ergonomics)
```

**Structure Decision**: programs go under the existing `programs/` tree, so that the name check
covers them with no change. Tests sit beside the other suites under `tests/`. The only C change
outside bug fixes is in the session module and `quux-test`.

## Complexity Tracking

No constitution violation needs justification.
