# 05 — Numerics

**Question:** Which numbers exist, and what happens on overflow and inexact division?

**Status:** Normative

## Answer

**Decision:** D-04 (numeric tower), D-18 (deterministic parallel reductions)

### Kinds

1. **Integers** are 64-bit signed, from −9223372036854775808 to 9223372036854775807.
2. **Floats** are IEEE 754 double precision. The special values are written `not-a-number`,
   `infinity` and `negative-infinity`.
3. There are no bignums and no rationals. **Open — revisited after Phase 3**, when a library can
   be chosen at a dependency gate.

### Arithmetic

4. `+`, `-`, `*`, `increment`, `decrement` and `absolute-value` on integers MUST raise
   `:overflow` when the exact result does not fit in 64 bits. They never wrap around.
5. When an argument is a float, the operation is done in floating point (the integer is
   converted). Floating-point operations follow IEEE 754 and never raise `:overflow`; they
   produce `infinity` or `not-a-number` instead.
6. Integer division is explicit:

   | Operation | Result |
   |---|---|
   | `(quotient a b)` | integer quotient, truncated toward zero |
   | `(remainder a b)` | remainder with the sign of `a` |
   | `(modulo a b)` | remainder with the sign of `b` (floored) |
   | `(/ a b)` with two integers | the exact integer quotient, or `:inexact-division` if `b` does not divide `a` |
   | `(/ a b)` with a float | floating-point division |

   An integer division by zero (`quotient`, `remainder`, `modulo`, `/` on integers) raises
   `:division-by-zero`. A float division by zero follows IEEE 754.
7. Comparison: `<`, `>`, `<=`, `>=` and `numerically-equal?` compare across kinds; `equal?` does
   not (section 01). `minimum`, `maximum`, `zero?`, `positive?`, `negative?`, `even?` and
   `odd?` are defined on integers, and the first five also on floats.

### Determinism of parallel reductions

8. Floating-point addition is not associative, so the result of a reduction depends on the
   order in which partial results are combined. Every parallel reduction (`parallel-reduce`,
   `fork-join`, and `parallel-transform` followed by a reduction) MUST combine partial results
   along a **fixed tree** whose shape depends only on the input size. The result is therefore
   bit-identical on every run and on every degree of parallelism. See
   [11-data-parallelism.md](./11-data-parallelism.md).

## Examples

```quux
(+ 1 2)                                   ;=> 3
(+ 1 2.5)                                 ;=> 3.5
(* 4611686018427387904 2)                 ;!! :overflow
(increment 9223372036854775807)           ;!! :overflow
(absolute-value -9223372036854775808)     ;!! :overflow

(/ 10 2)                                  ;=> 5
(/ 10 4)                                  ;!! :inexact-division
(/ 10 4.0)                                ;=> 2.5
(quotient -7 2)                           ;=> -3
(remainder -7 2)                          ;=> -1
(modulo -7 2)                             ;=> 1
(quotient 7 0)                            ;!! :division-by-zero
(/ 1.0 0)                                 ;=> infinity

(< 1 1.5)                                 ;=> true
(numerically-equal? 2 2.0)                ;=> true
(equal? 2 2.0)                            ;=> false
```

## Errors

| Kind | When |
|---|---|
| `:overflow` | an integer result does not fit in 64 bits |
| `:inexact-division` | `/` on two integers whose quotient is not an integer |
| `:division-by-zero` | integer division by zero |
| `:wrong-type` | an arithmetic operation receives a non-number |

## Open points

- Bignums and rationals. **Open — revisited after Phase 3.**
- Overflow checks in native code. **Open — closed in Phase 6** (D-04 is inherited there).

## Used by programs

05 (float reduction with a fixed tree), 08 (sums over large trees), 04 (integer balances).
