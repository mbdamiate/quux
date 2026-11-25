# Working notes (feature 003)

This is a temporary file. Its content moves into `docs/ergonomics-report.md` (task T034), and then
the file is deleted.

## Mistakes

These are real mistakes made while writing the programs that produced an interpreter error
(FR-010a). The columns follow `data-model.md`, entity `Mistake`.

| ID | Code as written | Kind and message | Pointed to the cause | Program | Load time |
|---|---|---|---|---|---|

## Bugs found

Interpreter bugs found by the programs (task T024).

| Program | Symptom | Fix | Regression test |
|---|---|---|---|
| (runner) | Candidate, to classify in the report: `quux FILE.qx INPUT` with a malformed `INPUT` exits with status 1 (`!! :syntax: …`), while feature 002's command-line contract says 2 "on a usage or read error". This behavior predates feature 003 and was kept unchanged by the move to `quux_session_run_file`. Either the contract or the runner should change. | not fixed yet | — |

## Timings

Times from `ctest --preset release -L ergonomics` on macOS arm64 (task T023, D-63).

| Program | Data size | Time (seconds) |
|---|---|---|
