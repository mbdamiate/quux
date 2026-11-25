# Research: Core Ergonomics Study (feature 003)

Decisions continue the project numbering after D-54. Each one lists the alternatives that were
considered and says which later phases inherit it.

The study's own findings come later. They are the friction points, the recommendations and the
proposed names, and they are produced during implementation in
`docs/ergonomics-report.md`. This file records only how the study is run.

## Starting point

What `main` (commit `c8b54a6`) gives the study, checked in the repository on 2026-11-25:

- **The file runner.** `quux FILE.qx [INPUT]` evaluates the file, then reads `INPUT` as quux data
  from the command-line argument (`nothing` if absent). It resolves `main` in the namespace that
  is current at the end of the file and calls `(main input)`.
  - On a normal return, it prints the returned value in canonical form followed by a newline, and
    exits with status 0.
  - On an error, it prints `!! :kind: message` on standard output and exits with status 1.

  `[FACT: source/main/quux.c, run_file]`
- **The core vocabulary.** The core functions and the prelude macros are those of section 02 and
  section 04, about 75 primitives plus 9 macros. `[FACT: source/evaluator/quux_core_functions.c,
  quux_core_prelude.qx]`
- **Strings.** The core has no string operations beyond `to-string` and `count`: nothing for
  splitting, joining, characters or substrings. It also has no function that reads data from a
  string at run time, and `read` is not an approved name. `[FACT: docs/naming.md sections B–J]`
- **The name check.** `quux-name-check` walks `programs/` recursively. It checks every `.qx` file
  of one directory together, as one program (at most 16 files), and every quux block of
  `docs/spec/`. `[FACT: source/main/quux_name_check.c, check_directory]`
- **CI and the local check.** Both run `ctest --label-exclude pending`, so they pick up any new
  label without a change. Valgrind runs only the tests labeled `valgrind`.
  `[FACT: .github/workflows/build-and-test.yml; tools/local-check.sh; tools/ci/run-valgrind.sh]`
- **The root verifier.** It is reachable only through `quux-test` (`--verify-roots`,
  `--verify-roots-every-allocation`), and today only for the `examples` mode.
  `[FACT: source/main/quux_test.c]`
- **The next open-question number** in `docs/spec/open-questions.md` is W12.

## D-55 — One directory per ergonomics program

- **Decision**: each program lives in `programs/ergonomics/NN-short-name/` with these files:

  | File | Required | Content |
  |---|---|---|
  | `main.qx` | yes | the program; defines `main` in the namespace that is current at the end of the file |
  | `input.txt` | no | quux data passed as the runner's `INPUT` argument, for programs that take input |
  | `expected-output.txt` | yes | the exact standard output, including the final printed value or the `!!` line |
  | `expected-exit-status.txt` | no | the expected exit status; absent means 0 |
  | `README.md` | yes | what it evaluates, status, and the "Friction observed" section (contract: [program-layout.md](./contracts/program-layout.md)) |

  `programs/ergonomics/README.md` is the index of the programs, with their areas, status and
  pairs.
- **Rationale**:
  - The name check groups the `.qx` files of one directory as one program. If several programs
    shared one directory, a name defined in one would hide an undefined name in another.
  - Keeping the input and the expected result as plain text means the test compares exactly what
    a user sees.
  - The spec's "a .qx file with expected output" is kept: the program is still one `.qx` file, and
    its expected result sits beside it.
- **Alternatives considered**:
  - Flat files `programs/ergonomics/NN-name.qx` with `NN-name.expected`. Rejected: this defeats
    the per-program grouping of the name check (above).
  - Reuse the `expected.qx` assertion format of the reference programs. Rejected: that harness is
    built for concurrency assertions and arrives with feature 002's User Story 3, which is not on
    `main`. FR-015 forbids depending on it.
- **Inherited by**: Phase 3 (the VM must run these programs unchanged; see D-62).

## D-56 — Test driver and `quux-test run`

- **Decision**: there are two ways to run a program as a test.
  - **Plain run.** A CMake script `tests/ergonomics/run_program_test.cmake` runs
    `quux main.qx [contents of input.txt]`. It compares standard output byte for byte with
    `expected-output.txt`, and the exit status with `expected-exit-status.txt` (or 0). It is
    labeled `ergonomics`.
  - **Verified run.** The same script, run through a new mode `quux-test [--verify-roots |
    --verify-roots-every-allocation] run FILE.qx [INPUT]`, is labeled `ergonomics;verified`. This
    meets User Story 2 scenario 4. The mode is chosen per program by the `Verifier` line of its
    README: `every allocation` by default, or `amortized` with a reason (for programs too slow to
    verify at every allocation).

  The file-running code moves from `source/main/quux.c` into the session module as
  `quux_session_run_file`, so that `quux` and `quux-test run` share it. This changes no
  behavior. Programs are registered from two lists in `tests/ergonomics/CMakeLists.txt`, ready and
  pending, the same pattern as the example suite.
- **Rationale**:
  - The plain run tests exactly the user's command.
  - The verified run reuses the verifier already wired for the examples, so a missing root in an
    everyday path fails here too.
  - Sharing one function keeps the two runs from drifting apart.
- **Alternatives considered**:
  - An environment variable read by `quux` to enable the verifier. Rejected: it adds a hidden
    switch to the user-facing runner.
  - Verify only through the examples. Rejected: the programs reach evaluator paths the examples do
    not.
- **Scope note**: this is a change to test tooling, not to the language. FR-009 limits changes to
  the *language runtime* to bug fixes. The tooling change is required by FR-006 and US2.
- **Inherited by**: Phase 3 (the VM replaces the evaluator behind `quux_session_run_file`).

## D-57 — No CI or local-check change; the label is picked up automatically

- **Decision**: CI and `tools/local-check.sh` are not edited. They already run every label except
  `pending`, so registering the tests is enough to meet FR-008. In addition:
  - the `debug` tests on Linux get the `valgrind` label, so that `tools/ci/run-valgrind.sh` also
    runs the programs under valgrind;
  - every program test has a CTest `TIMEOUT` of 30 seconds, so a hang fails instead of blocking.

  The acceptance check is that the CI log and `build/local-check.log` list the `ergonomics` tests.
- **Rationale**: the label-exclusion design of D-53 was chosen exactly so that new suites need no
  workflow edits.
- **Alternatives considered**: a separate CI job for ergonomics. Rejected: it duplicates the preset
  matrix with no gain.
- **Inherited by**: none.

## D-58 — Pending variants run with local stubs that raise

- **Decision**: a pending variant (FR-005a) is complete code, written only with approved names.
  - Each missing primitive is a **local stub**: a helper defined in the program with a full-word
    name. It raises `(error :user "missing: <what>" {:needed <description>})`.
  - `expected-output.txt` holds the output the program *will* produce once the primitive exists.
    It is derived by hand from the intended behavior.
  - The test is registered under `pending`, so CI and the local check skip it. Run by hand, it
    fails at the first stub, and its `!!` line names the missing piece.
  - The proposed public name for the primitive goes to `docs/naming.md` section K. The stub's
    local name is not a proposal.
- **Rationale**:
  - The name check passes, because the stubs are defined names (SC-003).
  - The missing piece is named in two places: the stub's message and the README (FR-007).
  - A later feature only has to replace the stub body with the approved function to turn the test
    on.
- **Alternatives considered**:
  - Call the missing function by its proposed name. Rejected: this breaks the name check and
    Principle VIII.
  - Leave the gap as a comment. Rejected: the program could then neither run nor fail in a way
    that names the gap.
- **Inherited by**: none.

## D-59 — Program list and numbering

- **Decision**: ten programs, numbered in this order. The order follows FR-001's areas, and each
  pair is kept together.

  | # | Directory | Area | Variant |
  |---|---|---|---|
  | 01 | `01-nested-data` | 1, nested data transformation | runnable |
  | 02 | `02-text-prepared-input` | 2, text processing | runnable; input already split (pair with 03) |
  | 03 | `03-text-raw` | 2, text processing | pending over raw text (pair with 02) |
  | 04 | `04-tree-and-parser` | 3, recursion and patterns | runnable |
  | 05 | `05-errors-prepared-input` | 4, error handling | runnable; user text already read as data (pair with 06) |
  | 06 | `06-errors-reading-text` | 4, error handling, reading text and `:syntax` | pending (pair with 05) |
  | 07 | `07-macros` | 5, user-written macros | runnable |
  | 08 | `08-namespaces` | 6, namespaces | runnable |
  | 09 | `09-numerics` | 7, numerics | runnable |
  | 10 | `10-common-mistakes` | 8, common mistakes | runnable |

  Program 04 parses arithmetic expressions given as **vectors of tokens**, such as
  `[1 :plus 2 :times 3]`, because tokenizing a string is impossible today (above). The tokenizing
  gap is recorded in the friction notes of programs 03 and 06, not repeated as a new pending
  program.
- **Rationale**:
  - The spec's ten-program assumption is kept.
  - Area 3 stays runnable without depending on the text gap.
- **Alternatives considered**: a token-string parser in program 04. Rejected: it would make the
  only recursion program pending, and the study would lose its pattern evidence.
- **Inherited by**: none.

## D-60 — Expected results are derived from the specification, then confirmed

- **Decision**: the expected output of each program is written **before** it is first run. It is
  derived from the specification sections (01–05, 12, 14) and the printer's canonical form.
  Running the program then confirms it. A mismatch is investigated, never pasted over:
  - **The interpreter disagrees with the specification**: this is a bug. It is fixed with a
    regression test in `tests/unit/` or in a specification example (FR-009), and recorded in the
    report's "Bugs found" section.
  - **The specification is ambiguous**: this is a new W-xx entry (spec edge case).
  - **The program is wrong**: the program is fixed, and the mistake goes into the mistakes log if
    the interpreter's message was involved (FR-010a).
- **Rationale**: copying the observed output would make the tests check the interpreter against
  itself, and the study would find no bugs `[OPINION]`.
- **Alternatives considered**: snapshot the output on the first run. Rejected for that reason.
- **Inherited by**: Phase 3 (the expected outputs become a regression suite for the VM).

## D-61 — Formats of the friction notes, the report and the decision list

- **Decision**:
  - **Friction notes** in each program README form a table with these columns:
    - ID `PNN-Fk`;
    - location (a top-level form name, or `main.qx` lines);
    - what was awkward, verbose or impossible;
    - workaround used;
    - origin (`writer` or `owner`);
    - status after review (`kept`, `disputed: reason`, `withdrawn`).
  - **`docs/ergonomics-report.md`** has these parts:
    1. method and counts;
    2. friction entries `E-NN` (merged, each listing its `PNN-Fk` sources, class, recommendation,
       rationale with an evidence label, and its link to a W-xx or D-xx);
    3. the mistakes log `M-NN` (FR-010a);
    4. bugs found;
    5. performance measurements;
    6. the owner decision list `K-NN` (FR-013), at most 15 entries.

  The exact columns are in [contracts/friction-report.md](./contracts/friction-report.md).
- **Rationale**: stable IDs let each entry be traced from the program to the report to the
  decision (SC-004, SC-005) without prose matching.
- **Alternatives considered**: free-form notes. Rejected: they cannot be checked for coverage.
- **Inherited by**: the later feature that implements approved additions starts from the `K-NN`
  list.

## D-62 — How the decisions are recorded

- **Decision**:
  - Every add or rename recommendation gets a W-xx entry in `docs/spec/open-questions.md`,
    starting at W12, with "owner decision, feature 003" in its status.
  - Every new or renamed name gets a row in `docs/naming.md` section K with status `proposed`.
  - When the owner answers the decision list:
    - each approved item becomes a D-xx decision, numbered after the last one used, in this
      research file and in `docs/decisions.md`, marked "inherited by: Phase 3" when the VM
      inherits the vocabulary;
    - the matching W-xx entry is closed;
    - the section K row becomes `approved` or `rejected`.

    A rejected item is closed as "keep as is", with the owner's reason.
  - "Document" and "keep as is" recommendations need no W-xx entry. A "document" item is applied
    in this feature, as a change to the specification text only.
- **Rationale**: this follows FR-011 and FR-012 and the precedent of W4–W6 in feature 002. A
  decision ID exists only for what the owner actually decided.
- **Alternatives considered**: a D-xx entry for every recommendation, at the moment it is
  written. Rejected: a decision would then exist before the owner made it.
- **Inherited by**: Phase 3, for every approved vocabulary change.

## D-63 — Performance evidence

- **Decision**:
  - The performance evidence comes from CTest's per-test times in the `release` preset, on the
    development machine (macOS arm64).
  - A program over 1 second is a performance surprise (spec Assumptions), and so is a
    `:stack-exhausted` error on input under 10,000 elements. The mistakes made on purpose in
    program 10 are excluded: only their messages are evaluated.
  - The report lists the time of every program in a table, labeled `[FACT: ctest --preset release
    -L ergonomics, macOS arm64]`.
  - Each program prints the size of its data, so that the timing can be compared.
- **Rationale**: this needs no new tooling, and the numbers are reproducible with one command.
- **Alternatives considered**: a benchmark harness. Rejected: performance is not this feature's
  subject, and Phase 3 owns measurement.
- **Inherited by**: none.

## D-64 — How comparisons with other Lisps are used

- **Decision**: a recommendation may cite an idiom from Clojure, Scheme or Racket as evidence that
  a need is common. Examples:
  - Clojure's nested-update functions for program 01;
  - string splitting in Clojure's string library, in Scheme's SRFI-130, or in Racket's `racket/string`
    for program 03;
  - reading data from a string with Clojure's `read-string` or Racket's `read` from a string port
    for program 06.

  Each citation links its source documentation and is labeled `[FACT: link]` for what the other
  language provides. Any proposed quux name is a new full-word name. A foreign name is used only
  if it already meets Principle VIII and the owner approves it.

  The links are checked on the web when the report is written, not now, because the report is
  where they are used (constitution VII: facts are checked when the decision is made).
- **Alternatives considered**: no comparisons. Rejected: the owner asked for them where they inform
  a recommendation.
- **Inherited by**: none.

## Risks found during research

| # | Risk | Probability | Impact | Mitigation |
|---|---|---|---|---|
| R1 | Feature 002's User Story 3 adds a CMake loop over `programs/NN-*` that also picks up `programs/ergonomics/` | Medium `[ESTIMATE]` | Low | The reference-program loop in feature 002 must list its programs explicitly or match `programs/[0-9][0-9]-*`. This is recorded as a note for feature 002's tasks. |
| R2 | The namespace program defines `main` in the wrong namespace and fails with "does not define main" | High `[ESTIMATE]` | Low | This is itself a likely friction point (`main` must be in the last namespace of the file). It is recorded rather than hidden. |
| R3 | Writing a program exposes a bug in a hard area (expander, patterns), and fixing it grows the feature | Medium `[ESTIMATE]` | Medium | FR-009: fix with a regression test. If a fix is larger than size M, the program stays `pending` with the bug as its reason, and the bug becomes a feature 002 task. |
| R4 | The writer's friction notes reflect the writer's habits rather than a user's | High `[ESTIMATE]` | Medium | The owner review checkpoint (FR-009a) comes before any classification, and the origin of each friction point is recorded. |
| R5 | Error messages include details that vary between platforms (addresses, pointer widths) and break exact comparison | Low `[ESTIMATE]` | Medium | Messages are checked for stability when program 10 is first run on all three targets. Any instability is a bug in the message, not in the test. |
