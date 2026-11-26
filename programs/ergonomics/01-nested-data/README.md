# 01 — Nested data

**Area**: 1. nested data transformation.

## What it evaluates

- Maps inside vectors inside a map, built from literals (section 02).
- Reading deep values with `get` on maps and on vectors by index, through a path (section 02).
- Changing deep values with `put` on maps and vectors, through a recursive helper, without
  changing the original (sections 01, 02).
- `thread-first` to chain changes, and `thread-last` pipelines with `transform`, `filter` and
  `reduce` (sections 02, 04).
- A vector pattern with `:rest` in `let` (section 03).
- Output with `print-line` and `to-string`.

## How to run

```bash
build/debug/source/main/quux programs/ergonomics/01-nested-data/main.qx
```

## Expected result

The program prints the number of warehouses and, before and after two restocks, one line per
warehouse with its stock value and the list of out-of-stock products. It then prints the total
number of bolts and the original washer quantity (still 0), and returns the restocked washer item.
It exits with status 0. The exact text is in `expected-output.txt`.

**How the expected output was derived** (D-60): every line was written by hand from the
specification before the first run. The only exception is the order of the keys in the returned
map: the printer writes map entries in the iteration order of the map (CHAMP hash order), which a
reader cannot work out by hand. That line was confirmed by running, and the fact is recorded as
friction P01-F7.

## Status

- runnable
- Verifier: every allocation

## Friction observed

| ID | Location | What was awkward | Workaround | Origin | Review |
|---|---|---|---|---|---|
| P01-F1 | `change-at-path` | No core function replaces or updates a value deep inside nested collections (Clojure has `assoc-in` and `update-in`). Every program with nested data has to write this recursive helper. The natural name, `update`, is already taken by agents (section 09). | A recursive helper with `get`, `put` and a `:rest` pattern | writer | kept |
| P01-F2 | `value-at-path` | No core function reads a value through a path of keys (Clojure's `get-in`). | `reduce` with `get` over the path | writer | kept |
| P01-F3 | `index-of-product` | No core function finds the first element that satisfies a predicate, or its position. Finding one item in a vector takes a `reduce` over `range` with a "found" accumulator. | `reduce` over `(range 0 (count items))` | writer | kept |
| P01-F4 | `report` | No core form runs an effect for each element of a collection (Clojure's `doseq` or `run!`). Printing one line per element uses `reduce` with an accumulator that is ignored. | `reduce` with an `ignored` accumulator and `nothing` as the initial value | writer | kept |
| P01-F5 | `all-items`, `out-of-stock` | No core function concatenates a sequence of collections, or transforms and concatenates (Clojure's `concat` and `mapcat`). | `reduce` with `pour-into` | writer | kept |
| P01-F6 | `stock-value`, `total-quantity` | A total takes two steps, `transform` and then `(reduce + 0)`, because there is no sum function and `reduce` always needs its initial value. This is minor: the pipeline reads well with `thread-last`. | none needed | writer | kept |
| P01-F7 | `main` (returned value) | The program does not control the order in which a map prints: `{:product "washer" :quantity 50 :price 1}` prints as `{:price 1 :product "washer" :quantity 50}`. The order is deterministic but follows the hash, so the reader cannot predict it, and the specification does not describe printed forms at all (sections 01 and 02 only give `;=>` values). | The expected line was confirmed by running | writer | kept |
| P01-F8 | `report`, `main` | Output is built only by concatenating with `to-string`. There is no formatting (padding, number formatting) and no way to join a collection with a separator, so the program prints the out-of-stock vector in its printed form `["south/washer"]` instead of a plain list. | Print the vector's printed form | writer | kept |
