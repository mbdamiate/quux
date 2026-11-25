# 03 — Raw text

**Area**: 2. text processing. Variant: raw text; partner: 02.

## What it evaluates

- The task of program 02 (word count, a comma-separated table, aligned output) on the original
  text, given as one string with line breaks (sections 01, 02).
- Splitting the text into its paragraph and its table with a `reduce` over lines and
  `conditional` (sections 02, 03).
- Which string functions an everyday text task needs: each one the core lacks is a stub (D-58).

## How to run

```bash
build/debug/source/main/quux programs/ergonomics/03-text-raw/main.qx "$(cat programs/ergonomics/03-text-raw/input.txt)"
```

Today the program stops at the first stub, printing `!! :user: missing: split text into lines`
and exiting with status 1.

## Expected result

When the missing functions exist, the program prints exactly the same report as program 02 and
returns `["the" 3]` with exit status 0. `expected-output.txt` is that intended output, a copy of
program 02's.

**How the expected output was derived** (D-60): it is program 02's output, which was derived by
hand. The code after the stubs is program 02's code, so it could not be checked by running: the
first stub stops the program.

## Status

- pending — missing: `split-into-lines` (split a string at line breaks)
- pending — missing: `split-on-whitespace` (split a string at runs of spaces)
- pending — missing: `split-on-separator` (split a string at every occurrence of a separator)
- pending — missing: `trim` (remove spaces at both ends of a string)
- Verifier: every allocation

Padding is not a stub: it can be written in quux, as program 02 does (friction P02-F2).

## Friction observed

| ID | Location | What was awkward | Workaround | Origin | Review |
|---|---|---|---|---|---|
| P03-F1 | `split-into-lines`, `split-on-whitespace`, `split-on-separator`, `trim` | The four string functions an everyday text task needs do not exist, and none of them can be written in quux: there is no way to take a character or a part of a string, or to look for a character in it. | Stubs (the program is pending) | writer | unreviewed |
| P03-F2 | the whole program | Programs 02 and 03 share most of their code, but a program cannot load another file, so the code is copied. Namespaces (section 14) organize names inside one file only. | Copy | writer | unreviewed |
| P03-F3 | `rows-of`, `words-of` | `transform` returns a list, so getting a vector back takes `pour-into []` around it. Here a list would also do, but rows are vectors in program 02, and keeping the same kind needs the extra call. | `pour-into []` | writer | unreviewed |
| P03-F4 | `split-at-empty-line` | Splitting a sequence at the first element that satisfies a predicate (Clojure's `split-with`) needs a `reduce` with a three-key state map. | `reduce` with `{:before :after :found}` | writer | unreviewed |
