# Feature Specification: Core Ergonomics Study

**Feature Branch**: `003-core-ergonomics`

**Created**: 2026-11-25

**Status**: Draft

**Input**: User description: "Core ergonomics study for quux (between Phase 1 and Phase 3). The 12
reference programs test the concurrency and distribution design; they barely exercise everyday use
of the core. This feature writes a second, smaller set of programs that use the sequential core the
way a user would, and records every point of friction as evidence for naming and library decisions
before the Phase 3 VM inherits the core vocabulary." (full text: seven program areas, per-program
friction sections, a classified friction report, an `ergonomics` test label, and the constraints on
base branch, naming, traceability and sizing.)

## Context

The 12 reference programs (`programs/01-…` to `programs/12-…`) are the acceptance suite for the
concurrency and distribution design. They use the sequential core only as glue. Phase 3 (the VM)
will inherit the core vocabulary: special forms, core functions, macros, error kinds and printed
forms. Changing a name or adding a function after Phase 3 costs more than before. This feature
collects evidence about everyday use of the core while that cost is still low.

The study has two kinds of output:

- **Programs**: small, realistic programs written against today's Phase 1 interpreter, with exact
  expected output.
- **Evidence**: a record of every place where writing those programs was awkward. Each record
  becomes a classified recommendation for the owner.

**Terms**: a *friction point* is an awkward spot found while writing a program. It is recorded
twice:
- as a *friction note* (`PNN-Fk`) in the program's README;
- after the owner review, inside a *report entry* (`E-NN`), which may merge several notes.

The programs are written **only with approved names** (constitution Principle VIII,
`docs/naming.md`), even when that is awkward. The awkwardness is the data being collected.

## Clarifications

### Session 2026-11-25

- Q: After the owner approves new names or additions, does this feature implement them and revise
  the programs? → A: No. The feature ends at the owner decision list. Approved additions and the
  revision of the programs belong to a later feature (FR-014; User Story 5 removed).
- Q: How are the parts the core cannot express handled (splitting text in area 2, reading text
  into data to handle `:syntax` in area 4)? → A: Each such area gets two programs: a runnable one
  whose input arrives already split as quux data through the runner's `INPUT`, and a `pending`
  one that takes the raw text and names the missing functions. Both count toward the seven to
  ten programs (FR-005a).
- Q: Where does the evidence on error message quality come from? → A: From a dedicated
  "common mistakes" program with about ten typical mistakes and their expected messages, plus a
  log in the report of the real mistakes made while writing the programs, with the message the
  interpreter gave for each (FR-001 area 8, FR-010a).
- Q: When does the owner review the programs and friction notes? → A: At a review checkpoint
  after all programs are written and before the report. The owner may add, dispute or withdraw
  friction points. The decision list stays at the end (FR-009a).

## User Scenarios & Testing *(mandatory)*

The "user" of this feature is the project owner, acting in two roles:

- **quux programmer**: reads and runs the programs.
- **language designer**: decides on names and library additions before Phase 3.

### User Story 1 - Everyday programs that run today (Priority: P1)

As a quux programmer, I can read and run a set of small programs that do ordinary sequential
work. The areas are:

- nested data transformation;
- text processing;
- recursion with patterns;
- error handling;
- user-written macros;
- namespaces;
- numerics;
- common mistakes and their error messages.

Each program runs with the Phase 1 interpreter (`quux program.qx`) and produces exactly the output
written next to it. This shows what quux code looks like for everyday tasks, not only for
concurrency.

**Why this priority**: the programs are the source of all the evidence. Without them there is
nothing to classify or decide. A set of runnable programs is also useful on its own, as executable
documentation of the core.

**Independent Test**: run each program and compare its output with its expected output. This is
done by hand first, and then by the `ergonomics` test label (User Story 2).

**Acceptance Scenarios**:

1. **Given** the Phase 1 interpreter built from `main`, **When** the owner runs any ergonomics
   program that needs nothing missing, **Then** it prints exactly the expected output, returns
   the expected value, and exits with its expected status (`expected-exit-status.txt`, 0 when
   absent). A program whose purpose is to show an uncaught error, such as program 05, expects
   status 1.
2. **Given** a program, **When** the owner reads it, **Then** it uses only names approved in
   `docs/naming.md` (or names it defines itself), with no reader shorthands, and the name check
   accepts it.
3. **Given** the eight program areas listed in FR-001, **When** the set is complete, **Then** every
   area has at least one program, and the set has between seven and ten programs.
4. **Given** a program that needs a function or form the core does not have, **When** it is
   written, **Then** the needed piece is named in the program's "Friction observed" section, and:
   - the program either works around the gap with a helper written in quux, or
   - it stays pending (User Story 2) with that piece named as the reason.

---

### User Story 2 - Programs are checked automatically (Priority: P1)

As the owner, I know that the ergonomics programs keep working as the interpreter changes. They
run on every push and in CI under their own test label, separate from the phase acceptance suite.

**Why this priority**: an example that is not checked goes stale. The programs must stay true
while Phase 1 continues (US3–US5 of feature 002) and while Phase 3 replaces the evaluator.

**Independent Test**:

- run the tests with the `ergonomics` label and see one test per program, each passing or marked
  pending;
- break the expected output of one program and see that test fail.

**Acceptance Scenarios**:

1. **Given** the ergonomics programs, **When** the tests with the `ergonomics` label run, **Then**
   each runnable program is one test that passes only if its output and exit status match exactly.
2. **Given** a program that needs a missing piece, **When** the tests run, **Then** that program
   carries the `pending` label, so it is skipped by CI and by the local pre-push check, and its
   pending reason names the missing piece.
3. **Given** CI and the local check (`tools/local-check.sh`), **When** either runs, **Then** it
   includes the `ergonomics` label. Ergonomics programs are not counted as phase acceptance
   programs.
4. **Given** the root verifier mode used for the specification examples, **When** the ergonomics
   programs run under it, **Then** they produce the same output. A difference is a runtime bug,
   not a friction point.

---

### User Story 3 - A classified friction report (Priority: P2)

As the language designer, I read one report that lists every friction point found while writing
the programs. Each point has:

- a class;
- the program and place where it was found;
- a recommendation with its rationale.

**Why this priority**: the report turns scattered notes into decisions. It depends on the
programs (User Story 1), but it is the main input for the owner's decisions before Phase 3.

**Independent Test**: take any friction note from any program and find it in the report with a
class, a recommendation and a rationale. The reverse must also hold: every report entry points
back to at least one program.

**Acceptance Scenarios**:

1. **Given** all programs written, **When** the owner review checkpoint happens, **Then** the
   owner's additions, disputes and withdrawals are recorded in the programs' friction notes
   before any classification starts.
2. **Given** the programs' "Friction observed" sections, **When** the report is written, **Then**
   every friction point appears in it exactly once. Similar points from different programs are
   merged, and each merged point lists all of its sources.
3. **Given** a friction point, **When** it is classified, **Then** it has exactly one class from
   this list:
   - missing function;
   - awkward name;
   - missing syntax or pattern;
   - error message quality;
   - performance surprise.
4. **Given** a classified point, **When** a recommendation is written, **Then**:
   - it is one of add, rename, document, or keep as is;
   - its rationale is labeled `[FACT: source]`, `[OPINION]` or `[ESTIMATE]`;
   - it may cite comparable idioms from Clojure, Scheme or Racket, without adopting their names.
5. **Given** a recommendation that would change the language (add or rename), **When** the report
   is final, **Then** it is recorded in one of two places:
   - a W-xx entry in `docs/spec/open-questions.md`, if the owner still has to decide;
   - a D-xx decision (numbered after D-54) in the feature's research and in `docs/decisions.md`.

   Either one is marked "inherited by: Phase 3" when the VM depends on it.

---

### User Story 4 - A short decision list for the owner (Priority: P2)

As the owner, I get a short list of the naming and library decisions to approve before Phase 3:

- each proposed new or renamed name, entered in `docs/naming.md` section K as `proposed`;
- each library addition recommended by the report.

**Why this priority**: this is the feature's stated success outcome. It is ranked below the report
because it is a summary of it.

**Independent Test**: the owner can approve or reject every item on the list from the list alone,
without re-reading the programs. After the review, every item in section K is `approved`,
`rejected`, or `proposed` with its W-xx entry kept open and a recorded reason for deferring it.

**Acceptance Scenarios**:

1. **Given** the report, **When** the decision list is produced, **Then** it has one entry per
   proposed name or addition, and each entry shows:
   - the proposal;
   - the friction it removes;
   - the programs affected;
   - the alternatives considered.
2. **Given** a proposed name, **When** it is added to `docs/naming.md` section K, **Then** its
   status is `proposed`, and no program, spec text or code uses it until the owner approves it.
3. **Given** the owner's answers, **When** they are recorded, **Then** each answer is one of:
   - approve: the section K row becomes `approved` with its decision ID, and the W-xx entry is
     closed;
   - reject: the row becomes `rejected`, and the W-xx entry is closed as "keep as is" with the
     reason;
   - defer: the row stays `proposed`, and the W-xx entry stays open with the reason. The feature
     can close with deferred items.

---

### Out of scope: revising the programs after approval

The owner chose to end this feature at the decision list (clarification Q1, 2026-11-25). This
feature does not implement approved additions or rewrite the programs with newly approved names.
A later feature does that, starting from the decision list. In that feature, programs that are
pending only because of an approved addition move to the `ergonomics` label, and each friction
note records whether the change removed the friction.

---

### Edge Cases

- **A program exposes an interpreter bug** (a crash, a sanitizer report, wrong output, or output
  that differs under the root verifier). The bug is fixed in this feature with a regression test,
  and it is recorded as a bug, not as a friction point. A bug that cannot be fixed within the
  feature keeps the program `pending` with the bug as its reason.
- **A friction point is a specification ambiguity rather than a missing feature** (two readings of
  section 02 are possible). It becomes a W-xx open question instead of a library recommendation.
- **The owner rejects a proposed name.** The programs keep their approved-name version. The
  report records the rejection and its reason, and the friction point is closed as "keep as is".
- **A gap can be filled by a helper written in quux.** The program uses the helper and stays
  runnable. The friction is still recorded as "missing function", because a user had to write it.
- **Helpers defined inside a program.** Their names are local to the program, not public quux
  names, so they do not need approval. They must still be full English words (Principle VIII),
  and the name check must accept them.
- **Expected output depends on the iteration order of maps or sets.** The canonical order (section
  01) makes it deterministic. A program whose output still varies between runs is a bug.
- **Deep recursion reaches the stack limit** (`:stack-exhausted`, limit 10000). This is recorded as
  a performance surprise, or as a missing pattern if a tail-recursive form was awkward to write.
  The mistakes made on purpose in program 10, including deep recursion, are never performance
  findings. Only their error messages are evaluated.
- **The same friction appears in several programs.** It is merged into one report entry that lists
  every source, so the decision list does not count it twice.
- **A program can only be completed with a name that is still proposed.** It stays `pending`
  until the owner decides. It is not rewritten with an unapproved name in the meantime.

## Requirements *(mandatory)*

### Functional Requirements

**Programs**

- **FR-001**: The feature MUST deliver between seven and ten ergonomics programs in
  `programs/ergonomics/`, covering at least these areas:
  1. nested data transformation: updating maps inside vectors inside maps, with `thread-first`
     and `thread-last`;
  2. text processing: a word count, a simple comma-separated values (CSV) parse, and formatted
     output;
  3. recursion and patterns:
     - a persistent binary search tree with insert, lookup and in-order walk;
     - a small recursive-descent parser;
     - both using vector patterns, map patterns and `:rest`;
  4. sequential error handling: `try`/`catch`/`finally`, user errors carrying `:data`, and
     reading user-supplied text with handling of `:syntax`;
  5. user-written macros: at least one non-trivial macro (a counted loop or a tiny
     domain-specific language) built with `quasi-quote`, `unquote` and `unquote-splicing`
     written out in full;
  6. namespaces: a program split across two or three namespaces, with `:import` and qualified
     names;
  7. numerics: integers versus floating-point, and `:overflow` and `:inexact-division` met in a
     realistic calculation (for example simple statistics or fraction-like arithmetic);
  8. common mistakes: a dedicated program that makes about ten typical mistakes, each caught and
     its kind and message printed, so that every message is part of the expected output. The
     mistakes include:
     - a wrong number of arguments;
     - an undefined name;
     - a pattern that does not match its value;
     - `get` on a value that is not a map;
     - calling a value that is not a function;
     - arithmetic on a non-number;
     - `element-at` out of range;
     - deep non-tail recursion.

     A mistake that a program cannot catch itself, because it is raised while the file is
     loaded (for example `unquote` outside `quasi-quote`), is recorded in the report with its
     message instead.
- **FR-002**: Each program MUST be runnable with the Phase 1 file runner (`quux program.qx`,
  calling `main`). It MUST produce a deterministic printed output, a returned value and an exit
  status, all of which are written down as its expected result.
- **FR-003**: Each program MUST be accompanied by:
  - a statement of what it evaluates (which core features it exercises);
  - its expected observable result, precise enough to be compared automatically;
  - a "Friction observed" section that lists every awkward, verbose or impossible spot, with the
    location in the program.
- **FR-004**: Programs MUST use only names approved in `docs/naming.md`, names defined by the
  program itself, and no reader shorthands. The name check MUST pass on every program.
- **FR-005**: When a program needs a function, form or name that does not exist or is not
  approved, it MUST NOT use an unapproved name. Instead it MUST either:
  - work around the gap with a helper written in quux, recording the friction; or
  - be marked pending, with the missing piece named as the reason.
- **FR-005a**: When an area cannot be expressed at all with the approved core (no helper can be
  written, because the needed primitive is missing), it MUST be covered by a pair of programs:
  - a **runnable variant**: its input arrives already split as quux data through the runner's
    `INPUT` (for example a vector of words, or a vector of rows of fields). It exercises
    everything that can be written today, such as counting, grouping, error handling and
    formatting;
  - a **pending variant**: it takes the raw text, as a user would have it. It is labeled
    `pending`, and its reason names each missing function.

  This applies at least to the text-processing area (area 2) and to the text-reading part of the
  error-handling area (area 4). Both variants count toward the seven to ten programs. The
  friction notes of the pair point to each other.

**Test integration**

- **FR-006**: Each runnable program MUST be one automated test under a new label `ergonomics`.
  The test passes only when the printed output, the returned value and the exit status all match
  the expected result exactly.
- **FR-007**: A program that cannot run yet MUST carry the `pending` label, with its reason
  recorded next to the program. Tests labeled `pending` MUST be skipped by CI and by the local
  pre-push check, as for the specification examples.
- **FR-008**: CI and `tools/local-check.sh` MUST run the `ergonomics` label on every target and
  preset where they run the specification examples. The ergonomics programs MUST NOT be counted
  as phase acceptance programs.
- **FR-009**: Runtime code MAY change only to fix a bug exposed by a program. Each fix MUST come
  with a regression test outside the ergonomics programs. Changes to test tooling that FR-006 and
  User Story 2 require (running a program under the root verifier) are allowed, provided they do
  not change the language's behavior.

**Report and decisions**

- **FR-009a**: Between the programs and the report there MUST be an owner review checkpoint. The
  owner reads the programs and their "Friction observed" sections, and may add, dispute or
  withdraw friction points. The report MUST NOT be written until the checkpoint is passed. The
  report records:
  - for each friction point, whether it came from the writer or from the owner;
  - every disputed point, with the owner's reason;
  - withdrawn points, which are kept out of the report.

- **FR-010**: The feature MUST deliver `docs/ergonomics-report.md`, which:
  - lists every friction point from every program exactly once, merging duplicates and keeping
    every source;
  - classifies each point as exactly one of: missing function, awkward name, missing syntax or
    pattern, error message quality, or performance surprise;
  - gives each point a recommendation (add, rename, document, or keep as is) with a labeled
    rationale.
- **FR-010a**: The report MUST include a **mistakes log**: every real mistake made while writing
  the programs that produced an interpreter error, with:
  - the code as written;
  - the kind and message the interpreter gave;
  - whether the message pointed to the cause.

  Entries where the message did not point to the cause become "error message quality" friction
  points.
- **FR-011**: Every recommendation that changes the language (add or rename) MUST be recorded as
  a W-xx entry in `docs/spec/open-questions.md` or as a D-xx decision (numbered after D-54) in the
  feature's research and in `docs/decisions.md`. It MUST be marked "inherited by: Phase 3" when the
  VM depends on it.
- **FR-012**: Every proposed new or renamed name MUST enter `docs/naming.md` in a new section K
  with status `proposed`. It MUST NOT be used anywhere until the owner approves it.
- **FR-013**: The report MUST end with an owner decision list. It has one entry per proposed name
  or library addition, each showing the proposal, the friction it removes, the affected programs
  and the alternatives considered.
- **FR-014**: This feature MUST NOT implement library additions or renames, even approved ones
  (clarification Q1: option A). The programs stay in their approved-names-only form. A program that
  needs a missing piece stays `pending`, with that piece named as its reason. Implementing approved
  additions and revising the programs belongs to a later feature that starts from the decision
  list.
- **FR-015**: The feature's base branch MUST be an up-to-date `main`, and its pull request MUST
  target `main`. It MUST NOT depend on unmerged work from other branches, including the remaining
  feature 002 stories.

**Process constraints**

- **FR-016**: Claims in the report and the research MUST be labeled `[FACT: source]`,
  `[OPINION]` or `[ESTIMATE]`. Tasks MUST be sized S, M or L, with no durations or dates.

### Key Entities

- **Ergonomics program**: a small quux program for one area of everyday use. Attributes:
  - area;
  - what it evaluates;
  - expected result (printed lines, returned value, exit status);
  - status (runnable or pending, with a reason);
  - for paired areas, its variant (runnable with input already split, or pending over raw text)
    and its partner program;
  - "Friction observed" notes.
- **Friction point**: one awkward spot found while writing a program. Attributes:
  - source programs and locations;
  - origin (writer or owner, from the review checkpoint);
  - class (one of five);
  - description;
  - recommendation (add, rename, document, keep as is);
  - rationale with an evidence label;
  - link to a W-xx or D-xx entry when the language would change.
- **Proposed name**: an entry in `docs/naming.md` section K. Attributes:
  - the name;
  - what it replaces or adds;
  - status (`proposed`, `approved`, `rejected`);
  - decision ID.
- **Owner decision list**: the summary at the end of the report that the owner answers item by
  item.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Between 7 and 10 ergonomics programs exist, and each of the 8 areas in FR-001 is
  covered by at least one program.
- **SC-002**: 100% of the programs either pass under the `ergonomics` label on every target and
  preset where the specification examples run, or are `pending` with a named missing piece.
- **SC-003**: 100% of the programs pass the name check, and none uses a name whose status in
  `docs/naming.md` is not `approved`, apart from names they define themselves.
- **SC-004**: 100% of the friction points noted in the programs appear in the report with exactly
  one class, one recommendation and a labeled rationale. 100% of the report entries point back to
  at least one program.
- **SC-005**: 100% of the add and rename recommendations have a W-xx or D-xx entry.
- **SC-006**: The owner decision list has at most 15 entries, after merging duplicates. Each entry
  can be decided from the list alone, without reading the programs.
- **SC-007**: Changing one character of any runnable program's expected result makes its test
  fail. Running the tests twice in a row gives identical results for every program.

## Assumptions

- **Single-file programs.** Each program is one `.qx` file, with its expected result and notes
  beside it. The namespace program (area 6) puts its two or three `namespace` forms in that one
  file. Section 14 allows several namespaces in one file, and the Phase 1 runner loads a single
  file.
- **Expected program count.** Ten programs: one each for areas 1, 3, 5, 6, 7 and 8, and a pair
  each for areas 2 and 4 (FR-005a). This is the upper bound of seven to ten, so area 3 keeps the
  binary search tree and the recursive-descent parser in one program.
- **What the expected result is.** It is what the Phase 1 file runner shows:
  - the lines printed with `print-line`;
  - the canonical printed form of the value `main` returns;
  - the exit status (0 on return, 1 if `main` raised).

  A program needs no input, unless it is testing input handling. In that case the input is
  written next to the program and passed as the runner's `INPUT` argument.
- **Gaps known in advance.** The approved core has no string operations beyond `to-string`. There
  is no splitting, joining, trimming, character access or number formatting, and no function that
  reads quux data from a string at run time (`read` is not an approved name). The text-processing
  program (area 2) and the text-reading part of area 4 therefore come in pairs (FR-005a): a
  runnable variant with input already split as data, and a pending variant over raw text, both
  recording "missing function" friction.

  This is a finding to record, not a failure of the feature.
- **Scope of additions.** No library addition is implemented in this feature (FR-014). The owner's
  answers to the decision list are recorded, which closes or keeps open the matching W-xx
  entries. The implementation is planned as a separate feature.
- **What counts as a performance surprise.** A friction point is classified as a performance
  surprise when a program in the `release` preset takes more than one second, or hits
  `:stack-exhausted` on input sizes a user would consider small (under 10,000 elements).
  `[ESTIMATE]`: a typical sequential program of this size should finish well under that on the
  development machine.
- **What the ergonomics label runs on.** It runs on the same targets and presets as the
  specification examples: macOS arm64 and the Linux arm64 container locally, and Linux x86_64 in
  CI. The sanitizer presets are included.
- **The 12 reference programs are not touched.** They stay the phase acceptance suite. Ergonomics
  programs neither replace nor extend them.
- **Feature 002 is paused, not abandoned.** US3–US5 of feature 002 continue later, on their own
  branch. This feature uses only what is already on `main`: the Phase 1 sequential core, as
  merged.
