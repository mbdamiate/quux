# Feature Specification: quux Blocking Prototype (Phase 1)

**Feature Branch**: `002-blocking-prototype`

**Created**: 2026-11-24

**Status**: Draft

**Input**: User description: "Blocking prototype of quux in C (Phase 1) that runs the reference
programs in programs/ against the specification in docs/spec/ (feature 001). The test harness reads
each program's conditions.qx and expected.qx as quux data, following
specs/001-language-spec/contracts/reference-program.md, and every public name comes from
docs/naming.md. Stories: toolchain smoke test; core language with REPL; futures and scopes;
agents, streams and data parallelism; multi-node simulation. Constraints: H2 root convention, H1
module boundaries, H7 portability layer, full-word C identifiers. Open points to close: W4, W5, W6,
H9, H11. Success = all 12 programs run as automated tests on macOS arm64, Linux arm64 and Linux
x86_64 (CI) with no ASan/UBSan/TSan errors."

## Context

Phase 0 (feature 001) produced the language specification (`docs/spec/`), the 12 reference
programs (`programs/`) with their injected conditions and expected assertions, and decisions
D-01..D-28. Nothing has run yet. This feature produces the first implementation, a **blocking
prototype**: simple, slow and correct, whose job is to prove that the specification can be
implemented as written and that every reference program behaves as `expected.qx` says.

Its users are:

- **The implementer** (the project's single developer), who needs a runnable baseline, a test
  harness and sanitizer-clean builds on every target before investing in the VM (Phase 3).
- **The quux programmer**, who for the first time can type quux at a REPL and run a `.qx` file.

Starting reality (from feature 001): the specification and programs are complete and reviewed;
there is no C code, no build, no container and no CI yet. The repository is public on GitHub, so
hosted CI runners are available.

Governing principles: constitution v1.2.3, in particular I (C only), IV (blocking runtime
first), V (small verifiable milestones, sanitizers as gates), VII (decisions and hypotheses) and
VIII (readable names). The implementation language and the toolchain are fixed by the
constitution; they appear below only as constraints, not as design choices of this spec.

## Clarifications

### Session 2026-11-25

- Q: How is a variadic (rest) parameter written (W4)? → A: With the keyword `:rest` before the
  parameter that receives the remaining arguments, for example
  `[agent function :rest arguments]`. No reader rule is added, and the marker is a full word
  (Principle VIII).
- Q: Which destructuring is in scope for Phase 1 (W5)? → A: Vectors and maps, in `let` and in
  parameter lists. The spelling of map destructuring is proposed in `docs/naming.md` and
  approved before use.
- Q: Is a deterministic choice of the primary error required in Phase 1 (H9)? → A: No. The
  primary error stays the first failure in completion order (D-11); tests keep using
  `raises-one-of`, and deterministic replay is deferred to Phase 7. H9 is recorded with the
  evidence from the programs.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Toolchain smoke test on every target (Priority: P1)

The implementer builds a minimal program in every build configuration (debug, address and
undefined-behavior sanitizers, thread sanitizer, release) on macOS arm64, in the Linux arm64
container and in hosted CI on Linux x86_64, and sees each sanitizer catch a deliberate defect.

**Why this priority**: Every later story depends on builds that work identically on the three
targets and on sanitizers that are proven to report errors. A sanitizer that silently reports
nothing is worse than none.

**Independent Test**: Run the smoke test on each target; it passes when every configuration
builds, the clean program runs without reports, and each deliberate defect is reported by the
expected tool.

**Acceptance Scenarios**:

1. **Given** a fresh checkout on macOS arm64, **When** the implementer builds and runs the smoke
   test in each of the four configurations, **Then** all four succeed and the clean program
   produces no sanitizer report.
2. **Given** the smoke test's deliberate heap overflow, **When** it runs under the address
   sanitizer, **Then** the run fails with an overflow report.
3. **Given** the smoke test's deliberate data race, **When** it runs under the thread
   sanitizer, **Then** the run fails with a race report.
4. **Given** the deliberate leak, **When** it runs in the Linux container under the leak
   checker and under valgrind, **Then** both report the leak.
5. **Given** a push to the repository, **When** CI runs, **Then** the same configurations build
   and run on Linux x86_64, with the same results as locally. CI runs only on Linux x86_64;
   macOS arm64 and Linux arm64 are checked locally before every push.
7. **Given** a change that breaks a test on macOS arm64 or in the Linux arm64 container,
   **When** the implementer pushes, **Then** the push is blocked before it reaches GitHub.
6. **Given** the container needed extra capabilities or settings for a sanitizer to work, **When**
   the smoke test is complete, **Then** each adjustment is recorded with its reason.

---

### User Story 2 - Core language and REPL (Priority: P1)

The quux programmer types expressions at a REPL, or runs a `.qx` file, and gets the results the
specification sections 01–05 and 14 describe: reading, evaluation, closures, tail calls that do
not grow the stack, macros, numerics, persistent canonical collections with stable hashes,
errors and modules.

**Why this priority**: Every reference program is written in this core. It is the first point
where the specification meets reality, and the examples in `docs/spec/` become executable tests.

**Independent Test**: Run the examples of sections 01–05, 12 (sequential part) and 14 as
automated tests; run a REPL session; run a deep tail-recursive loop.

**Acceptance Scenarios**:

1. **Given** the REPL, **When** the programmer enters `(+ 1 2)`, **Then** it prints `3`, and an
   error is printed with its kind without ending the session.
2. **Given** a function that calls itself in tail position one million times, **When** it runs,
   **Then** it returns its result with bounded stack use.
3. **Given** two maps built by inserting the same entries in different orders, **When** they are
   compared and hashed, **Then** they are equal, have the same value hash and the same printed
   form.
4. **Given** an integer addition that overflows 64 bits, **When** it is evaluated, **Then** it
   raises `:overflow`.
5. **Given** each example in sections 01–05 and 14 marked `;=>` or `;!!`, **When** the example
   suite runs, **Then** every example produces the stated value or error kind.
6. **Given** a program that uses a name not in `docs/naming.md`, **When** the name check runs,
   **Then** it fails and names the offending identifier.

---

### User Story 3 - Futures and scopes (Priority: P2)

The quux programmer writes concurrent code with `future`, `spawn`, `wait-for`, `with-scope`,
cancellation and the combinators of section 08, on a blocking runtime (one operating-system
thread per future), and the test harness runs reference programs 01, 02, 03, 08 and 09 against
their `conditions.qx` and `expected.qx`.

**Why this priority**: Structured concurrency is the center of the language (Principle III) and
the riskiest semantics to get right. Programs 01–03, 08 and 09 exercise it without agents,
streams or nodes.

**Independent Test**: The harness runs the five programs, each with its injected conditions,
and reports each assertion of its `expected.qx` as held or not.

**Acceptance Scenarios**:

1. **Given** program 01 (parallel search with `all`), **When** the harness runs it, **Then**
   every assertion in its `expected.qx` holds, including the elapsed-time bound that proves
   parallelism.
2. **Given** program 02 (first response wins), **When** it runs, **Then** the losing futures end
   `:failed` with `:cancelled` and the scope does not fail.
3. **Given** program 03 (timeout and error propagation), **When** the injected failure happens,
   **Then** `main` raises `:user` and the slow sibling ends with `:cancelled`.
4. **Given** program 09 (deadlock), **When** two futures wait for each other, **Then** both fail
   with `:deadlock` instead of hanging.
5. **Given** any of the five programs, **When** it runs 20 times in a row under the thread
   sanitizer, **Then** every run passes with no report.

---

### User Story 4 - Agents, streams and data parallelism (Priority: P3)

The quux programmer uses agents (`update`, `transact`, `update-once`), pull streams with bounded
look-ahead, the input/output appendix (files, listeners, connections), and the deterministic data
parallelism of section 11 (`parallel-transform`, `parallel-reduce`, fork/join). The harness runs
programs 04, 05, 06, 07 and 10, and program 08 keeps passing.

**Why this priority**: These features build on futures and scopes, and each is exercised by one
or two programs. They complete the single-node language.

**Independent Test**: The harness runs programs 04, 05, 06, 07 and 10 with their conditions and
assertions.

**Acceptance Scenarios**:

1. **Given** program 04 (bank account), **When** concurrent withdrawals race, **Then** the
   balance never goes negative and the agent's final state matches `expected.qx`.
2. **Given** program 05 (parallel transform over a large vector), **When** it runs several times,
   **Then** the result and its hash are identical on every run and equal to the sequential
   version.
3. **Given** program 06 (file line stream), **When** the generated file is read through
   transform, filter and take, **Then** the printed lines match and the file is closed.
4. **Given** program 07 (fast producer, slow consumer), **When** it runs, **Then** the producer
   is never more than the stream's maximum look-ahead ahead of the consumer.
5. **Given** program 10 (chat server), **When** clients connect, send and disconnect, **Then**
   each connection runs in its own scope, messages are delivered in the room's commit order, and
   a disconnected client's scope ends with `:cancelled`.

---

### User Story 5 - Multi-node simulation (Priority: P4)

The implementer runs programs 11 and 12 against several simulated nodes inside one process, with
crashes, delays and message loss injected from `conditions.qx`, and sees `spawn-on`,
`:node-unreachable`, `:transport` and `update-once` behave as sections 12 and 13 specify.

**Why this priority**: Distribution is a later phase, but its decisions were taken early
(Principle IV). A simulation validates them cheaply before any network code exists.

**Independent Test**: The harness runs programs 11 and 12 with the `:nodes` and `:network`
conditions of their `conditions.qx`.

**Acceptance Scenarios**:

1. **Given** program 11 with node `:b` set to crash after its fourth completed task, **When** it
   runs, **Then** the pending futures on `:b` fail with `:node-unreachable` and the program
   recomputes them on another node and returns the expected result.
2. **Given** program 12 with the seventh acknowledgement dropped, **When** it runs, **Then** the
   send fails with `:transport`, the program resends with the same operation identifier, and
   `(applied-once …)` holds.
3. **Given** program 12's stream between nodes, **When** the consumer is slower than the
   producer, **Then** the look-ahead bound holds across the simulated network.
4. **Given** a closure that captures an open file, **When** it is sent with `spawn-on`, **Then**
   the send fails at the sender with `:not-serializable`.

---

### Edge Cases

- A `conditions.qx` with an unknown key, or an `expected.qx` with an unknown assertion form: the
  harness rejects the program instead of running it with the injection silently disabled.
- A program that never terminates (for example a missed cancellation): the harness stops it at
  `:bounds :wall-clock-milliseconds` and reports a failure naming the program, not a hang of the
  whole suite.
- Racing failures in one scope: assertions written with `raises-one-of` accept any of the listed
  kinds; the primary error is the first failure in completion order (H9, deferred to Phase 7).
- A future that tries to create more threads than the system allows in the blocking runtime: it
  fails with an error value, not a crash of the process.
- A deep non-tail recursion: it raises a stack-exhaustion error value instead of crashing.
- A stream handed from its owner scope to a sibling scope (H11): the behavior follows D-17, and
  the cases met in programs 06, 07 and 10 are recorded as evidence for H11.
- A target where CI and local results differ (for example a failure only on x86_64): the CI log
  is enough to reproduce the failing test by name, and the divergence is recorded.
- Sanitizer reports that come from the system libraries rather than from quux code: each
  suppression is listed with its reason; no blanket suppression.

## Requirements *(mandatory)*

### Functional Requirements

**Toolchain and targets**

- **FR-001**: The project MUST build in the four configurations debug, address + undefined
  behavior sanitizers, thread sanitizer and release, with the same configuration names on
  macOS arm64, in the Linux arm64 container and in CI.
- **FR-002**: A smoke test MUST contain a clean program and three deliberate defects (heap
  overflow, data race, leak), and MUST show that each defect is reported by the corresponding
  tool (address sanitizer; thread sanitizer; leak checker and valgrind in the container).
- **FR-003**: CI MUST run the build and every test suite on Linux x86_64 on every push. CI runs
  only on Linux x86_64; macOS arm64 and Linux arm64 are built and tested locally (on the M2 and
  in the container).
- **FR-003a**: Every push MUST first run the build and test suites on macOS arm64 and in the
  Linux arm64 container, and MUST be blocked if any of them fails.
- **FR-004**: Every adjustment needed for the container or CI (capabilities, kernel settings,
  sanitizer options, suppressions) MUST be recorded with its reason.
- **FR-005**: All platform-specific code (threads, atomics, page size, clocks, file and network
  input/output) MUST be confined to a single portability layer that supports the three targets
  (H7, D-28). The page size MUST be queried at run time.

**Core language**

- **FR-006**: The system MUST read quux source text into quux data following section 02 and the
  core forms of `specs/001-language-spec/contracts/core-forms.md`, with no reader shorthands.
- **FR-007**: The system MUST evaluate the core forms with lexical closures, rest parameters
  marked with `:rest`, and vector and map destructuring in `let` and parameter lists; every call
  in tail position MUST run in constant stack space, including mutual recursion (section 03).
- **FR-008**: The system MUST support non-hygienic `define-macro`, `quasi-quote`, `unquote`,
  `unquote-splicing` and `unique-symbol` as section 04 specifies.
- **FR-009**: The system MUST implement the numerics of section 05: 64-bit integers that raise
  `:overflow`, IEEE doubles, and the error kinds `:inexact-division` and `:division-by-zero`.
- **FR-010**: The system MUST provide persistent lists, vectors, maps and sets whose structure is
  canonical (the same contents give the same structure and printed form, whatever the
  insertion order), with a stable value hash per node (sections 01–02, D-02, D-03).
- **FR-011**: The system MUST compute the content hash of top-level definitions after macro
  expansion, with local variables by position and mutually recursive groups hashed together
  (section 13 rules 1–5).
- **FR-012**: The system MUST provide namespaces that map names to definition hashes (section
  14).
- **FR-013**: The system MUST provide a REPL that evaluates expressions, prints values in their
  canonical form, prints errors with their kind and message, and continues after an error.
- **FR-014**: The system MUST run a `.qx` file from the command line, calling its `main`.
- **FR-015**: Every public quux name MUST be listed as approved in `docs/naming.md`, and every C
  identifier MUST be in full words with the `quux_` prefix (constitution VIII); an automated
  check MUST enforce both.

**Concurrency**

- **FR-016**: The runtime MUST implement futures with the three states of section 06, scopes
  that join all children before ending (section 07), cooperative cancellation at the points
  section 07 lists, `shield-from-cancellation`, error propagation with a primary error and
  `:suppressed` errors (D-11 and amendment), and deadlock detection within one process (D-14).
- **FR-017**: The runtime MUST use a blocking model: one operating-system thread per running
  future, with blocking waits. Resource exhaustion (threads, memory) MUST surface as an error
  value.
- **FR-018**: The system MUST implement the combinators and the sequential-looking macro of
  section 08.
- **FR-019**: The system MUST implement agents as section 09 specifies: one update at a time,
  FIFO per sender, `state` never waits, failed updates keep the state, `transact`, and
  `update-once`.
- **FR-020**: The system MUST implement pull streams with `:maximum-ahead` (section 10) and the
  input/output appendix (files, listeners, connections) with the `local-resource` flag.
- **FR-021**: The system MUST implement `parallel-transform`, `parallel-reduce` with the fixed
  reduction tree, and fork/join (section 11), with results identical across runs.

**Simulation and harness**

- **FR-022**: The system MUST simulate several named nodes inside one process, each with its
  own tasks and agents, connected by a simulated network that can delay and drop messages and
  crash nodes as `conditions.qx` describes, and MUST implement `spawn-on`, `node`,
  `serializable?` and the error kinds `:node-unreachable`, `:transport`, `:remote` and
  `:not-serializable` (sections 12–13).
- **FR-023**: Values crossing a simulated node boundary MUST travel in the serialization
  classes of section 13 (copied, by remote reference, or rejected), so that the simulation
  exercises the same rules as a real network.
- **FR-024**: A test harness MUST run every program in `programs/`, reading its
  `conditions.qx` and `expected.qx` as quux data with the reader of FR-006, applying the
  harness conventions of `specs/001-language-spec/contracts/reference-program.md` (`tag`,
  injected delays and failures, `sequential-main`, nodes, drops, `applied-once`), and reporting
  each assertion as held or failed with the observed value.
- **FR-025**: The harness MUST reject unknown keys and unknown assertion forms, and MUST stop a
  program that exceeds its wall-clock bound, reporting it as failed.
- **FR-026**: The examples in `docs/spec/` marked `;=>` and `;!!` MUST run as an automated
  example suite.

**Design constraints carried by this phase**

- **FR-027**: All runtime code MUST follow one minimal convention for allocation and for
  registering roots that a future collector can rely on (H2), and the convention MUST be checked
  automatically (for example by a debug mode that detects unregistered references), even though
  the prototype does not collect memory yet. In Phase 1 the automatic check covers code paths
  run while a single task is running; concurrent paths are verified in Phase 3 with safepoints
  (risk R6).
- **FR-028**: The code MUST be divided into modules whose boundaries state which ones are
  expected to be reused by the VM of Phase 3 (reader, values, collections, printer, macro
  expander, hashing, harness) and which are throwaway (the blocking runtime), recording the
  evidence for H1.
- **FR-029**: This phase MUST close the open points left by feature 001, each recorded as a
  decision (D-xx) or a hypothesis update and reflected in `docs/spec/`:
  - W4, variadic parameters: the keyword `:rest` marks the parameter that receives the
    remaining arguments (`[agent function :rest arguments]`), and section 03 is updated;
  - W5, destructuring: vectors and maps, in `let` and in parameter lists; the map destructuring
    spelling is approved in `docs/naming.md` before use, and sections 03 and 11 are updated;
  - W6, kind predicates such as `vector?` and `map?`, proposed and approved in `docs/naming.md`
    before use;
  - H9, the primary error when several children fail: completion order is kept (D-11),
    deterministic replay is deferred to Phase 7, and the evidence from the programs is recorded;
  - H11, stream ownership across scopes: the prototype follows D-17 and records what programs
    06, 07 and 10 needed.

### Key Entities

- **Value**: any quux datum (section 01), with its value hash; collections are persistent and
  canonical.
- **Definition**: a top-level name bound in a namespace to a content hash.
- **Future / Scope / Agent / Stream**: the runtime entities of `specs/001-language-spec/data-model.md`,
  now with real state and threads.
- **Simulated node**: a named execution place inside the process, with its tasks, agents, crash
  state and a count of completed `spawn-on` tasks.
- **Simulated network**: the message channel between simulated nodes, with latency and drop
  rules taken from `conditions.qx`.
- **Program run**: one execution of a reference program with its conditions, producing the
  observations (result or error, future states, agent states, stream look-ahead, printed lines,
  elapsed time) that the assertions are checked against.
- **Toolchain record**: what was needed to make each configuration work on each target, and why.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All 12 reference programs pass every assertion of their `expected.qx` on macOS
  arm64, Linux arm64 and Linux x86_64 (CI).
- **SC-002**: The full test suite (smoke test, examples, programs) reports zero errors under the
  address, undefined-behavior and thread sanitizers on every target where they run, and zero
  leaks in the Linux container.
- **SC-003**: Each concurrent program (01–04, 06–12) passes 20 consecutive runs under the thread
  sanitizer without a failure (no flaky results).
- **SC-004**: 100% of the `;=>` / `;!!` examples in `docs/spec/` run and produce the stated
  result; any example found wrong is corrected in the specification with a recorded reason.
- **SC-005**: A tail-recursive loop of one million iterations completes with bounded stack use.
- **SC-006**: The full suite of 12 programs completes within the sum of their declared
  wall-clock bounds on the slowest target.
- **SC-007**: W4, W5, W6, H9 and H11 each have a decision or a hypothesis status recorded, and
  H1, H2 and H7 each have a status with the evidence gathered in this phase.
- **SC-008**: Every container and CI adjustment and every sanitizer suppression is recorded with
  its reason; there are no unexplained suppressions.
- **SC-009**: The name check passes: no public quux name outside `docs/naming.md`, no
  abbreviated C identifier outside the exceptions of `docs/naming.md` section H.

## Assumptions

- The specification and programs of feature 001 are the contract. When the prototype shows that
  one of them is wrong or ambiguous, the fix goes into `docs/spec/` or `programs/` with a
  recorded decision; the test is never weakened silently.
- Performance is not a goal: the prototype may be slow, as long as every program finishes within
  its declared generous bounds.
- No garbage collector runs in this phase. Memory may grow during a run, but the root convention
  (FR-027) is followed and checked, and leak checks apply to the runtime's own bookkeeping at
  exit.
- Nodes are simulated in one process. Real networking, containers per node and remote code
  fetching belong to Phase 5.
- Actors are not implemented; the `actors.qx` files remain comparison documents.
- CI runs only on Linux x86_64 (owner decision, 2026-11-25, D-31). The arm64 targets are
  verified locally before every push (FR-003a).
- Library choices (hash functions, test helpers) are researched and recorded in the plan; this
  spec does not choose them.
- Depends on: feature 001 (docs/spec/, programs/, D-01..D-28), constitution v1.2.3.
