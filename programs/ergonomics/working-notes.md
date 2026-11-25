# Working notes (feature 003)

This is a temporary file. Its content moves into `docs/ergonomics-report.md` (task T034), and then
the file is deleted.

## Mistakes

These are real mistakes made while writing the programs that produced an interpreter error
(FR-010a). The columns follow `data-model.md`, entity `Mistake`.

| ID | Code as written | Kind and message | Pointed to the cause | Program | Load time |
|---|---|---|---|---|---|
| M-01 | `(quasi-quote (define-function (unquote name) [row] … (get row …)))` with a global `row` defined before the macro | `:wrong-type`: `malformed binding: a local name cannot be qualified` | partly: it says a binding is qualified, but not which name, which macro or where | 07 | yes |
| M-02 | `(define x (unquote y))` | `:wrong-type`: `unquote outside quasi-quote` | partly: it names the problem but not the line, and the kind is `:wrong-type`, not `:syntax` | 10 (scratch) | yes |
| M-03 | `(let [total] total)` (a binding without a value) | `:wrong-type`: `malformed let: expected (let [name value ...] body ...)` | partly: it shows the expected shape, but not the line or that one value is missing | 10 (scratch) | yes |
| M-04 | `'done` (a reader shorthand) | `:syntax`: `line 1: reader shorthands do not exist; write (quote x), (quasi-quote x), (unquote x) or (wait-for x)` | yes: line and the replacement | 10 (scratch) | yes |
| M-05 | a `define-function` missing its closing parenthesis at the end of the file | `:syntax`: `line 3: unexpected end of text` | partly: the line is where the text ends, not where the unclosed form starts (line 1) | 10 (scratch) | yes |
| M-06 | `(define-function add-one (n) (+ n 1))` (parameters in a list, a Scheme habit) | `:wrong-type`: `malformed function: parameters must be a vector` | partly: clear, but it names neither `add-one` nor the line | 10 (scratch) | yes |
| M-07 | `(let [x 1 y] x)` (odd binding vector) | `:wrong-type`: `malformed let: expected (let [name value ...] body ...)` | partly: same message as M-03, no line | 10 (scratch) | yes |
| M-08 | `(catch division-by-zero problem :caught)` (a symbol where the kind keyword goes) | `:undefined-name`: `division-by-zero is not defined` | partly: the name is right, but nothing says it is the `catch` clause, and the mistake shows only when an error reaches the clause | 10 (scratch) | no |

## Bugs found

Interpreter bugs found by the programs (task T024).

| Program | Symptom | Fix | Regression test |
|---|---|---|---|
| 08 | Unqualified names were searched in the imports in reverse order: `quux_namespace_import` prepended each import, so with `(:import geometry report)` a name defined in both resolved to `report`'s. Section 14 says the imports are searched in order. | `quux_namespace_import` (`source/namespaces/quux_namespaces.c`) now adds a new import after the existing ones and before `core` | `imports are searched in order` in `tests/unit/test_evaluator.c` |
| 07 | `quux-name-check` reported the names that a user-written macro binds inside a vector argument (`(repeat-times [index 0 4] …)`) as unapproved; it only knew symbol arguments. | `quux-name-check` (`source/main/quux_name_check.c`) also treats the symbols directly inside a vector argument of a file's own macro as bound | `names_quux` over program 07 |
| (runner) | Candidate, to classify in the report: `quux FILE.qx INPUT` with a malformed `INPUT` exits with status 1 (`!! :syntax: …`), while feature 002's command-line contract says 2 "on a usage or read error". This behavior predates feature 003 and was kept unchanged by the move to `quux_session_run_file`. Either the contract or the runner should change. | not fixed yet | — |
| 07 | Candidate, to classify in the report: `quasi-quote` qualifies only the template symbols that resolve when the macro is defined (`qualify_symbol` in `source/expander/quux_expander.c`); the others stay as written. Section 04 rule 4 says every free symbol is qualified. A macro that binds a literal symbol works or fails depending on what was defined before it (P07-F3). Either the specification or the expander should change. | not fixed yet | — |

## Timings

Times from `ctest --preset release -L ergonomics` on macOS arm64 (task T023, D-63).

| Program | Data size | Time in seconds (run 1, run 2) |
|---|---|---|
| 01 | 2 warehouses, 4 items | 0.01, 0.01 |
| 02 | 12 words, 3 records | 0.01, 0.01 |
| 04 | 8 keys, 5 expressions | 0.01, 0.01 |
| 05 | 6 records | 0.01, 0.01 |
| 07 | 3 accounts, 4 + 6 loop steps | 0.01, 0.01 |
| 08 | 3 shapes | 0.01, 0.01 |
| 09 | 6 integers, 3 floats, 10 fractions | 0.01, 0.01 |
| 10 | 12 mistakes, one over a list of 20,000 elements | 0.01, 0.01 |

Both runs passed every test (two runs, same results: SC-007). No program comes near 1 second; the
data are small, so these times say little about performance. The only performance finding is the
stack limit: non-tail recursion stops at 10,000 calls (program 10, mistake 9; D-63 counts
`:stack-exhausted` under 10,000 elements as a finding, and this limit is exactly at it).

## Change detection (task T021)

- Changing `height: 3` to `height: 4` in program 04's `expected-output.txt` failed exactly
  `ergonomics_04-tree-and-parser` and its `_verified` test; the other 14 tests passed.
- Changing program 05's `expected-exit-status.txt` from `1` to `0` failed exactly
  `ergonomics_05-errors-prepared-input` and its `_verified` test.
- Both files were restored, and all 16 tests passed again.
