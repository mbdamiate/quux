# Quickstart: Validating the Core Ergonomics Study

These scenarios check that the feature works end to end. Run them from the repository root on
the development machine. The contracts are in [contracts/](./contracts/), and the entities in
[data-model.md](./data-model.md).

## Prerequisites

- The feature 002 toolchain: Homebrew `llvm@22`, CMake and Ninja, and Docker Desktop running for
  the container.
- Git hooks installed (`tools/install-git-hooks.sh`), so that the pre-push check runs.
- A build of the debug preset:

  ```bash
  cmake --preset debug && cmake --build --preset debug
  ```

## 1. Run one program by hand

```bash
build/debug/source/main/quux programs/ergonomics/01-nested-data/main.qx
```

**Expected**: the output equals `programs/ergonomics/01-nested-data/expected-output.txt`, and the
exit status is 0.

For a program with input, pass the contents of `input.txt` as the second argument:

```bash
build/debug/source/main/quux programs/ergonomics/02-text-prepared-input/main.qx "$(cat programs/ergonomics/02-text-prepared-input/input.txt)"
```

## 2. Run the suite

```bash
ctest --preset debug -L ergonomics --output-on-failure
```

**Expected**: eight programs, each with a plain run and a verified run, and all pass.

```bash
ctest --preset debug -N -L pending | grep ergonomics
```

**Expected**: two programs are listed, `03-text-raw` and `06-errors-reading-text`.

## 3. A pending program names its missing pieces

```bash
build/debug/source/main/quux programs/ergonomics/03-text-raw/main.qx "$(cat programs/ergonomics/03-text-raw/input.txt)"
```

**Expected**:

- the exit status is 1;
- the output ends with a `!! :user: missing: …` line;
- that message matches the first stub named in the program README's **Status**.

## 4. The tests detect a change

Edit one character of `programs/ergonomics/09-numerics/expected-output.txt`, then run:

```bash
ctest --preset debug -R ergonomics_09 --output-on-failure
```

**Expected**: the test fails, and the output shows the expected and the actual text. Restore the
file afterwards (`git checkout -- programs/ergonomics/09-numerics/expected-output.txt`).

## 5. Names

```bash
ctest --preset debug -L names
```

**Expected**: the test passes. Changing a call in any program to an unapproved name (for example
`car`) makes `names_quux` fail and name the file.

## 6. All targets and presets

```bash
tools/local-check.sh
```

**Expected**:

- every line reports `pass`;
- `build/local-check.log` lists the `ergonomics_*` tests for each preset on macOS and in the
  container, including the `_valgrind` tests in the container's debug run.

After pushing, the CI log for each preset also lists the `ergonomics_*` tests.

## 7. Performance table

```bash
cmake --preset release && cmake --build --preset release && ctest --preset release -L ergonomics
```

**Expected**: CTest prints one time per test. These times are the ones copied into the report's
"Performance" table, and any over 1 second is flagged (D-63).

## 8. Report coverage

```bash
grep -rhoE 'P[0-9]{2}-F[0-9]+' programs/ergonomics/*/README.md | sort -u > /tmp/notes.txt
grep -oE 'P[0-9]{2}-F[0-9]+' docs/ergonomics-report.md | sort -u > /tmp/reported.txt
comm -23 /tmp/notes.txt /tmp/reported.txt
```

**Expected**: no output. The only IDs allowed in it are friction notes whose review status is
`withdrawn` (SC-004).

Then check the decision list:

- `grep -c '^### K-' docs/ergonomics-report.md` prints at most 15 (SC-006);
- every `K-NN` names a W-xx entry that exists in `docs/spec/open-questions.md` (SC-005).

## 9. Proposed names are not used

For each name in `docs/naming.md` section K with status `proposed`:

```bash
grep -rn -- '<name>' programs docs/spec source
```

**Expected**: no match outside `docs/naming.md`, `docs/ergonomics-report.md` and
`docs/spec/open-questions.md` (FR-012).
