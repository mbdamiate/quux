# 04 — Tree and parser

**Area**: 3. recursion and patterns.

## What it evaluates

- A persistent binary search tree made of maps: insert, lookup, size, height, smallest key and
  an in-order walk. Inserting into a tree leaves the old tree unchanged (sections 02, 03).
- Map patterns `{name :key …}` in `let`, and non-tail recursion bounded by the tree's height
  (section 03).
- A recursive-descent parser for arithmetic over token vectors such as `[1 :plus 2 :times 3]`,
  with precedence, left associativity and nested vectors as groups. Each parse function returns
  `[tree remaining-tokens]`, taken apart with a vector pattern; tokens are consumed with
  `[token :rest remaining]` (section 03).
- `conditional`, `case`, a set used for membership, kind predicates `integer?` and `vector?`
  (sections 02, 03).
- A `:user` error raised by the parser and caught per expression (section 12).

## How to run

```bash
build/debug/source/main/quux programs/ergonomics/04-tree-and-parser/main.qx
```

## Expected result

The program prints facts about a tree built from eight keys (one of them repeated), then about
a second tree with one more key, and the size of the first tree again (unchanged). It then
prints each of five expressions with its tree and value; the last one is incomplete and prints
the parser's error. It returns the size of the second tree, `8`, and exits with status 0. The
exact text is in `expected-output.txt`.

**How the expected output was derived** (D-60): every line was written by hand from the
specification before the first run. The first run matched.

## Status

- runnable
- Verifier: every allocation

## Friction observed

| ID | Location | What was awkward | Workaround | Origin | Review |
|---|---|---|---|---|---|
| P04-F1 | `insert`, `has?`, `in-order` | A map pattern names each binding and then its key, even when they are the same word: `{key :key left :left right :right}`. Clojure has a short form for this (`{:keys [key left right]}`). | Write both | writer | unreviewed |
| P04-F2 | `evaluate` | `case` with no matching clause and no default returns `nothing` without an error, so an unknown operator would surface later as an arithmetic `:wrong-type` far from its cause. Writing a default that raises is easy to forget. | A known set of operators | writer | unreviewed |
| P04-F3 | `parse-factor` | The end of the tokens is `nothing`: `(rest (list :plus))` is `nothing`, and a vector pattern on `nothing` binds `nothing`. This works, but the parser cannot tell "no more tokens" from a `nothing` token, so its message reads "found nothing" instead of "found the end of the input". | Accept the message | writer | unreviewed |
| P04-F4 | `in-order` | Joining the left walk, the key and the right walk takes `pour-into` and `add`, which copies the left part at every level (no concatenation, as in P01-F5). | `(pour-into (add left key) right)` | writer | unreviewed |
