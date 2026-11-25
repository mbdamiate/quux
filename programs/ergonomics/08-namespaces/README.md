# 08 — Namespaces

**Area**: 6. namespaces.

## What it evaluates

- Three `namespace` forms in one file, with `:import` of one and of two namespaces
  (section 14).
- Calls to imported names without qualification (`area`, `show`, `total-area`) and with it
  (`geometry/describe`, `report/describe`) (section 14).
- One name, `describe`, defined in two namespaces with different meanings, and an unqualified
  call to it from a namespace that imports both (section 14).
- `main` defined in the last namespace of the file, where the runner looks for it (D-56).
- `case` on a keyword, `quotient` for a triangle's area (sections 03, 05).

## How to run

```bash
build/debug/source/main/quux programs/ergonomics/08-namespaces/main.qx
```

## Expected result

The program prints the number of shapes, one line per shape, a summary, the largest shape's kind
and the result of the unqualified `describe` (geometry's, because `geometry` is imported first).
It returns the total area, `46`, and exits with status 0. The exact text is in
`expected-output.txt`.

**How the expected output was derived** (D-60): every line was written by hand from the
specification before the first run. The first run did not match: the unqualified `describe`
resolved to `report`'s, the last import, and the program failed with
`!! :wrong-type: + expects a number, got a nothing`. Section 14 says imports are searched in
order, so this was an interpreter bug, fixed with a regression test (see "Bugs found" in
`docs/ergonomics-report.md`). After the fix the output matched.

## Status

- runnable
- Verifier: every allocation

## Friction observed

| ID | Location | What was awkward | Workaround | Origin | Review |
|---|---|---|---|---|---|
| P08-F1 | `main` (unqualified `describe`) | Section 14 is not consistent about a name defined in two imported namespaces: rule 1 makes a name available unqualified "when there is no clash", while the closed open point says the imports are searched in order. Nothing warns about the clash, so the unqualified call silently uses the first import's definition. | Qualify every call to a clashing name | writer | kept |
| P08-F2 | `main` (before the fix) | A wrong `describe` led far away: `area` of a map entry matched no `case` clause, returned `nothing`, and `+` failed with `expects a number, got a nothing`. The message names neither `describe` nor `area`. Same root as P04-F2 (`case` without a default). | Read the code | writer | kept |
| P08-F3 | `report` | Every definition is public: the helper `show` in `report` is imported into `drawing` with the rest. There is no way to keep a helper private to its namespace. | none | writer | kept |
| P08-F4 | the whole program | The three namespaces live in one file: a program cannot load a namespace from another file (as in P03-F2). | One file | writer | kept |
