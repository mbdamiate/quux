# Core ergonomics report (feature 003)

This report collects what ten small programs found when they used the sequential core of quux the
way a user would. It classifies every friction point and recommends what to do with it. Its last
section is the short list of decisions the owner takes before Phase 3, when the VM inherits the
core vocabulary.

Decisions: D-55..D-64 (`specs/003-core-ergonomics/research.md`). Contract:
`specs/003-core-ergonomics/contracts/friction-report.md`.

Labels: `[FACT: source]` is checked against a file, a run or a linked page; `[OPINION]` is a
judgment; `[ESTIMATE]` is a guess about size or effect.

## Method

### Programs

| Program | Area | Status | Friction notes |
|---|---|---|---|
| [01-nested-data](../programs/ergonomics/01-nested-data/README.md) | 1. nested data | runnable | 8 |
| [02-text-prepared-input](../programs/ergonomics/02-text-prepared-input/README.md) | 2. text processing | runnable | 9 |
| [03-text-raw](../programs/ergonomics/03-text-raw/README.md) | 2. text processing | pending: string functions (E-01) | 4 |
| [04-tree-and-parser](../programs/ergonomics/04-tree-and-parser/README.md) | 3. recursion and patterns | runnable | 4 |
| [05-errors-prepared-input](../programs/ergonomics/05-errors-prepared-input/README.md) | 4. error handling | runnable | 5 |
| [06-errors-reading-text](../programs/ergonomics/06-errors-reading-text/README.md) | 4. error handling | pending: string functions (E-01), reading from text (E-02) | 4 |
| [07-macros](../programs/ergonomics/07-macros/README.md) | 5. macros | runnable | 4 |
| [08-namespaces](../programs/ergonomics/08-namespaces/README.md) | 6. namespaces | runnable | 4 |
| [09-numerics](../programs/ergonomics/09-numerics/README.md) | 7. numerics | runnable | 6 |
| [10-common-mistakes](../programs/ergonomics/10-common-mistakes/README.md) | 8. common mistakes | runnable | 7 |

Every program was written with approved names only (Principle VIII); `quux-name-check` passes on
all ten. The eight runnable programs run under the CTest label `ergonomics`, plainly and under the
root verifier; the two pending programs call stubs that raise `(error :user "missing: …")` (D-58)
and carry the label `pending`.

### Expected outputs

Every expected output was written by hand from the specification before the program first ran
(D-60). Programs 02, 04, 05, 07 and 09 matched on their first run. The exceptions:

- 01: the order of keys in a printed map follows the hash, so that one line was confirmed by
  running (E-27).
- 08: the first run failed because of an interpreter bug (imports searched in reverse order), which
  was fixed (Bugs found).
- 10: one predicted message was wrong at first (`reduce` with two arguments). After the owner
  review the messages were fixed, and the line now reads as first predicted (E-24).

### Owner review

The owner reviewed the 55 friction notes on 2026-11-25 (task T025): **0 added, 0 disputed, 0
withdrawn**; all 55 are `kept`. The owner also accepted the proposed recommendation for each group
of notes, which this report follows.

### Counts

40 entries from 55 notes and 7 mistakes (M-01..M-08 except M-04, whose message pointed to the
cause).

| Class | Entries |
|---|---|
| missing function | 19 |
| missing syntax or pattern | 8 |
| error message quality | 10 |
| awkward name | 2 |
| performance surprise | 1 |

| Recommendation | Entries |
|---|---|
| add | 21 |
| rename | 0 |
| document | 7 |
| keep as is | 12 |

Of the 12 "keep as is" entries, four were resolved by a bug fix during the study (E-22..E-24, and
the runner bug in Bugs found), and two are deferred to a later phase (E-29, E-30).

## Friction entries

The 21 `add` entries come first, roughly by how many programs they affect; then the entries
resolved by fixes; then `document` and `keep as is`.

### E-01 — No way to split, trim or search text

- **Sources**: P02-F1, P03-F1, P06-F2
- **Class**: missing function
- **Observed**: the input of program 02 has to arrive already split into words and rows; programs
  03 and 06, which receive one string, cannot run.

  ```quux
  ;; 03: stubs, because none of these can be written in quux
  (define-function split-into-lines [text]
    (raise (error :user "missing: split-into-lines" {:needed "a core function"})))
  ```

- **Recommendation**: add
- **Rationale**: no core function takes a part of a string, a character of it, or the position of a
  substring, so the missing functions cannot be written by a user either [FACT: section 02, naming
  sections A–J]. Text input is the most common everyday task the study met, and it blocks two of
  ten programs [FACT: programs 03, 06]. Clojure keeps these in a separate namespace
  ([`clojure.string/split`](https://clojure.github.io/clojure/clojure.string-api.html#clojure.string/split),
  [`split-lines`](https://clojure.github.io/clojure/clojure.string-api.html#clojure.string/split-lines),
  [`trim`](https://clojure.github.io/clojure/clojure.string-api.html#clojure.string/trim),
  [`index-of`](https://clojure.github.io/clojure/clojure.string-api.html#clojure.string/index-of))
  [FACT: links]. A small set (split on lines, on whitespace, on a separator; trim; a part of a
  string; the position of a substring) covers both pending programs [OPINION]. Size S–M in the
  interpreter [ESTIMATE].
- **Record**: W12, D-65
- **Decision item**: K-01

### E-02 — No way to read data from text

- **Sources**: P06-F1
- **Class**: missing function
- **Observed**: program 06 cannot turn a line of text into a map; it calls a stub
  `read-from-string`.
- **Recommendation**: add
- **Rationale**: the reader already exists, the runner uses it for `INPUT`, and `:syntax` is an
  approved error kind [FACT: `source/harness/quux_session.c`, section 12]; only a core function is
  missing. Exposing it is size S [ESTIMATE].
- **Record**: W13, D-66
- **Decision item**: K-02

### E-03 — Changing a value deep inside nested data takes a recursive helper

- **Sources**: P01-F1, P02-F8
- **Class**: missing function
- **Observed**:

  ```quux
  (define-function change-at-path [collection path change]
    (if (empty? path)
      (change collection)
      (let [[key :rest remaining] path]
        (put collection key (change-at-path (get collection key) remaining change)))))
  ```

  At one level, the same need reads `(put counts word (increment (get counts word 0)))`.
- **Recommendation**: add
- **Rationale**: every program with nested data writes this helper [FACT: programs 01, 02].
  Clojure has
  [`assoc-in`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/assoc-in),
  [`update-in`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/update-in)
  and [`update`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/update)
  [FACT: links]; the name `update` is taken by agents in quux [FACT: section 09], so the proposal
  needs a new full-word name (D-64).
- **Record**: W14, D-67
- **Decision item**: K-03

### E-04 — Reading a value through a path of keys

- **Sources**: P01-F2
- **Class**: missing function
- **Observed**: `(reduce (function [current key] (get current key)) collection path)`
- **Recommendation**: add
- **Rationale**: the workaround is one line, but it appears wherever E-03 does, and the pair reads
  better as two functions with related names [OPINION]. Clojure:
  [`get-in`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/get-in)
  [FACT: link].
- **Record**: W14, D-67
- **Decision item**: K-03

### E-05 — Finding the first element that satisfies a predicate

- **Sources**: P01-F3
- **Class**: missing function
- **Observed**:

  ```quux
  (reduce (function [found index]
            (if (and (nothing? found)
                     (equal? (get (element-at items index) :product) product))
              index
              found))
          nothing
          (range 0 (count items)))
  ```

- **Recommendation**: add
- **Rationale**: eight lines for a lookup, and the `reduce` does not stop at the first match
  [FACT: program 01]. Clojure uses
  [`some`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/some) [FACT: link].
  A function for the element and one for its position cover both uses [OPINION].
- **Record**: W15, D-68
- **Decision item**: K-04

### E-06 — No form runs an effect for each element

- **Sources**: P01-F4, P02-F9, P07-F4
- **Class**: missing syntax or pattern
- **Observed**:

  ```quux
  (reduce (function [ignored warehouse] (print-line …)) nothing (get inventory :warehouses))
  ```

  Program 07 writes a macro, `repeat-times`, for a counted loop.
- **Recommendation**: add
- **Rationale**: three programs [FACT: 01, 02, 07]. A `reduce` with an ignored accumulator hides
  the intent [OPINION]. Clojure has
  [`doseq`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/doseq),
  [`run!`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/run!) and
  [`dotimes`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/dotimes)
  [FACT: links]. A function that takes a function and a collection needs no new syntax, and
  `range` covers the counted case [OPINION].
- **Record**: W16, D-69
- **Decision item**: K-05

### E-07 — No concatenation

- **Sources**: P01-F5, P04-F4
- **Class**: missing function
- **Observed**: `(reduce (function [items more-items] (pour-into items more-items)) [] groups)`;
  the in-order walk of program 04 copies the left part at every level:
  `(pour-into (add left key) right)`.
- **Recommendation**: add
- **Rationale**: two programs [FACT: 01, 04]. Clojure:
  [`concat`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/concat),
  [`mapcat`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/mapcat)
  [FACT: links].
- **Record**: W17, D-70
- **Decision item**: K-06

### E-08 — No text formatting: join, padding, decimals

- **Sources**: P01-F8, P02-F2, P02-F6
- **Class**: missing function
- **Observed**:

  ```quux
  (define-function spaces [width]
    (reduce (function [text ignored] (to-string text " ")) "" (range 0 width)))
  ```

  A mean prints as `84.33333333333333`; a vector prints as `["south/washer"]` because there is no
  join.
- **Recommendation**: add
- **Rationale**: every program that prints a table or a list writes these helpers [FACT: 01, 02].
  Clojure: [`clojure.string/join`](https://clojure.github.io/clojure/clojure.string-api.html#clojure.string/join)
  [FACT: link]. Join with a separator, padding to a width, and a number with a fixed count of
  decimals are enough for the programs here; a general format string can wait [OPINION].
- **Record**: W18, D-71
- **Decision item**: K-07

### E-09 — No sort, and strings cannot be ordered

- **Sources**: P02-F4, P09-F1
- **Class**: missing function
- **Observed**: `(< "a" "b")` raises `:wrong-type`; program 09 writes an insertion sort from two
  `filter` calls per element to find a median.
- **Recommendation**: add
- **Rationale**: two programs [FACT: 02, 09]. Clojure:
  [`sort`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/sort),
  [`sort-by`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/sort-by),
  [`compare`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/compare)
  [FACT: links]. Ordering strings by bytes is enough for ASCII data; ordering by characters ties to
  E-28 [OPINION].
- **Record**: W19, D-72
- **Decision item**: K-08

### E-10 — No conversion between integers and floats, no rounding

- **Sources**: P02-F5, P09-F4
- **Class**: missing function
- **Observed**: an integer becomes a float only through `(* 1.0 n)`; a float cannot be rounded,
  truncated or floored to an integer.
- **Recommendation**: add
- **Rationale**: two programs [FACT: 02, 09]. Clojure:
  [`double`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/double),
  [`long`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/long)
  [FACT: links].
- **Record**: W20, D-73
- **Decision item**: K-09

### E-11 — No square root or power

- **Sources**: P09-F2
- **Class**: missing function
- **Observed**: program 09 stops at the variance; it cannot compute a standard deviation.
- **Recommendation**: add
- **Rationale**: one program [FACT: 09], but the functions are small and belong with E-10 in one
  numeric decision [OPINION].
- **Record**: W20, D-73
- **Decision item**: K-09

### E-12 — A mean of integers needs an error for control flow

- **Sources**: P09-F3
- **Class**: missing function
- **Observed**:

  ```quux
  (define-function divide [a b]
    (try
      (/ a b)
      (catch :inexact-division problem (/ a (* 1.0 b)))))
  ```

  The result's kind then depends on the data: `18` but `15.5`.
- **Recommendation**: add
- **Rationale**: `/` on integers raises `:inexact-division` unless the quotient is exact
  [FACT: section 05]. Keeping `/` exact is sound for Phase 3 [OPINION]; what is missing is a
  division that always returns a float, so that the program says which it wants [OPINION].
- **Record**: W20, D-73
- **Decision item**: K-09

### E-13 — No transform with an index, or over two collections

- **Sources**: P02-F7, P06-F3
- **Class**: missing function
- **Observed**: `(transform (function [index] … (element-at lines index) …) (range 0 (count lines)))`
- **Recommendation**: add
- **Rationale**: two programs [FACT: 02, 06]. Clojure:
  [`map-indexed`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/map-indexed)
  [FACT: link].
- **Record**: W21, D-74
- **Decision item**: K-10

### E-14 — The message of an error has no accessor

- **Sources**: P05-F3
- **Class**: missing function
- **Observed**: `(error-kind problem)` and `(error-data problem)`, but `(get problem :message)`.
- **Recommendation**: add
- **Rationale**: two ways to read the parts of one value [FACT: section 12]. Clojure has
  [`ex-message`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/ex-message)
  next to `ex-data` [FACT: link].
- **Record**: W22, D-75
- **Decision item**: K-11

### E-15 — An error cannot be wrapped in another one

- **Sources**: P05-F1
- **Class**: missing function
- **Observed**: `error` takes a kind, a message and data only, and `put` on an error raises
  `:wrong-type`, so turning a core error into a program error loses the original.
- **Recommendation**: add
- **Rationale**: section 12 already has a `:cause` chain for errors that cross nodes, but a program
  cannot build one [FACT: section 12 rule 4]. Clojure:
  [`ex-info`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/ex-info) takes
  an optional cause, read with
  [`ex-cause`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/ex-cause)
  [FACT: links].
- **Record**: W22, D-75
- **Decision item**: K-11

### E-16 — Core errors do not say which value or where

- **Sources**: P05-F2, P10-F3
- **Class**: error message quality
- **Observed**: `< expects a number, got a string` has no value, no argument position and nothing
  in `:data`; in a larger program, `+ expects a number, got nothing` gives no clue which `+`.
- **Recommendation**: add
- **Rationale**: program 05 cannot name the field that failed [FACT: 05]. Putting the value and the
  argument position in `:data` is a library addition the VM must keep [OPINION]. The location
  (file, line, calling function) stays open until Phase 7, as section 12 says [FACT: section 12].
- **Record**: W22, D-75
- **Decision item**: K-11

### E-17 — No way to see a macro's expansion

- **Sources**: P07-F1
- **Class**: missing function
- **Observed**: the only way to check `define-checks` was to run it and read its effects.
- **Recommendation**: add
- **Rationale**: Clojure:
  [`macroexpand`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/macroexpand),
  [`macroexpand-1`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/macroexpand-1)
  [FACT: links]. The expander already does the work [FACT: `source/expander/`]; size S
  [ESTIMATE].
- **Record**: W23, D-76
- **Decision item**: K-12

### E-18 — `case` with no matching clause returns nothing silently

- **Sources**: P04-F2, P08-F2
- **Class**: error message quality
- **Observed**: an unknown operator in program 04, and a wrong `describe` in program 08, surface
  far away as `+ expects a number, got nothing`.
- **Recommendation**: add
- **Rationale**: two programs [FACT: 04, 08]. A `case` with no default that matches nothing should
  raise an error that names the value [OPINION]. Clojure's `case` throws in that situation
  [FACT: [`case`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/case)].
  This changes the meaning of existing code, so the VM inherits it [OPINION].
- **Record**: W24, D-77
- **Decision item**: K-13

### E-19 — Map patterns repeat every name

- **Sources**: P04-F1
- **Class**: missing syntax or pattern
- **Observed**: `(let [{key :key left :left right :right} tree] …)`
- **Recommendation**: add
- **Rationale**: Clojure has a short form for this (`{:keys [key left right]}`, described with
  [`let`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/let))
  [FACT: link]. A short form is a pattern-syntax decision that Phase 3 inherits [OPINION].
- **Record**: W25, D-78
- **Decision item**: K-14

### E-20 — A malformed special form is `:wrong-type` without a line

- **Sources**: P10-F6, M-02, M-03, M-06, M-07
- **Class**: error message quality
- **Observed**: `(let [x 1 y] x)` gives
  `:wrong-type: malformed let: expected (let [name value ...] body ...)`, with no line; text the
  reader cannot read gives `:syntax` with a line.
- **Recommendation**: add
- **Rationale**: four of the eight real mistakes are of this kind, and all four were rated
  "partly" [FACT: Mistakes log]. `:syntax` is defined for the reader only [FACT: section 12]; using
  it, with a line, for malformed special forms gives one kind and one level of detail for "this
  code is malformed" [OPINION].
- **Record**: W26, D-79
- **Decision item**: K-15

### E-21 — A name defined in two imports is silently taken from the first

- **Sources**: P08-F1
- **Class**: error message quality
- **Observed**: `describe` is defined in `geometry` and in `report`; the unqualified call uses
  `geometry`'s with no warning.
- **Recommendation**: add
- **Rationale**: section 14 rule 1 makes a name available unqualified "when there is no clash",
  while its closed open point says the imports are searched in order [FACT: section 14]. Raising an
  error at load time for a clashing unqualified name matches rule 1 and makes the clash visible
  [OPINION]. Either way, the text of section 14 must be made consistent.
- **Record**: W26, D-79
- **Decision item**: K-15

### E-22 — `quasi-quote` qualified only symbols that already resolved

- **Sources**: P07-F3, M-01
- **Class**: error message quality
- **Observed**: a macro that bound `row` directly in its template worked while no global `row`
  existed and failed after one was defined, with
  `malformed binding: a local name cannot be qualified`.
- **Recommendation**: keep as is (resolved by a fix)
- **Rationale**: section 04 rule 4 already says every free symbol is qualified, and D-06 chose a
  qualifying syntax-quote [FACT: section 04, docs/decisions.md]. The expander now follows it, and
  the message names the symbol: `malformed binding: user/row is qualified; a macro binds a local
  name made by unique-symbol (section 04 rule 5)` [FACT: commit `cf9f0c6`].
- **Record**: Bugs found

### E-23 — Messages did not adapt the article

- **Sources**: P10-F1
- **Class**: error message quality
- **Observed**: `expects a integer`, `got a nothing`, `got a error`.
- **Recommendation**: keep as is (resolved by a fix)
- **Rationale**: now `an integer`, `nothing`, `an error` [FACT: commit `bcc213b`].
- **Record**: Bugs found

### E-24 — Two styles of wrong-argument-count message

- **Sources**: P10-F2
- **Class**: error message quality
- **Observed**: `deposit expects 2 arguments, got 1`, but `reduce does not take 2 arguments`.
- **Recommendation**: keep as is (resolved by a fix)
- **Rationale**: core functions now say `reduce expects 3 arguments, got 2`,
  `get expects 2 or 3 arguments, got 1` and `- expects at least 1 argument, got 0`
  [FACT: commit `bcc213b`].
- **Record**: Bugs found

### E-25 — Calling a keyword gives no hint

- **Sources**: P10-F4
- **Class**: error message quality
- **Observed**: `(:a {:a 1})` gives `a keyword is not a function`.
- **Recommendation**: keep as is
- **Rationale**: the message is accurate; a hint about `get` would help people coming from Clojure
  but is not worth a rule [OPINION].
- **Record**: none

### E-26 — Non-tail recursion stops at 10,000 calls

- **Sources**: P10-F5
- **Class**: performance surprise
- **Observed**: a recursive count over a list of 20,000 elements raises
  `:stack-exhausted: calls not in tail position are nested deeper than 10000`.
- **Recommendation**: document
- **Rationale**: section 03 leaves the limit to the implementation [FACT: section 12,
  `:stack-exhausted`]; the message states it, which helps [FACT: program 10]. Writing the number
  down for Phase 1 lets a user plan for it, and the VM may raise it [OPINION].
- **Record**: section 03

### E-27 — The printed form of values is not specified

- **Sources**: P01-F7
- **Class**: missing syntax or pattern
- **Observed**: `{:product "washer" :quantity 50 :price 1}` prints as
  `{:price 1 :product "washer" :quantity 50}`.
- **Recommendation**: document
- **Rationale**: sections 01 and 02 give `;=>` values but no printed forms [FACT: sections 01,
  02]. Stating that a map or set prints in an order the program does not control is enough; a
  sorted printer would cost every print [OPINION].
- **Record**: sections 01 and 02

### E-28 — `count` on a string counts bytes

- **Sources**: P02-F3
- **Class**: awkward name
- **Observed**: `(count "héllo")` is 6.
- **Recommendation**: document
- **Rationale**: the specification does not say what `count` means for a string [FACT: section
  02]. Documenting bytes now is cheap; whether a later version counts characters is a decision of
  its own, recorded as an open question [OPINION].
- **Record**: section 02, W27

### E-29 — A program cannot load another file

- **Sources**: P03-F2, P06-F4, P08-F4
- **Class**: missing function
- **Observed**: programs 03 and 06 copy code from 02 and 05; program 08 keeps three namespaces in
  one file.
- **Recommendation**: keep as is (deferred)
- **Rationale**: how namespaces map to files and packages is already an open point of section 14,
  closed in Phase 7 [FACT: section 14]. Three programs show the need, which this entry adds as
  evidence to that open point [OPINION].
- **Record**: section 14 open point (packages)

### E-30 — No private definitions

- **Sources**: P08-F3
- **Class**: missing syntax or pattern
- **Observed**: the helper `show` in `report` is imported into `drawing` with the rest.
- **Recommendation**: keep as is (deferred)
- **Rationale**: one program [FACT: 08]; Clojure marks private definitions with
  [`defn-`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/defn-)
  [FACT: link]. Visibility belongs with the module decisions of section 14 [OPINION].
- **Record**: W28

### E-31 — The end of a list is `nothing`

- **Sources**: P04-F3
- **Class**: missing syntax or pattern
- **Observed**: `(rest (list :plus))` is `nothing`, so the parser reports "found nothing" at the end
  of the input.
- **Recommendation**: document
- **Rationale**: the behavior is consistent [FACT: program 04] but not written down [FACT: section
  02].
- **Record**: section 02

### E-32 — Catching every error needs a predicate

- **Sources**: P09-F6
- **Class**: missing syntax or pattern
- **Observed**: `(catch (function [problem] true) problem …)`
- **Recommendation**: document
- **Rationale**: the predicate form already covers it [FACT: section 12]; an example in section 12
  is enough [OPINION].
- **Record**: section 12

### E-33 — `if` without an else branch is accepted

- **Sources**: P10-F7
- **Class**: missing syntax or pattern
- **Observed**: `(if false 1)` returns `nothing`.
- **Recommendation**: document
- **Rationale**: section 03 shows only the three-part form [FACT: section 03]. The two-part form is
  useful with effects and costs nothing [OPINION].
- **Record**: section 03

### E-34 — A total takes `transform` and `(reduce + 0)`

- **Sources**: P01-F6
- **Class**: missing function
- **Observed**: `(thread-last items (transform price-of) (reduce + 0))`
- **Recommendation**: keep as is
- **Rationale**: the pipeline reads well [OPINION].
- **Record**: none

### E-35 — `transform` always returns a list

- **Sources**: P03-F3, P05-F5
- **Class**: awkward name
- **Observed**: `(pour-into [] (transform f rows))` to keep a vector.
- **Recommendation**: document
- **Rationale**: one result kind is simple and consistent [OPINION]; section 02 should say it
  [FACT: section 02 does not].
- **Record**: section 02

### E-36 — Splitting a collection by a predicate

- **Sources**: P03-F4, P05-F4
- **Class**: missing function
- **Observed**: `filter` and `reject` over the same records; a `reduce` with a three-key state to
  split at an empty line.
- **Recommendation**: keep as is
- **Rationale**: two passes are clear, and the need is rare in the programs [OPINION]. Clojure has
  [`group-by`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/group-by) and
  [`split-with`](https://clojure.github.io/clojure/clojure.core-api.html#clojure.core/split-with)
  [FACT: links].
- **Record**: none

### E-37 — Templates are long without reader shorthands

- **Sources**: P07-F2
- **Class**: missing syntax or pattern
- **Observed**: `define-checks` needs seven `unquote`, one `unquote-splicing` and a nested
  `quasi-quote`.
- **Recommendation**: keep as is
- **Rationale**: rule R5 (no reader shorthands) was approved by the owner [FACT: docs/naming.md];
  this entry is evidence if it is ever reviewed.
- **Record**: none

### E-38 — No greatest common divisor

- **Sources**: P09-F5
- **Class**: missing function
- **Observed**: program 09 writes `greatest-common-divisor` for its fractions.
- **Recommendation**: keep as is
- **Rationale**: rationals are an open point of section 05, closed after Phase 3 [FACT: section 05];
  the helper belongs with that decision [OPINION].
- **Record**: none

### E-39 — An unclosed form is reported where the text ends

- **Sources**: M-05
- **Class**: error message quality
- **Observed**: a `define-function` without its closing parenthesis on line 1 gives
  `:syntax: line 3: unexpected end of text`.
- **Recommendation**: keep as is
- **Rationale**: the language rule is fine; naming the line where the unclosed form starts is an
  implementation improvement the VM's reader can make [OPINION].
- **Record**: none

### E-40 — A symbol in a `catch` clause is checked only when an error arrives

- **Sources**: M-08
- **Class**: error message quality
- **Observed**: `(catch division-by-zero problem :caught)` gives
  `:undefined-name: division-by-zero is not defined`, only when an error reaches the clause.
- **Recommendation**: keep as is
- **Rationale**: the predicate form makes any expression valid there [FACT: section 12], so the
  check cannot happen earlier without a rule against bare symbols [OPINION].
- **Record**: none

## Mistakes log

Real mistakes made while writing the programs that produced an interpreter error (FR-010a).

| ID | Code as written | Kind and message | Pointed to the cause | Program | Load time |
|---|---|---|---|---|---|
| M-01 | `(quasi-quote (define-function (unquote name) [row] … (get row …)))` with a global `row` defined before the macro | `:wrong-type`: `malformed binding: a local name cannot be qualified` (since `cf9f0c6`: `malformed binding: user/row is qualified; …`) | partly: it said a binding was qualified, but not which name, which macro or where | 07 | yes |
| M-02 | `(define x (unquote y))` | `:wrong-type`: `unquote outside quasi-quote` | partly: it names the problem but not the line, and the kind is `:wrong-type`, not `:syntax` | 10 (scratch) | yes |
| M-03 | `(let [total] total)` (a binding without a value) | `:wrong-type`: `malformed let: expected (let [name value ...] body ...)` | partly: it shows the expected shape, but not the line or that one value is missing | 10 (scratch) | yes |
| M-04 | `'done` (a reader shorthand) | `:syntax`: `line 1: reader shorthands do not exist; write (quote x), (quasi-quote x), (unquote x) or (wait-for x)` | yes: line and the replacement | 10 (scratch) | yes |
| M-05 | a `define-function` missing its closing parenthesis at the end of the file | `:syntax`: `line 3: unexpected end of text` | partly: the line is where the text ends, not where the unclosed form starts (line 1) | 10 (scratch) | yes |
| M-06 | `(define-function add-one (n) (+ n 1))` (parameters in a list, a Scheme habit) | `:wrong-type`: `malformed function: parameters must be a vector` | partly: clear, but it names neither `add-one` nor the line | 10 (scratch) | yes |
| M-07 | `(let [x 1 y] x)` (odd binding vector) | `:wrong-type`: `malformed let: expected (let [name value ...] body ...)` | partly: same message as M-03, no line | 10 (scratch) | yes |
| M-08 | `(catch division-by-zero problem :caught)` (a symbol where the kind keyword goes) | `:undefined-name`: `division-by-zero is not defined` | partly: the name is right, but nothing says it is the `catch` clause, and the mistake shows only when an error reaches the clause | 10 (scratch) | no |

## Bugs found

Interpreter bugs found by the programs (task T024). Each was fixed with a regression test written
first.

| Program | Symptom | Fix commit | Regression test |
|---|---|---|---|
| 08 | Unqualified names were searched in the imports in reverse order: with `(:import geometry report)`, a name defined in both resolved to `report`'s. Section 14 says the imports are searched in order. | `a622ef6` (`quux_namespace_import` adds a new import after the existing ones and before `core`) | `imports are searched in order`, `tests/unit/test_evaluator.c` |
| 07 | `quux-name-check` reported the names that a file's own macro binds inside a vector argument (`(repeat-times [index 0 4] …)`) as unapproved. | `a622ef6` (`source/main/quux_name_check.c`) | `names_quux` over program 07 |
| 10 | Messages used "a" before every kind (`a integer`, `a nothing`, `a error`) (E-23), and core functions reported a wrong argument count in another style than program functions (E-24). | `bcc213b` (`quux_kind_phrase`; one arity format) | `error messages read well`, `tests/unit/test_evaluator.c`; program 10's expected output |
| 07 | `quasi-quote` qualified only the template symbols that resolved when the template was analyzed, while section 04 rule 4 says every free symbol (E-22). | `cf9f0c6` (`qualify_symbol` in `source/expander/quux_expander.c`) | `quasi-quote, patterns, :rest and macros`, `tests/unit/test_expander.c` |
| 06 | `quux FILE.qx INPUT` with a malformed `INPUT` exited with 1, while feature 002's command-line contract says 2 on a read error. | `5f18cbf` (`run_file` in `source/harness/quux_session.c`) | `ergonomics_malformed_input_status` |

No bug was left unfixed, so no task was added to feature 002.

## Performance

Times from `ctest --preset release -L ergonomics` on macOS arm64 (task T023, D-63), two runs.

| Program | Data size | Time in seconds (run 1, run 2) | Flag |
|---|---|---|---|
| 01 | 2 warehouses, 4 items | 0.01, 0.01 | — |
| 02 | 12 words, 3 records | 0.01, 0.01 | — |
| 04 | 8 keys, 5 expressions | 0.01, 0.01 | — |
| 05 | 6 records | 0.01, 0.01 | — |
| 07 | 3 accounts, 4 + 6 loop steps | 0.01, 0.01 | — |
| 08 | 3 shapes | 0.01, 0.01 | — |
| 09 | 6 integers, 3 floats, 10 fractions | 0.01, 0.01 | — |
| 10 | 12 mistakes, one over a list of 20,000 elements | 0.01, 0.01 | — |

No program comes near 1 second; the data are small, so these times say little about performance
[OPINION]. `:stack-exhausted` appeared only at 20,000 elements (program 10, mistake 9, a planned
mistake), not under 10,000, so D-63 flags nothing; the limit itself is entry E-26.

## Owner decision list

Fifteen items, one per group of `add` recommendations. The owner approved all fifteen on
2026-11-25 (D-65..D-79); the names are `approved` in `docs/naming.md` section K, and the open
questions W12–W26 are resolved. A later feature implements them. The names are full words (Principle VIII). Where a Clojure name is
also plain English (`sort`, `sort-by`), it is kept (D-64). Names that a program already uses for a
local helper (`trim`, `change-at-path`, `pad-right`, `read-from-string`) are avoided, as are
`concatenate` (a helper in reference program 08) and words that appear in the specification's
prose (`compare`, `power`).

An answer is `approve`, `reject: <reason>` or `defer: <reason>`. An item can be answered in
parts, for example "approve, without `position-in-text`".

### K-01 — String functions

- **Proposal**:
  - `(split-lines text)`: a vector of the lines, without line ends.
  - `(split-words text)`: the parts separated by whitespace, without empty parts.
  - `(split-text text separator)`: the parts between occurrences of the string `separator`.
  - `(trim-whitespace text)`: without leading and trailing whitespace.
  - `(substring text start end)`: the bytes from `start` up to `end` (see E-28).
  - `(position-in-text text part)`: the index of the first occurrence of `part`, or `nothing`.
- **Example**: `(split-text "ana,lisbon,82" ",")` ;=> `["ana" "lisbon" "82"]`
- **Friction removed**: E-01
- **Programs affected**: 02, 03, 06 (03 and 06 become runnable together with K-02)
- **Alternatives**:
  - keep as is: text must arrive already split, and programs 03 and 06 stay pending;
  - one general `split-text` with a pattern instead of three functions: fewer names, but the
    pattern needs a syntax of its own;
  - a separate `text` namespace, like `clojure.string`: groups the names, but adds an import to
    every text program.
- **Owner answer**: approve (2026-11-25; D-65)

### K-02 — Reading data from text

- **Proposal**: `(parse-data text)`: the first value read from `text`. It raises `:syntax` when
  the text is malformed, as the reader does.
- **Example**: `(parse-data "{:id 7 :name \"ana\"}")` ;=> `{:id 7 :name "ana"}`
- **Friction removed**: E-02
- **Programs affected**: 06
- **Alternatives**: keep as is (no program can read data from text); the name `read` (shorter, but
  it suggests reading a file or stream).
- **Owner answer**: approve (2026-11-25; D-66)

### K-03 — Paths in nested data

- **Proposal**:
  - `(get-at-path collection path)`
  - `(put-at-path collection path value)`
  - `(modify-at-path collection path function)`
  - `(modify-value collection key function)`, the one-level case.
- **Example**:
  `(modify-at-path inventory [:warehouses 1 :items 0 :quantity] increment)`, and
  `(modify-value counts word increment)` where a missing key starts as `nothing`. A default can be
  given with `(modify-value counts word (function [n] (+ (or n 0) 1)))`.
- **Friction removed**: E-03, E-04
- **Programs affected**: 01, 02
- **Alternatives**:
  - keep as is: every program with nested data writes the recursive helper;
  - the names `update-at-path` and `update-value`: closer to Clojure, but `update` is the agent
    operation (section 09).
- **Owner answer**: approve (2026-11-25; D-67)

### K-04 — Searching a collection

- **Proposal**:
  - `(find-first predicate collection)`: the first element for which `predicate` is true, or
    `nothing`.
  - `(find-position predicate collection)`: its index, or `nothing`.
- **Example**: `(find-position (function [item] (equal? (get item :product) "nut")) items)`
  ;=> `1`
- **Friction removed**: E-05
- **Programs affected**: 01
- **Alternatives**: keep as is (a `reduce` over `range`); only `find-first`, with the index found
  through `transform-indexed` (K-10).
- **Owner answer**: approve (2026-11-25; D-68)

### K-05 — Effects for each element

- **Proposal**: `(for-each function collection)`: calls `function` on each element in order, for
  its effects, and returns `nothing`.
- **Example**: `(for-each print-line ["north" "south"])`, and with `range` for a counted loop.
- **Friction removed**: E-06
- **Programs affected**: 01, 02, 07
- **Alternatives**: keep as is (a `reduce` with an ignored accumulator); a special form with
  bindings, like Clojure's `doseq` (more syntax for the same effect).
- **Owner answer**: approve (2026-11-25; D-69)

### K-06 — Concatenation

- **Proposal**:
  - `(concatenate-all collections)`: one list with the elements of each collection in order.
  - `(flat-transform function collection)`: `transform`, then concatenate the results.
- **Example**: `(flat-transform (function [warehouse] (get warehouse :items)) warehouses)`
- **Friction removed**: E-07
- **Programs affected**: 01, 04
- **Alternatives**: keep as is (`reduce` with `pour-into`); a variadic `concatenate`, which needs
  another name because program 08 uses that one.
- **Owner answer**: approve (2026-11-25; D-70)

### K-07 — Formatting

- **Proposal**:
  - `(join-text separator collection)`: the elements as text, with `separator` between them.
  - `(pad-start text width)` and `(pad-end text width)`: add spaces before or after, up to `width`.
  - `(format-decimal number places)`: the number as text with `places` digits after the point.
- **Example**: `(format-decimal 84.333 2)` ;=> `"84.33"`
- **Friction removed**: E-08
- **Programs affected**: 01, 02
- **Alternatives**: keep as is (local helpers); one format function with a format string
  (powerful, but a small language of its own).
- **Owner answer**: approve (2026-11-25; D-71)

### K-08 — Ordering

- **Proposal**:
  - `(compare-values a b)`: `-1`, `0` or `1`, for numbers and for strings (by bytes).
  - `(sort collection)`: a vector in ascending order.
  - `(sort-by key-function collection)`: the same, by the value of `key-function`.
- **Example**: `(sort-by (function [pair] (element-at pair 1)) word-counts)`
- **Friction removed**: E-09
- **Programs affected**: 02, 09
- **Alternatives**: keep as is (hand-written insertion sort); make `<` and `>` accept strings
  (no new name, but R4 keeps them as numeric notation).
- **Owner answer**: approve (2026-11-25; D-72)

### K-09 — Numbers

- **Proposal**:
  - `(to-floating-point n)`: an integer as a float.
  - `(round x)`, `(floor x)`, `(ceiling x)` and `(truncate x)`: a float as an integer. They raise
    `:overflow` outside 64 bits.
  - `(square-root x)` and `(power-of base exponent)`.
  - `(divide-floating-point a b)`: always a float, so `/` stays exact (section 05).
- **Example**: `(divide-floating-point (reduce + 0 scores) (count scores))` ;=> `84.33333333333333`
- **Friction removed**: E-10, E-11, E-12
- **Programs affected**: 02, 09
- **Alternatives**:
  - keep as is: `(* 1.0 n)`, and `try` around `/`;
  - make `/` return a float when the quotient is not exact: simpler for a mean, but it changes
    section 05, and the VM inherits it.
- **Owner answer**: approve (2026-11-25; D-73)

### K-10 — Transform with an index

- **Proposal**: `(transform-indexed function collection)`: calls `(function index element)`.
- **Example**: `(transform-indexed (function [index line] [(increment index) line]) lines)`
- **Friction removed**: E-13
- **Programs affected**: 02, 06
- **Alternatives**: keep as is (`range` and `element-at`); let `transform` take several
  collections, like Clojure's `map` (no new name, and it covers walking two collections side by
  side).
- **Owner answer**: approve (2026-11-25; D-74)

### K-11 — Errors

- **Proposal**:
  - `(error-message error)`, next to `error-kind` and `error-data`.
  - `(error kind message data cause)`: an optional fourth argument, read with
    `(error-cause error)`.
  - Core errors put the offending value and its place in `:data`: `:wrong-value`,
    `:argument-position` and `:function-name`.
- **Example**:
  `(raise (error :user "invalid record" {:id 3} problem))`, then `(error-cause caught)` is
  `problem`.
- **Friction removed**: E-14, E-15, E-16
- **Programs affected**: 05, 10
- **Alternatives**: keep as is (`(get problem :message)`, and the original error is lost); the
  location (file and line) is not part of this item, because section 12 leaves it to Phase 7.
- **Owner answer**: approve (2026-11-25; D-75)

### K-12 — Macro expansion

- **Proposal**: `(expand-macro form)` (expands completely) and `(expand-macro-once form)` (one
  step). Neither evaluates.
- **Example**: `(expand-macro-once (quote (repeat-times [index 0 3] (print-line index))))`
- **Friction removed**: E-17
- **Programs affected**: 07
- **Alternatives**: keep as is (run and read the effects); only `expand-macro`.
- **Owner answer**: approve (2026-11-25; D-76)

### K-13 — `case` without a matching clause

- **Proposal**: a `case` with no default whose value matches no clause raises
  `:no-matching-case`, with `{:value v}` in `:data`. A `case` with a default is unchanged.
- **Example**: `(case :times :plus 1 :minus 2)` raises `:no-matching-case`.
- **Friction removed**: E-18
- **Programs affected**: 04, 08
- **Alternatives**:
  - keep as is (`nothing`, and the error surfaces far away);
  - reuse `:wrong-type` (no new kind, but the kind says nothing about the cause).
- **Inherited by**: Phase 3 (it changes the meaning of existing code).
- **Owner answer**: approve (2026-11-25; D-77)

### K-14 — Short map pattern

- **Proposal**: in a map pattern, `:names` followed by a vector of symbols binds each symbol to the
  value of the keyword with the same name.
- **Example**: `(let [{:names [key left right]} tree] …)` binds `key` to `(get tree :key)`, and so
  on.
- **Friction removed**: E-19
- **Programs affected**: 04
- **Alternatives**: keep as is (J2, every name written twice); Clojure's `:keys` (shorter, but the
  word means map keys, not the names bound).
- **Inherited by**: Phase 3 (pattern syntax).
- **Owner answer**: approve (2026-11-25; D-78)

### K-15 — Two load-time checks

The two parts are independent and can be answered separately.

- **Proposal**:
  1. A malformed special form raises `:syntax`, with the line of the form, like the reader.
  2. An unqualified name defined in two imported namespaces raises `:name-clash` at load time,
     naming both namespaces. A qualified name, or a definition in the namespace itself, is
     unaffected. Section 14 rule 1 and its closed open point are made consistent either way.
- **Example**: `(let [x 1 y] x)` raises `:syntax: line 4: malformed let: …`; in program 08,
  `(describe shape)` would raise `:name-clash: describe is defined in geometry and report`.
- **Friction removed**: E-20, E-21
- **Programs affected**: 08, 10
- **Alternatives**:
  - keep as is (`:wrong-type` without a line; the first import wins silently);
  - for part 2, a warning instead of an error (quux has no warnings yet);
  - for part 2, keep "first import wins" and only fix the text of section 14.
- **Inherited by**: Phase 3 (error kinds and name resolution).
- **Owner answer**: approve (2026-11-25; D-79)
