# 02 — Collections

**Question:** Which collections exist, and what does "canonical" guarantee?

**Status:** Normative

## Answer

**Decision:** D-03 (structures and hashes), D-01 (literals)

### Kinds

| Kind | Literal | Constructor | Structure |
|---|---|---|---|
| list | — | `(list 1 2 3)` | persistent singly linked list |
| vector | `[1 2 3]` | `(vector 1 2 3)` | persistent bit-partitioned trie with a tail |
| map | `{:a 1 :b 2}` | `(map-of :a 1 :b 2)` | CHAMP, canonical |
| set | `#{1 2 3}` | `(set-of 1 2 3)` | CHAMP, canonical |

Every operation returns a new collection and leaves its argument unchanged. A new collection
shares structure with the old one wherever possible.

### Canonical form

1. Two maps (or two sets) with equal contents MUST have the same internal structure, regardless
   of the order of insertions and removals that built them. This is the canonical form of CHAMP
   (Steindorfer and Vinju, OOPSLA 2015).
2. As a consequence, equal maps and sets have the same `hash` (section 01) and the same
   **content hash** (section 13). Phase 5c relies on this to transfer only the nodes that a
   receiver lacks.
3. Vectors and lists are canonical by construction: their structure depends only on their
   elements and their order. Concatenation in logarithmic time (RRB vectors) is not provided.
   **Open — closed in Phase 3** (added only if a reference program needs it).

### Operations

| Operation | Meaning |
|---|---|
| `(add collection value)` | adds to a set; appends to the end of a vector; prepends to a list |
| `(prepend value list)` | a new list with `value` first |
| `(put map key value)` | a map with `key` bound to `value` |
| `(remove-key map key)` | a map without `key` |
| `(get map key)` / `(get map key default)` | the value for `key`, or `nothing` / `default` when absent |
| `(has-key? map key)` | whether `key` is present; for a set, whether the element is present |
| `(element-at vector index)` | the element at `index`; raises `:index-out-of-range` outside `0 … count - 1` |
| `(count collection)`, `(empty? collection)` | size; whether the size is zero |
| `(first sequence)`, `(rest sequence)` | first element (or `nothing`); the remaining elements as a sequence |
| `(as-sequence collection)` | an ordered view: vectors and lists in order; maps and sets in an order that is fixed for a given content |
| `(transform function collection)` | a list of `(function element)` for each element |
| `(filter predicate collection)`, `(reject predicate collection)` | the elements that satisfy, or do not satisfy, the predicate |
| `(reduce function initial collection)` | a left fold; the initial value is always required |
| `(pour-into target collection)` | adds every element of `collection` to `target` |
| `(range start end)` | the integers from `start` up to, but not including, `end` |
| `(to-string value)` | a printed representation |

The iteration order of a map or set is a function of its contents only. Two equal maps iterate
in the same order, on every run and on every node.

## Examples

```quux
(define account {:owner "Ana" :balance 100})
(put account :balance 80)                ;=> {:owner "Ana" :balance 80}
account                                  ;=> {:owner "Ana" :balance 100}   ; unchanged
(remove-key account :owner)              ;=> {:balance 100}
(get account :limit)                     ;=> nothing
(get account :limit 0)                   ;=> 0
(has-key? #{1 2 3} 2)                    ;=> true

(add [1 2] 3)                            ;=> [1 2 3]
(add (list 1 2) 0)                       ;=> (list 0 1 2)
(element-at [10 20 30] 1)                ;=> 20
(element-at [10 20 30] 3)                ;!! :index-out-of-range

;; canonical form: different construction history, same structure and hash
(define left  (put (put (map-of) :x 1) :y 2))
(define right (remove-key (put (put (put (map-of) :y 2) :z 3) :x 1) :z))
(equal? left right)                      ;=> true
(equal? (hash left) (hash right))        ;=> true
(equal? (as-sequence left) (as-sequence right))   ;=> true   ; same iteration order

(reduce + 0 (filter even? (range 0 10)))         ;=> 20
(pour-into #{} [1 2 2 3])                        ;=> #{1 2 3}
```

## Errors

| Kind | When |
|---|---|
| `:index-out-of-range` | `element-at` outside `0 … count - 1` |
| `:wrong-type` | an operation receives a value of the wrong kind (for example `put` on a vector with a non-integer key) |

## Open points

- RRB vectors (logarithmic concatenation). **Open — closed in Phase 3.**
- Whether `transform`, `filter` and `reject` return lists or keep the input kind. This section
  fixes lists; revisit if the reference programs show the need. **Open — closed in Phase 1.**

## Used by programs

01, 04, 05, 08 (vectors and maps), 10 and 12 (maps as agent state), and every `expected.qx`.
