# 06 — Errors while reading text

**Area**: 4. error handling. Variant: raw text; partner: 05.

## What it evaluates

- The validation of program 05 on records written as quux text, one per line (section 12).
- Catching `:syntax` for a line that cannot be read, next to `:user` and `:wrong-type`, in one
  `try` with `finally` (sections 01, 12; D-54).
- Which functions reading user text needs: each one the core lacks is a stub (D-58).

## How to run

```bash
build/debug/source/main/quux programs/ergonomics/06-errors-reading-text/main.qx "$(cat programs/ergonomics/06-errors-reading-text/input.txt)"
```

Today the program stops at the first stub, printing `!! :user: missing: split text into lines`
and exiting with status 1.

## Expected result

When the missing functions exist, the program prints the report of program 05 for the first six
lines, plus a report for line 7, which ends inside a map and cannot be read. It then raises the
summary error `!! :user: 5 of 7 records are invalid` and exits with status 1. The exact text is in
`expected-output.txt`.

**How the expected output was derived** (D-60): by hand from the specification and program 05's
output. The report for line 7 uses the program's own message, not the reader's, so it does not
depend on the reader's wording. The code after the stubs could not be checked by running.

## Status

- pending — missing: `split-into-lines` (split a string at line breaks)
- pending — missing: `read-from-string` (read one quux value from a string, raising `:syntax`)
- Verifier: every allocation

## Friction observed

| ID | Location | What was awkward | Workaround | Origin | Review |
|---|---|---|---|---|---|
| P06-F1 | `read-from-string` | The reader exists (the runner uses it to read the input argument, and `:syntax` is an approved error kind) but no core function exposes it to programs, so a program cannot read data from text it received. | Stub (the program is pending) | writer | unreviewed |
| P06-F2 | `split-into-lines` | Same missing function as program 03 (P03-F1). | Stub | writer | unreviewed |
| P06-F3 | `main` | Numbering the lines takes `transform` over `(range 0 (count lines))` with `element-at` and `increment`: there is no indexed transform (as in P02-F7). | `range` and `element-at` | writer | unreviewed |
| P06-F4 | the whole program | The validation code is copied from program 05, because a program cannot load another file (as in P03-F2). | Copy | writer | unreviewed |
