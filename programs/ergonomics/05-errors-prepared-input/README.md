# 05 — Errors with prepared input

**Area**: 4. error handling. Variant: prepared input; partner: 06.

## What it evaluates

- `:user` errors created with `error`, carrying `:data` with the record and the field
  (section 12).
- `try` with two `catch` clauses chosen by kind, and `finally`, once per record, so one bad record
  does not stop the others (section 12).
- An error raised by the core itself (`:wrong-type` from `<` on a string) caught next to the
  program's own errors (sections 05, 12).
- Reading an error with `error-data` and `get … :message` (section 12).
- An error that no `catch` handles: `finally` runs, the error keeps propagating, and the runner
  prints it and exits with status 1 (section 12, D-56).

## How to run

```bash
build/debug/source/main/quux programs/ergonomics/05-errors-prepared-input/main.qx "$(cat programs/ergonomics/05-errors-prepared-input/input.txt)"
```

## Expected result

The program prints one "checked" line per record (from `finally`), one report line per record,
the counts and the mean age of the accepted records. It then raises a summary error: "report
finished" is printed by `finally`, and the runner prints `!! :user: 4 of 6 records are invalid`.
The exit status is 1 (`expected-exit-status.txt`). The exact text is in `expected-output.txt`.

**How the expected output was derived** (D-60): every line was written by hand from the
specification before the first run, except the message of the `:wrong-type` error on record 5.
The specification gives error kinds but not their messages, so that message
(`< expects a number, got a string`) was taken from the interpreter. The first run matched.

## Status

- runnable
- Verifier: every allocation

## Friction observed

| ID | Location | What was awkward | Workaround | Origin | Review |
|---|---|---|---|---|---|
| P05-F1 | `check-record` (`:wrong-type` clause) | There is no way to wrap an error in a new one that keeps it as `:cause`: `error` takes a kind, a message and data only, and `put` on an error raises `:wrong-type`. Turning a core error into a program error loses the original. | Copy its message into the report | writer | unreviewed |
| P05-F2 | `check-record` (`:wrong-type` clause) | The core's `:wrong-type` error does not say which value or which argument was wrong, or where: `< expects a number, got a string` has no value, no position and nothing in `:data`. The report cannot name the field. | Field `:unknown` | writer | unreviewed |
| P05-F3 | `check-record`, `describe-report` | The kind and the data have accessors (`error-kind`, `error-data`) but the message does not: it is read with `(get problem :message)`. Two ways to read the parts of one value. | `get` | writer | unreviewed |
| P05-F4 | `main` | Splitting a collection in two by a predicate takes `filter` and `reject` over the same collection (Clojure's `group-by` or a partition by predicate). | Two passes | writer | unreviewed |
| P05-F5 | `main` | `transform` returns a list; a vector or a set takes `pour-into` around it (as in P03-F3). | `pour-into` | writer | unreviewed |
