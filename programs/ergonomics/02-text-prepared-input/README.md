# 02 — Text with prepared input

**Area**: 2. text processing. Variant: prepared input; partner: 03.

## What it evaluates

- Counting words with a map, `get` with a default and `put` (section 02).
- Keeping the first-appearance order of distinct words with a set and a vector (section 02).
- A small comma-separated table given as rows of string fields: a vector pattern with `:rest`
  to split the header from the records, and `parse-integer` on the fields (sections 02, 03).
- Aligned output built only with `to-string` and `count` (section 02, appendix).
- A mean of integers, which needs floating-point division (section 05).

## How to run

```bash
build/debug/source/main/quux programs/ergonomics/02-text-prepared-input/main.qx "$(cat programs/ergonomics/02-text-prepared-input/input.txt)"
```

## Expected result

The program prints the number of words and of distinct words, one line per distinct word with
its count (in the order of first appearance), and the most frequent word. It then prints the
number of records, the table with aligned columns, the total score and the mean score. It returns
`["the" 3]` and exits with status 0. The exact text is in `expected-output.txt`.

**How the expected output was derived** (D-60): every line was written by hand from the
specification before the first run, including the mean (253 / 3.0, printed with the shortest
representation that reads back as the same float). The first run matched.

## Status

- runnable
- Verifier: every allocation

## Friction observed

| ID | Location | What was awkward | Workaround | Origin | Review |
|---|---|---|---|---|---|
| P02-F1 | `input.txt`, `main` | The core has no function that splits a string (on whitespace, on a separator, into lines) or joins strings with a separator. The input has to arrive already split; program 03 shows the same task on raw text. | Prepared input | writer | unreviewed |
| P02-F2 | `spaces`, `pad-right`, `pad-left` | No padding or text formatting. Padding is written by hand: a `reduce` over `range` that calls `to-string` once per space. | Three local helpers | writer | unreviewed |
| P02-F3 | `pad-right`, `column-widths` | `count` on a string counts bytes, not characters: `(count "héllo")` is 6. The specification does not say what `count` means for a string. Padding is right only for ASCII text. | The data is ASCII | writer | unreviewed |
| P02-F4 | `distinct-in-order`, `print-word-counts` | There is no sort, and strings cannot be ordered (`(< "a" "b")` raises `:wrong-type`). The word counts cannot be listed alphabetically or by frequency. | First-appearance order, kept with a set and a vector | writer | unreviewed |
| P02-F5 | `print-table` (mean) | No function turns an integer into a float. `/` on two integers raises `:inexact-division` unless the division is exact, so a mean needs one float operand. | `(* 1.0 (count records))` | writer | unreviewed |
| P02-F6 | `print-table` (mean) | No rounding and no number formatting: the mean prints as `84.33333333333333`, and there is no way to print `84.33`. | none | writer | unreviewed |
| P02-F7 | `column-widths` | No way to walk two collections side by side (Clojure's `map` over several collections, or an indexed transform). Updating the widths element by element takes a `reduce` over `(range 0 (count row))` with `element-at`. | `range` and `element-at` | writer | unreviewed |
| P02-F8 | `count-words` | Changing one map value with a function takes `get` with a default and then `put` (Clojure's `update` with a default). Same need as P01-F1, at one level. | `(put counts word (increment (get counts word 0)))` | writer | unreviewed |
| P02-F9 | `print-word-counts`, `print-table` | Printing one line per element uses `reduce` with an ignored accumulator, as in P01-F4. | `reduce` with `nothing` | writer | unreviewed |
