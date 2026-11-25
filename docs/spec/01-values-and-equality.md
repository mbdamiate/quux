# 01 — Values and Equality

**Question:** What values exist, and when are two values equal?

**Status:** Normative

## Answer

**Decision:** D-01, D-02, D-03

### Values and literals

Every value is immutable. The kinds of value are listed in
[data-model.md](../../specs/001-language-spec/data-model.md) (entity `Value`).

| Kind | Literal or constructor |
|---|---|
| absence | `nothing` |
| boolean | `true`, `false` |
| integer | `42`, `-7` (64-bit; see [05-numerics.md](./05-numerics.md)) |
| float | `3.5`, `not-a-number`, `infinity`, `negative-infinity` |
| string | `"text"` |
| keyword | `:name` |
| symbol | `(quote name)` |
| list | `(list 1 2 3)` |
| vector | `[1 2 3]` |
| map | `{:balance 100 :owner "Ana"}` |
| set | `#{1 2 3}` |
| function | `(function [x] (increment x))` |

Futures, agents, scopes, nodes and local resources are also values, but they carry identity;
they are described in sections 06, 07, 09 and 13.

The source syntax is made of S-expressions with the literals above. There are **no reader
shorthands**: no `'x`, `` `x ``, `~x`, `~@x` or `@x` (naming rules R5 and R6). A comment starts
with `;` and runs to the end of the line. A namespace-qualified symbol is written
`namespace/name` (see [14-modules.md](./14-modules.md)). Text the reader cannot read raises
`:syntax` (section 12); an integer literal outside 64 bits raises `:overflow`.

### Equality

1. `equal?` MUST be structural for data values: two lists, vectors, maps or sets are equal when
   they have equal elements (in order for lists and vectors, regardless of order for maps and
   sets).
2. Integers and floats are different kinds: `(equal? 1 1.0)` is `false`.
   `numerically-equal?` compares numbers across kinds.
3. `equal?` MUST be reflexive for every value, including `not-a-number`:
   `(equal? not-a-number not-a-number)` is `true`, so maps and sets keyed by it stay
   well-behaved. `numerically-equal?` follows IEEE 754, so `not-a-number` is not numerically
   equal to itself.
4. Two functions are `equal?` when their content hash (D-19) and their captured environments are
   equal. See [13-serialization-and-identity.md](./13-serialization-and-identity.md).
5. Futures, agents, scopes, nodes and local resources are `equal?` only to themselves.
   Comparing them never waits and never looks at their contents.
6. `same-identity?` is `true` when both arguments are the same object. It is intended for
   values with identity; for data values its result is unspecified, and programs MUST NOT rely
   on it.
7. `not-equal?` is the negation of `equal?`.

### Hashing

`hash` MUST agree with `equal?`: if `(equal? a b)`, then `(equal? (hash a) (hash b))`. For maps
and sets the hash does not depend on construction order (D-03). The complete per-kind rules are
in the table "Equality, hash and serialization" of
[data-model.md](../../specs/001-language-spec/data-model.md#equality-hash-and-serialization-table).

`hash` is the fast **value hash**. The separate cryptographic **content hash** identifies code
and structures across nodes (D-03, section 13).

## Examples

```quux
(equal? [1 2 3] [1 2 3])                     ;=> true
(equal? (list 1 2) [1 2])                    ;=> false   ; list and vector are different kinds
(equal? {:a 1 :b 2} {:b 2 :a 1})             ;=> true
(equal? (hash #{1 2 3}) (hash #{3 2 1}))     ;=> true
(equal? 1 1.0)                               ;=> false
(numerically-equal? 1 1.0)                   ;=> true
(equal? not-a-number not-a-number)           ;=> true
(numerically-equal? not-a-number not-a-number) ;=> false
(not-equal? :a :b)                           ;=> true

(define f (function [x] (increment x)))
(define g (function [y] (increment y)))
(equal? f g)                                 ;=> true    ; same normalized code, no captures
```

<!-- example-requires: concurrency -->
```quux
(with-scope [scope]
  (let [a (future 1)
        b (future 1)]
    (equal? a b)))                           ;=> false   ; futures compare by identity
```

## Errors

Equality and hashing never raise errors and never wait.

## Open points

- `same-identity?` on data values is unspecified. **Open — closed in Phase 3** (it depends on the
  VM's value representation).

## Used by programs

All programs. In particular 05 (`same-as-sequential`, `deterministic` compare results with
`equal?` and `hash`) and 12 (deduplication by operation identifier).
