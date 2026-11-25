# 14 — Modules

**Question:** How are names organized and bound when code is identified by content?

**Status:** Partially open

## Answer

**Decision:** D-23 (namespaces bind names to hashes), D-19 (content identity)

1. `(namespace name (:import other-namespace …))` starts a namespace and makes the public names of
   the imported namespaces available, qualified as `other-namespace/name` or unqualified when
   there is no clash.
2. A namespace is an immutable map from **names to definition hashes**. A name is only a label:
   the identity of a definition is its content hash (section 13).
3. `(define name value)` and `define-function` bind `name` to the hash of the new definition. A
   redefinition creates a **new** hash and rebinds the name. Code compiled earlier keeps the hash
   it referenced; closures created earlier keep calling the old definition.
4. Renaming a definition does not change its hash. Two nodes that hold the same definition under
   different names agree on its identity.
5. In the interactive loop, a name is resolved when a form is evaluated, so the next form sees a
   redefinition.

## Examples

```quux
(namespace accounts)
(define-function balance-of [account] (get account :balance))

(namespace bank (:import accounts))
(balance-of {:balance 120})                             ;=> 120

(define-function fee [amount] (quotient amount 100))
(define first-fee-hash (function-hash fee))

(define-function fee [amount] (quotient amount 50))     ; redefinition
(equal? (function-hash fee) first-fee-hash)            ;=> false

;; renaming keeps identity
(define-function charge [amount] (quotient amount 50))
(equal? (function-hash charge) (function-hash fee))    ;=> true
```

## Errors

| Kind | When |
|---|---|
| `:undefined-name` | an unqualified or qualified name is not bound |

## Open points

- The policy for **code reloading**: when and how live references are re-resolved to new
  hashes. **Open — closed in Phase 5c and Phase 7.**
- How a namespace name relates to package distribution. **Open — closed in Phase 7** (packages).
- None for the core forms: they live in the namespace `core`, which is imported implicitly into
  every namespace, and a name is looked up in the namespace itself, then in its imports in
  order, then in `core` (closed in Phase 1, feature 002).

## Used by programs

All programs live in a namespace. 11 and 12 depend on the same definitions having the same hash
on every node.
