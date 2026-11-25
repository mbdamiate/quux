# 10 — Common mistakes

**Area**: 8. common mistakes.

## What it evaluates

- Twelve run-time mistakes, each caught and described with its error kind and message: a
  missing argument, a misspelled name, a vector pattern on a map, `get` on a string, a keyword
  called as a function (a Clojure habit), arithmetic on a missing key, an index one past the
  end, `put` on a vector with a keyword, deep non-tail recursion, `raise` of a string, integer
  division expecting a fraction, and `reduce` without an initial value (sections 02, 03, 05, 12).
- The quality of the messages: whether each one points to the cause.
- Load-time mistakes, which stop a file before `main` runs, were run as separate scratch files and
  are recorded as mistakes M-02 to M-08 in the report.

## How to run

```bash
build/debug/source/main/quux programs/ergonomics/10-common-mistakes/main.qx
```

## Expected result

The program prints the number of mistakes and one line per mistake with its kind and message. It
returns `12` and exits with status 0. The exact text is in `expected-output.txt`.

**How the expected output was derived** (D-60): the error kinds were written by hand from the
specification. The specification does not define messages, so the messages were written from the
interpreter's behavior seen while writing the earlier programs, and confirmed by running. One
predicted message was wrong: for `reduce` with two arguments the core says
`reduce does not take 2 arguments`, not `reduce expects 3 arguments, got 2` (see P10-F2).

## Status

- runnable
- Verifier: amortized — mistake 9 recurses 10,000 calls deep, and checking every root on every allocation at that depth takes longer than the 30-second test limit

## Friction observed

| ID | Location | What was awkward | Workaround | Origin | Review |
|---|---|---|---|---|---|
| P10-F1 | mistakes 6 and 8 | Messages do not adapt the article to the word: `expects a integer`, `got a nothing`, and in program 05's case `got a error`. | none | writer | kept |
| P10-F2 | mistakes 1 and 12 | Wrong argument counts are reported in two styles: a program's function says `deposit expects 2 arguments, got 1`, while a core function says `reduce does not take 2 arguments` without saying how many it takes. | none | writer | kept |
| P10-F3 | every mistake | No run-time message says where the mistake is: no file, line or calling function (section 12 leaves stack traces open until Phase 7). In a larger program, `+ expects a number, got a nothing` (mistake 6) gives no clue which `+`. | Read the code | writer | kept |
| P10-F4 | mistake 5 | Calling a keyword as a function, a habit from Clojure, gives `a keyword is not a function`, with no hint that `get` is the way to read a key. | none | writer | kept |
| P10-F5 | mistake 9 | Non-tail recursion stops at a depth of 10,000: a simple recursive count over a list of 20,000 elements raises `:stack-exhausted`. The message states the limit, which helps. | Write it with a tail call or `reduce` | writer | kept |
| P10-F6 | load-time mistakes M-02, M-03, M-06, M-07 | A malformed special form (`let` with an odd binding vector, parameters in a list, `unquote` outside `quasi-quote`) raises `:wrong-type` with no line number, while text the reader cannot read raises `:syntax` with a line number. Two kinds and two levels of detail for "this code is malformed". | Read the code | writer | kept |
| P10-F7 | scratch file | `(if test then)` without an else branch is accepted and returns `nothing` when the test fails, although section 03 shows only the three-part form. | none | writer | kept |

## Mistakes made while writing

- M-02 to M-08 (load-time and scratch mistakes, see `docs/ergonomics-report.md`).
