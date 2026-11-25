# 03 — Evaluation and Tail Calls

**Question:** How are expressions evaluated, and which calls never grow the stack?

**Status:** Normative

## Answer

**Decision:** D-05 (tail calls), D-01 (syntax)

### Evaluation

1. A literal evaluates to itself. A symbol evaluates to the value it is bound to; an unbound
   symbol raises `:undefined-name`.
2. A list `(operator argument …)` is a special form, a macro call or a function call. In a
   function call, the operator and the arguments are evaluated **from left to right**, then the
   function is applied. Calling a function with the wrong number of arguments raises
   `:wrong-argument-count`; calling something that is not a function raises `:wrong-type`.
3. Special forms (see [contracts/core-forms.md](../../specs/001-language-spec/contracts/core-forms.md)):

   | Form | Meaning |
   |---|---|
   | `(quote x)` | `x`, unevaluated |
   | `(if test then else)` | evaluates `then` unless `test` is `false` or `nothing` |
   | `(sequence e1 … en)` | evaluates in order, returns the value of `en` |
   | `(let [name value …] body …)` | sequential local bindings |
   | `(let-functions [(name [parameter …] body …) …] body …)` | local functions that may call each other |
   | `(function [parameter …] body …)` | a closure over the current environment |
   | `(define name value)` | binds `name` in the current namespace (section 14) |
   | `(try body … (catch kind-or-predicate name handler …) (finally cleanup …))` | error handling (section 12) |
   | `(raise error)` | raises an error value |

4. Only `false` and `nothing` count as false in a test.

### Tail positions

A call in tail position MUST NOT grow the stack, whatever function it calls, including mutual
recursion through several functions. There is no `recur`-style special form: every call in tail
position is a proper tail call.

These are the tail positions:

| Form | Tail position |
|---|---|
| `function` body, `define-function` body | the last expression |
| `sequence` | the last expression |
| `let`, `let-functions` | the last expression of the body |
| `if` | both `then` and `else` |
| `conditional` | the result expression of every clause |
| `case` | the result expression of every clause |
| `when`, `unless` | the last expression of the body |
| `and`, `or` | the last operand |
| `thread-first`, `thread-last` | the last call after expansion |

These positions are **not** tail positions:

- The body of `try` when it has `catch` or `finally`, because the handler must stay active.
- The argument expressions of any call.
- The body of `with-scope`, because the scope must join its children after the body returns
  (section 07).

## Examples

```quux
;; 1,000,000 mutually recursive calls in constant stack
(let-functions [(even? [n] (if (zero? n) true  (odd?  (decrement n))))
                (odd?  [n] (if (zero? n) false (even? (decrement n))))]
  (even? 1000000))                         ;=> true

;; a loop is a tail-recursive function
(define-function sum-to [n accumulator]
  (if (zero? n)
    accumulator
    (sum-to (decrement n) (+ accumulator n))))
(sum-to 1000000 0)                         ;=> 500000500000

(undefined-thing 1)                        ;!! :undefined-name
((function [x] x) 1 2)                     ;!! :wrong-argument-count
(42 1)                                     ;!! :wrong-type
```

The local `even?` and `odd?` in the first example shadow the global predicates of the same name
inside the `let-functions` body only.

## Errors

| Kind | When |
|---|---|
| `:undefined-name` | a symbol has no binding |
| `:wrong-argument-count` | a function receives the wrong number of arguments |
| `:wrong-type` | the operator of a call is not a function |

## Open points

- Variadic parameters (`& rest`) are used by `update` and `transact` (section 09), but the
  parameter syntax is not fixed yet. **Open — closed in Phase 1.**
- Destructuring of vectors in parameter lists and `let` (`[[start end]]`), used in section 11.
  **Open — closed in Phase 1.**

## Used by programs

All programs. 08 depends on deep recursion, and every server loop in 10 is a tail-recursive
function.
