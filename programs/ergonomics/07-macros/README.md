# 07 — Macros

**Area**: 5. user-written macros.

## What it evaluates

- `define-macro` with a `:rest` parameter and a vector pattern on an unevaluated argument
  (sections 03, 04).
- `quasi-quote`, `unquote` and `unquote-splicing` written out in full, with no reader shorthands
  (section 04, naming rule R5).
- `unique-symbol` for every name the expansion introduces: a counted loop whose hidden limit does
  not capture the caller's `limit`, and whose bound is evaluated once (section 04).
- A nested `quasi-quote` built inside a `transform`, to generate one `if` per check of a small
  record-checking language (section 04).
- Macros that expand into `let-functions` and `define-function`, and nested macro calls
  (sections 03, 04).

## How to run

```bash
build/debug/source/main/quux programs/ergonomics/07-macros/main.qx
```

## Expected result

The program prints four squares, two lines that show the caller's `limit` is untouched (after one
"limit computed" line), a six-line multiplication table, and the failed checks of three accounts.
It returns the number of accounts that pass every check, `1`, and exits with status 0. The exact
text is in `expected-output.txt`.

**How the expected output was derived** (D-60): every line was written by hand from the
specification before the first run. The first run matched.

## Status

- runnable
- Verifier: every allocation

## Friction observed

| ID | Location | What was awkward | Workaround | Origin | Review |
|---|---|---|---|---|---|
| P07-F1 | both macros | No approved function expands a macro call without evaluating it (Clojure's `macroexpand`), so the only way to see what a macro produces is to run it and read the effects. | Run and compare output | writer | unreviewed |
| P07-F2 | `define-checks` | Without reader shorthands (R5) the templates are long: `define-checks` needs seven `unquote` and one `unquote-splicing`, and a nested `quasi-quote`. The written-out forms read clearly but bury the shape of the generated code. | none (approved rule) | writer | unreviewed |
| P07-F3 | `define-checks` (first draft) | A first draft wrote the binding name `row` directly in the template. It ran, because `row` was not defined anywhere when the macro was defined; with a global `row` defined earlier, the same macro fails at load time with `malformed binding: a local name cannot be qualified`. Whether a template symbol is qualified depends on what is defined when the macro is defined, which section 04 rule 4 does not say ("every free symbol"). The message names neither the macro nor the symbol. | `unique-symbol` for every binding (mistake M-01) | writer | unreviewed |
| P07-F4 | `repeat-times` | The core has no counted loop or "for each" form, so running a body for each number (or element) for its effects needs either this macro or a `reduce` with an ignored accumulator (P01-F4). | This macro | writer | unreviewed |

## Mistakes made while writing

- M-01 (see `docs/ergonomics-report.md`).
