# Data Model: Core Ergonomics Study

The entities of this feature are documents and test artifacts, not runtime values. Each entity
has a stable identifier, so that it can be traced from a program to the report and on to a
decision (D-61, D-62).

## Ergonomics program

| Field | Values and rules |
|---|---|
| `number` | `01`..`10`, unique; the directory prefix (D-59) |
| `directory` | `programs/ergonomics/NN-short-name/`; the short name is in full words |
| `area` | one of FR-001's areas 1–8; every area has at least one program (SC-001) |
| `evaluates` | the core features it exercises, written in its README |
| `input` | `input.txt` holding quux data as text, or absent (input is then `nothing`) |
| `expected output` | `expected-output.txt`, compared byte for byte; it is derived before the first run (D-60) |
| `expected exit status` | `expected-exit-status.txt` (`0` or `1`), or absent, meaning `0` |
| `status` | `runnable` or `pending` |
| `pending reason` | required when the status is `pending`: the missing pieces, each matching a local stub (D-58) or a bug |
| `variant` | for areas 2 and 4 only: `prepared-input` or `raw-text`, plus the number of its partner program (FR-005a) |
| `verifier mode` | `every allocation` (default) or `amortized` with a reason; it selects the verified test's option (D-56) |
| `friction notes` | zero or more friction notes, listed in its README |

**Status transitions**:

- `pending` → `runnable`: this happens only in a later feature, when the stubbed primitive is
  approved and implemented (FR-014), or in this feature when the blocking bug is fixed.
- `runnable` → `pending` is not allowed. A runnable program that starts failing is a regression,
  and it must be fixed.

## Friction note

| Field | Values and rules |
|---|---|
| `identifier` | `PNN-Fk`, for example `P04-F3`; unique, never reused |
| `location` | the name of a top-level form, or `main.qx` lines |
| `description` | what was awkward, verbose or impossible |
| `workaround` | the code used instead, or "none (stub)" |
| `origin` | `writer`, or `owner` (added at the review checkpoint) |
| `review status` | `unreviewed` → one of `kept`, `disputed: <reason>`, `withdrawn` |

**Rules**:

- Only notes with status `kept` or `disputed` go into the report. A disputed note keeps the
  owner's reason.
- No note is classified while its status is `unreviewed` (FR-009a).

## Report entry

| Field | Values and rules |
|---|---|
| `identifier` | `E-NN`, unique |
| `sources` | one or more `PNN-Fk`, and `M-NN` where it applies; every kept or disputed note appears in exactly one entry (SC-004) |
| `class` | exactly one of: `missing function`, `awkward name`, `missing syntax or pattern`, `error message quality`, `performance surprise` |
| `recommendation` | exactly one of: `add`, `rename`, `document`, `keep as is` |
| `rationale` | text in which each claim carries `[FACT: source]`, `[OPINION]` or `[ESTIMATE]`; it may cite another Lisp's idiom with a link (D-64) |
| `record` | for `add` and `rename`: a W-xx and, after approval, a D-xx; for `document`: the spec section changed; for `keep as is`: none |
| `decision item` | for `add` and `rename`: the `K-NN` item that carries it |

## Mistake

| Field | Values and rules |
|---|---|
| `identifier` | `M-NN` |
| `code` | the code as first written |
| `kind and message` | exactly what the interpreter printed |
| `pointed to the cause` | `yes`, `partly` or `no` |
| `program` | the program being written when it happened, or `10` when it is one of program 10's planned mistakes (planned mistakes are never performance findings) |
| `load time` | `yes` if it happens while the file is loaded, so that program 10 cannot catch it (spec, area 8) |

**Rule**: a mistake marked `partly` or `no` is the source of an `error message quality` report
entry.

## Proposed name (in `docs/naming.md` section K)

| Field | Values and rules |
|---|---|
| `row` | `K<n>`, following the group style of section J |
| `name` | full English words; kebab-case; `?` and `!` suffixes follow rules R2 and R3 |
| `replaces or adds` | the existing name it replaces, or "new" |
| `decision` | the W-xx entry while it is open, then the D-xx entry |
| `status` | `proposed` → `approved` or `rejected` (only the owner changes it) |

**Rule**: a `proposed` name never appears in a program, in the specification or in code (FR-012).

## Decision item

| Field | Values and rules |
|---|---|
| `identifier` | `K-NN`; at most 15 of them (SC-006) |
| `proposal` | the addition or rename, with its signature and one example |
| `friction removed` | the `E-NN` entries it resolves |
| `programs affected` | the program numbers |
| `alternatives` | the other options considered, including "keep as is" |
| `owner answer` | `approve`, `reject: <reason>`, or `defer: <reason>` |

**Rules**:

- When the owner answers `approve`, the item gets a D-xx (D-62).
- When the owner answers `reject`, the report entry becomes `keep as is`.
- When the owner answers `defer`, the W-xx entry stays open.

## Relationships

```text
program 1──* friction note *──1 report entry *──0..1 decision item 1──* proposed name
mistake *──0..1 report entry
report entry (add/rename) 1──1 W-xx ──(approved)──▶ D-xx
program (pending) 1──* stub ──names──▶ proposed name (via its report entry)
```
