# Contract: Test Driver and `quux-test run`

Decisions D-56 and D-57.

## `quux-test run`

```text
quux-test [--verify-roots | --verify-roots-every-allocation] run FILE.qx [INPUT]
```

- Its behavior is identical to `quux FILE.qx [INPUT]` (contract `command-line.md` of feature
  002): the same standard output and the same exit statuses (0, 1, 2). The only difference is that
  the root verifier runs in the chosen mode.
- Both tools call the same function, `quux_session_run_file(path, input_text)`. The file runner
  moves there from `source/main/quux.c` with no change in behavior.
- A usage error prints the extended usage text of `quux-test` and exits with status 2.

## `tests/ergonomics/run_program_test.cmake`

```text
cmake -DRUNNER="<command and options>" -DPROGRAM_DIRECTORY=<dir> -P run_program_test.cmake
```

1. It runs `RUNNER <dir>/main.qx`. If `<dir>/input.txt` exists, its contents (with the final
   newline stripped) are added as one more argument.
2. It reads `<dir>/expected-output.txt` and `<dir>/expected-exit-status.txt` (if that file is
   absent, the expected status is 0).
3. It passes only if standard output equals the expected output byte for byte **and** the exit
   status equals the expected status.
4. On a failure, it prints the expected output, the actual output and both statuses.

## Registration (`tests/ergonomics/CMakeLists.txt`)

```text
set(quux_ergonomics_ready   01-nested-data 02-text-prepared-input …)
set(quux_ergonomics_pending 03-text-raw 06-errors-reading-text)
```

| Test name | Runner | Labels | Programs |
|---|---|---|---|
| `ergonomics_<dir>` | `quux` | `ergonomics` | ready |
| `ergonomics_<dir>_verified` | `quux-test --verify-roots-every-allocation run` (or `--verify-roots` when the README says `Verifier: amortized`) | `ergonomics;verified` | ready |
| `ergonomics_<dir>_valgrind` | `valgrind --error-exitcode=1 --leak-check=full quux` | `ergonomics;valgrind` | ready, Linux only, when valgrind is found |
| `ergonomics_<dir>` | `quux` | `pending` | pending |

- Every test has the CTest property `TIMEOUT 30`.
- Moving a program from the pending list to the ready list is the only step needed to turn its
  test on.

## CI and local check

No change. Both already run `ctest --label-exclude pending` for every preset, and the valgrind
script runs `--label-regex valgrind`. The new tests appear in their logs automatically.
