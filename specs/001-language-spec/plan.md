# Implementation Plan: quux Language Specification by Example (Phase 0)

**Branch**: `001-language-spec` | **Date**: 2026-11-24 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/001-language-spec/spec.md`

## Summary

This plan produces quux's normative specification by example. It has three parts:

- 14 topic sections in `docs/spec/`;
- the 12 reference programs in `programs/`, each with machine-readable expected behavior and
  injected conditions written as quux data;
- actor versions of programs 4, 10 and 12, for comparison.

Design decisions D-01 to D-28 are closed in [research.md](./research.md). Nine of them are
inherited by Phase 3 or Phase 5:

- D-07 (future states)
- D-09 (cancellation)
- D-10 (escaped futures)
- D-11 (several failing children)
- D-13 (error model)
- D-14 (deadlock detection)
- D-19 (function identity)
- D-20 (serialization)
- D-21 (scope identity)

No runtime code is written in this phase.

## Technical Context

| Field | Value |
|---|---|
| **Language/Version** | quux surface syntax (D-01) for programs. Markdown for the specification. No C in this feature. |
| **Primary Dependencies** | None. |
| **Storage** | Files in the repository: `docs/spec/`, `programs/`. |
| **Testing** | Manual review per [quickstart.md](./quickstart.md), plus `grep`-based structural checks. The machine-readable `expected.qx` files become automated tests in feature 002. |
| **Target Platform** | Documents. The programs target the Phase 1 prototype on macOS arm64, Linux arm64 and Linux x86_64 (CI) (D-28). |
| **Project Type** | Language specification (documents and example programs). |
| **Performance Goals** | Not applicable. Delays in programs are injected to make ordering observable (spec Assumptions). |
| **Constraints** | Constitution Principles II, III, IV and VII. No durations (VI). Every normative choice cites a D-xx. |
| **Scale/Scope** | 14 specification sections, 12 programs, 3 actor counterparts, 28 decisions, 5 new hypotheses (H8–H12). |

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Status |
|---|---|---|
| I. C only | No implementation language is chosen or proposed here; the documents are language-neutral about the runtime. | PASS |
| II. Immutability and canonical structures | D-02, D-03: canonical CHAMP, stable hashes, identity only for futures, agents and scopes. | PASS |
| III. Structured concurrency | D-07 to D-18: three states, scopes join, siblings are cancelled, actors stay outside the core (D-24). | PASS |
| IV. Design before performance; distribution decisions early | D-19 to D-23 are closed now; no performance targets are set. | PASS |
| V. Verifiable small milestones | Each task ends in a file that the quickstart checks verify; the stopping point is defined below. | PASS |
| VI. No durations | Only S/M/L sizes and dependency order. | PASS |
| VII. Evidence and traceability | D-01..D-28 and H8..H12 are logged; claims are labeled; no version facts (deferred to the Phase 2 gate). | PASS |
| VIII. Readable names | D-27. All names were approved in `docs/naming.md` (owner review, 2026-11-24) and applied to research.md, data-model.md and contracts/. | PASS |

Post-design re-check: PASS. The design artifacts introduce no new dependency or violation.

## Phase Template (from `docs/planning-prompts/00-base.md`)

### 1. Objective and main question

Can quux's semantics, including concurrency, failure and the parts that affect distribution, be
pinned down precisely enough that the 12 reference programs become executable acceptance tests,
without writing any runtime?

### 2. Deliverables and stopping point

**Deliverables**

- `docs/spec/00-index.md` and `docs/spec/01…14-*.md`: one section per topic, in the
  [section format](./contracts/spec-section.md).
- `programs/01-…` to `programs/12-…`, each holding:
  - `main.qx`
  - `conditions.qx`
  - `expected.qx`
  - `README.md`
  - `actors.qx` (programs 04, 10 and 12 only)

  These follow the [program contract](./contracts/reference-program.md).
- `programs/actors-comparison.md`: the D-24 table.
- `docs/spec/open-questions.md`: the register for FR-026.
- Updated `docs/decisions.md` (D-01..D-28) and `docs/hypotheses.md` (H3, H4 and H8..H12).

**Stopping point**: if the project stops here, it still has a complete design document for a
structured-concurrency Lisp, with executable-ready examples. It can be implemented by anyone,
and its expected behavior is already written as data.

### 3. Task outline (dependency order)

This outline is superseded by [tasks.md](./tasks.md), which is the authoritative task list
(T001–T046). The T0.x IDs below are kept only as a summary.

| ID | Size | Task |
|---|---|---|
| T0.0 | M | Done: the owner reviewed `docs/naming.md` name by name, and the approved names were applied to research.md, data-model.md and contracts/. |
| T0.1 | S | Write `docs/spec/00-index.md` with the 14 topics and the question each one answers, and the section-format conventions. |
| T0.2 | M | Write the sections on values, collections, equality and numerics (D-01..D-04). |
| T0.3 | S | Write the sections on evaluation, TCO, macros and modules (D-05, D-06, D-23). |
| T0.4 | M | Write the sections on futures, scopes and cancellation, and the combinators (D-07..D-12, D-14). |
| T0.5 | M | Write the error-model section, with a table of kinds and which phase introduces each (D-13). |
| T0.6 | M | Write the sections on agents, streams and data parallelism (D-15..D-18). |
| T0.7 | M | Write the section on serialization and identity: the complete equality, hash and serialization table (D-19..D-22). |
| T0.8 | S | Write the program contract files and a template directory (`programs/_template/`). |
| T0.9 | M | Write programs 1, 2, 3 and 8 (all, any, timeout and errors, fork/join). |
| T0.10 | M | Write programs 4 and 9 (bank agent, deadlock). |
| T0.11 | M | Write programs 5, 6 and 7 (pmap, file stream, backpressure). |
| T0.12 | M | Write program 10 (server). |
| T0.13 | L | Write programs 11 and 12 (remote futures with a crash, remote agent with idempotence, cross-machine stream), with node layouts in `conditions.qx`. |
| T0.14 | M | Write the actor versions of programs 4, 10 and 12, and `actors-comparison.md`. |
| T0.15 | S | Write the open-questions register and update `docs/decisions.md` and `docs/hypotheses.md`. |
| T0.16 | S | Do the self-review pass (SC-007): draft the Phase 1 assertions for each program from the documents alone, and fix the gaps found. |

### 4. Acceptance criteria

Each criterion is checked with the commands in [quickstart.md](./quickstart.md).

- 14 section files exist, and each one starts with a `**Question:**` line and contains either
  a `**Decision:** D-xx` or an `**Open — closed in Phase N**` marker (SC-002).
- Each of the 12 program directories has its four required files. `expected.qx` uses only the
  assertion forms of the contract (SC-001).
- Every FR-026 item appears in `open-questions.md` with a D-xx or H-n link (SC-003).
- `docs/decisions.md` lists D-01..D-26 with their "Inherited by" column filled (SC-004).
- The equality, hash and serialization table has no empty cell (SC-005).
- The comparison table has identical rows for its three pairs (SC-006).
- The self-review log shows zero unresolved questions (SC-007).

### 5. Dependencies on earlier phases

None. This is the first feature. It depends only on the constitution (v1.2.2) and the approved glossary `docs/naming.md`.

### 6. Technical risks and mitigation

| Risk | Probability | Impact | Mitigation |
|---|---|---|---|
| The specification grows into a full language reference and stalls the project | Medium `[ESTIMATE]` | High | Write only what the 12 programs need; anything else gets an "open" marker with its closing phase. |
| `expected.qx` assertions turn out to be untestable on a nondeterministic runtime (timing, order of failures) | Medium `[ESTIMATE]` | Medium | Assert orderings and bounds, never exact times; use `one-of` for racing failures (D-11, H9). |
| A decision made on paper (D-14 deadlock detection, D-18 fixed reduction tree) turns out costly in the VM | Medium `[ESTIMATE]` | Medium | H8 and H12 are recorded for Phase 3; the decisions are reversible because only the error kind is normative. |
| Programs 11 and 12 depend on a simulator API that does not exist yet | High `[ESTIMATE]` | Low | D-25 keeps the simulation inside `conditions.qx`; feature 002 adapts its harness to the data. |
| The hashing scheme (D-19) has a flaw with macros or SCCs that only appears in Phase 5 | Low `[ESTIMATE]` | High | Feature 002 computes hashes for every definition and tests that they are stable (renaming, reordering a group). |
| x86_64 is tested only in CI and has no local reproduction (constitution v1.2.0, D-28) | — | — | Not applicable to this documents-only phase; carried into feature 002's risk table. |

### 7. Design decisions closed

D-01..D-28. The full records are in [research.md](./research.md), and the index is in
`docs/decisions.md`.

## Project Structure

### Documentation (this feature)

```text
specs/001-language-spec/
├── plan.md              # This file
├── research.md          # D-01..D-28, H3/H4 outcomes, H8..H12
├── data-model.md        # Entities, state machines, equality/hash/serialization table
├── quickstart.md        # Validation checks
├── contracts/
│   ├── spec-section.md      # Format of each docs/spec section
│   ├── reference-program.md # Program directory + expected.qx/conditions.qx forms
│   └── core-forms.md        # Normative list of special forms and core primitives
├── checklists/requirements.md
└── tasks.md             # Created by /speckit-tasks
```

### Deliverables (repository root)

```text
docs/
├── decisions.md            # D-01..D-28 index
├── naming.md               # name-by-name glossary (Principle VIII, D-27) (updated by this feature)
├── hypotheses.md           # H3/H4 outcomes, H8..H12 added
└── spec/
    ├── 00-index.md
    ├── _section-template.md
    ├── appendix-input-output.md   # I/O used by programs 6 and 10 (not counted among the 14 topics)
    ├── 01-values-and-equality.md
    ├── 02-collections.md
    ├── 03-evaluation-and-tail-calls.md
    ├── 04-macros.md
    ├── 05-numerics.md
    ├── 06-futures.md
    ├── 07-scopes-and-cancellation.md
    ├── 08-combinators.md
    ├── 09-agents.md
    ├── 10-streams.md
    ├── 11-data-parallelism.md
    ├── 12-errors.md
    ├── 13-serialization-and-identity.md
    ├── 14-modules.md
    └── open-questions.md
programs/
├── _template/
├── 01-parallel-search/      … 12-remote-agent-stream/
│   ├── main.qx
│   ├── conditions.qx
│   ├── expected.qx
│   ├── README.md
│   └── actors.qx            # 04, 10, 12 only
└── actors-comparison.md
```

**Structure Decision**: the deliverables are documents and data only. `docs/spec/` holds the
normative text and `programs/` holds the acceptance suite, so feature 002 can point its test
harness at `programs/*/expected.qx` without moving anything.

## Complexity Tracking

No constitution violations: none to justify.
