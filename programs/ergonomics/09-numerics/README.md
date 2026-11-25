# 09 — Numerics

**Area**: 7. numerics.

## What it evaluates

- Mean, median and population variance of integers, dividing exactly when possible and falling
  back to floating point when `/` raises `:inexact-division` (sections 05, 12).
- A mean of floats, and the classic `0.1 + 0.2` rounding (section 05).
- `equal?` against `numerically-equal?` across integers and floats (sections 01, 05).
- Fractions as `[numerator denominator]` in lowest terms, with `quotient`, `remainder` and
  `absolute-value`; the harmonic sum up to 10 (section 05).
- `:overflow` from an integer factorial, compared with the same product in floating point;
  `:division-by-zero`, and a float division by zero giving `infinity` (section 05).

## How to run

```bash
build/debug/source/main/quux programs/ergonomics/09-numerics/main.qx
```

## Expected result

The program prints the statistics of six integers and of three floats, the floating-point
comparisons, three fraction results, two factorials (the second overflows and prints its error
kind), the float factorial, and three division cases. It returns the harmonic sum `[7381 2520]`
and exits with status 0. The exact text is in `expected-output.txt`.

**How the expected output was derived** (D-60): every line was written by hand from the
specification before the first run, including the float values (each printed with the shortest
representation that reads back as the same double, e.g. `151.66666666666666`). The first run
matched.

## Status

- runnable
- Verifier: every allocation

## Friction observed

| ID | Location | What was awkward | Workaround | Origin | Review |
|---|---|---|---|---|---|
| P09-F1 | `sort-numbers`, `insert-in-order` | No sort function (as in P02-F4). A median needs a hand-written insertion sort, built from two `filter` calls per element. | Insertion sort | writer | unreviewed |
| P09-F2 | `main` | No square root or power function, so the standard deviation cannot be computed; the program stops at the variance. | Leave it out | writer | unreviewed |
| P09-F3 | `divide`, `mean`, `median` | A mean of integers needs `try` around `/` to catch `:inexact-division` and retry in floating point: an error used for ordinary control flow. The result's kind then depends on the data (`18` but `15.5`). | `divide` helper | writer | unreviewed |
| P09-F4 | `divide`, `main` | No function converts between integers and floats in either direction: an integer becomes a float only through `(* 1.0 n)`, and a float cannot be rounded, truncated or floored to an integer (as in P02-F5 and P02-F6). | `(* 1.0 n)` | writer | unreviewed |
| P09-F5 | `greatest-common-divisor`, `fraction` | There are no rationals (section 05, open until after Phase 3) and no greatest common divisor in the core, so fraction arithmetic is written by hand. | Local helpers | writer | unreviewed |
| P09-F6 | `value-or-error-kind` | Catching every error needs a predicate that always returns true, `(catch (function [problem] true) problem …)`; there is no catch-all form. | The predicate | writer | unreviewed |
