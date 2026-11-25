# 11 — Data Parallelism

**Question:** How is parallel work kept deterministic?

**Status:** Normative

## Answer

**Decision:** D-18 (fixed reduction tree), D-04 (floats), D-08 (child scopes)

### Operations

| Operation | Result |
|---|---|
| `(parallel-transform function collection)` | a vector of `(function element)`, **in the order of** `collection` |
| `(parallel-reduce function initial collection)` | the reduction of `collection`, combining partial results along a fixed tree |
| `(fork-join split combine problem)` | solves `problem` recursively: `split` returns either `[:leaf value]` or `[:split sub-problem …]`; the sub-problems run in parallel and their results are joined with `combine`, in the order `split` returned them |

### Guarantees

1. For pure functions, `parallel-transform` and `fork-join` MUST return a result `equal?` to
   their sequential counterparts (`transform`, plain recursion), with the same `hash`.
   `parallel-reduce` with a pure **associative** function on exact values (integers,
   collections) MUST return the same result as `reduce`.
2. `parallel-reduce` and `fork-join` combine partial results along a **fixed tree**. The tree's
   shape depends only on the size of the input and a chunk size that is a constant of the
   implementation, never on scheduling. The scheduler may run the chunks in any order, but the
   combination order is fixed. Floating-point results are therefore bit-identical on every run
   and with any number of workers.
3. Floating-point addition is not associative, so `parallel-reduce` over floats may differ
   from the left fold of `reduce` in the last bits. The reference result is the reduction
   **along the fixed tree**, and that result never changes between runs.
4. All parallel work runs in a **child scope** of the current scope. If any chunk or
   sub-problem fails, the remaining ones are cancelled and the error propagates (section 07).

## Examples

```quux
(define numbers (pour-into [] (range 0 1000000)))

(equal? (parallel-transform increment numbers)
        (pour-into [] (transform increment numbers)))        ;=> true

(parallel-reduce + 0 numbers)                                 ;=> 499999500000

;; floats: the same value on every run
(define fractions (parallel-transform (function [n] (/ n 3.0)) numbers))
(equal? (parallel-reduce + 0.0 fractions)
        (parallel-reduce + 0.0 fractions))                    ;=> true, always

;; fork-join: sum of the integers in [start, end) by halving the range
(define-function split-range [[start end]]
  (if (<= (- end start) 1000)
    [:leaf (reduce + 0 (range start end))]
    (let [middle (quotient (+ start end) 2)]
      [:split [start middle] [middle end]])))

(fork-join split-range + [0 1000000])                         ;=> 499999500000
```

Reference program 08 contains the complete fork/join examples. `split-range` destructures its
argument; destructuring in parameter lists is an open point of section 03.

## Errors

| Kind | When |
|---|---|
| any kind | a chunk or sub-problem raised; the others are cancelled |
| `:cancelled` | the enclosing scope was cancelled |

## Open points

- H12: a fixed tree limits how well work-stealing balances irregular work (unbalanced trees in
  program 08). **Open — closed in Phase 3.**
- Kind predicates (`vector?`, `map?`, `list?` …) are not in the glossary yet. **Open — closed in
  Phase 1** (to be proposed in `docs/naming.md` when program 08 is written).
- Distributed partitioning that follows the same tree. **Open — closed in Phase 5e.**

## Used by programs

05 (`parallel-transform`, determinism), 08 (`fork-join`).
