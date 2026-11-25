# Quickstart: validating the blocking prototype

How to check each acceptance criterion of [plan.md](./plan.md) §4. Command formats are in
[contracts/command-line.md](./contracts/command-line.md).

## Prerequisites

- macOS arm64: Homebrew `llvm@22`, `cmake`, `ninja` (D-29).
- Docker Desktop for the Linux arm64 container (D-30).
- Pushing to GitHub runs CI on Linux x86_64 only (D-31); no local x86_64 setup exists or is
  needed. The arm64 targets are checked locally.

## 1. Toolchain smoke test (story 1)

```bash
cmake --preset debug && cmake --build --preset debug && ctest --preset debug -L smoke
```

Repeat with `asan-ubsan`, `tsan` and `release`. Expected: the clean program passes in every
preset. The defect tests pass because their tool reports the defect: heap overflow under
`asan-ubsan`, data race under `tsan`.

```bash
tools/container/build-image.sh
```

```bash
tools/container/run-preset.sh asan-ubsan ctest -L smoke
```

```bash
tools/container/run-preset.sh debug tools/ci/run-valgrind.sh tests/smoke
```

Expected: LSan and valgrind report the deliberate leak. Every container flag that was needed is
listed in `docs/toolchain-record.md`.

## 2. Core language (story 2)

```bash
build/debug/source/main/quux
```

Type `(+ 1 2)` and expect `3`. Type `(quotient 1 0)` and expect `!! :division-by-zero: …`, with
the session still open.

```bash
ctest --preset debug -L "unit|examples"
```

Expected: every module test passes, and every `;=>` / `;!!` example in `docs/spec/` passes
(SC-004). Run the same suite with the root verifier:

```bash
build/asan-ubsan/source/main/quux-test --verify-roots examples docs/spec
```

The CTest label `verified` runs each ready section with the root verifier (every allocation,
or amortized for section 03).

## 3. Reference programs (stories 3–5)

```bash
build/asan-ubsan/source/main/quux-test program programs/03-timeout-and-errors
```

Expected: a [report](./contracts/harness-report.md) ending in `result  pass`. Run all 12:

```bash
ctest --preset asan-ubsan -L programs
```

Run them 20 times under TSan (SC-003):

```bash
ctest --preset tsan -L concurrent --repeat until-fail:20
```

## 4. Names and formatting

```bash
ctest --preset debug -L names
```

Expected: no offending name (SC-009).

## 5. CI

Install the hooks once per clone, so every push first runs every preset on macOS arm64 and in
the Linux arm64 container (D-53):

```bash
tools/install-git-hooks.sh
```

The same check can be run by hand, and with the 20-run TSan loop at story checkpoints:

```bash
tools/local-check.sh --repeat 20
```

Push the branch. Expected: the Linux x86_64 job of D-31 is green in all four presets, with no
sanitizer report and no valgrind error (SC-001, SC-002). For a failure that happens only in CI,
re-run that test by name with the workflow's manual trigger.

## 6. Closing check

- `docs/hypotheses.md` has a status with evidence for H1, H2, H7, H9, H10 and H11 (SC-007).
- `docs/decisions.md` lists D-29..D-53.
- `docs/speckit-roadmap.md` has the "Starting reality" for feature 003 filled in.
