# Contract: command-line tools

Decisions: D-46 (harness), D-47 (name check), D-52 (REPL).

## `quux`

| Invocation | Behavior | Exit status |
|---|---|---|
| `quux` | REPL: reads complete forms line by line, prints the value in canonical form, or `!! <kind>: <message>` for an error, and continues. End of input ends the session. | 0 |
| `quux FILE.qx [INPUT]` | Loads `FILE.qx` and calls `(main input)`. `INPUT` is read as quux data; without it, `input` is `nothing`. Prints the returned value. | 0 on return; 1 if `main` raised (the error is printed as above); 2 on a usage or read error |

Only the `.qx` extension is accepted (R9).

## `quux-test`

| Invocation | Behavior | Exit status |
|---|---|---|
| `quux-test program DIRECTORY` | Runs one reference program per the [reference-program contract](../../001-language-spec/contracts/reference-program.md) and prints a [report](./harness-report.md). | 0 if every assertion holds; 1 otherwise; 2 if `conditions.qx` or `expected.qx` is invalid; 3 if the wall-clock bound was exceeded |
| `quux-test examples PATH` | Runs the `;=>` / `;!!` examples of one section file or of every file in a directory. | 0 if all pass; 1 otherwise |
| `quux-test unit [MODULE]` | Runs the C unit tests of one module or of all modules. | 0 if all pass; 1 otherwise |

Options: `--report FILE` (also write the report as quux data), `--seed N` (fix the seed used for
scope identifiers, printed in every report), `--verify-roots` (enable the root verifier, debug
builds only).

## `quux-name-check`

| Invocation | Behavior | Exit status |
|---|---|---|
| `quux-name-check` | Checks every symbol and keyword in `programs/**/*.qx` and in the quux blocks of `docs/spec/*.md` against the `approved` names of `docs/naming.md`, plus names each program defines. | 0 if clean; 1 with one line per offending name: file, line, name |
| `quux-name-check --c source/ tests/` | Splits C identifiers into words and rejects the abbreviations that `docs/naming.md` section H does not allow. | same |

## CTest labels

| Label | Tests |
|---|---|
| `smoke` | the clean program and the three deliberate defects (expected to fail under their tool) |
| `unit` | one test per module |
| `examples` | one test per specification section |
| `programs` | one test per reference program |
| `concurrent` | programs 01–04 and 06–12 (used for the 20-run TSan loop) |
| `names` | the name check |
| `pending` | example sections and programs that do not pass yet; skipped by the pre-push hook and CI, and empty at the end of the feature (D-53) |
