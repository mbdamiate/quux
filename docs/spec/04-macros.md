# 04 — Macros

**Question:** How are macros written and expanded, and how do they relate to code identity?

**Status:** Normative

## Answer

**Decision:** D-06 (macros and hashing), D-27 (names: no reader shorthands)

### Writing macros

1. `(define-macro name [parameter …] body …)` defines a macro. When the expander sees
   `(name argument …)`, it calls the macro with the **unevaluated** arguments and replaces the
   call with the returned form.
2. Macros are **not hygienic**. A macro can capture or shadow names at the call site, and the
   programmer avoids this explicitly.
3. Templates are written with forms; there are no reader shorthands:

   | Form | Meaning |
   |---|---|
   | `(quasi-quote template)` | the template as data, with its symbols qualified by the namespace where the macro is defined |
   | `(unquote expression)` | inside `quasi-quote`: inserts the value of `expression` |
   | `(unquote-splicing expression)` | inside `quasi-quote`: inserts the elements of a sequence |

4. `quasi-quote` qualifies every free symbol in the template with its namespace (for example,
   `print-line` becomes `core/print-line`). This prevents the most common capture: a call-site
   definition shadowing a function that the macro uses.
5. There is no automatic generation of fresh names. `(unique-symbol "prefix")` returns a symbol
   that is guaranteed not to clash with any other symbol. Macros that introduce local bindings
   MUST use it.

### Expansion and identity

6. Macros expand completely, from the outside in, before a definition is evaluated or hashed.
7. The content hash of a definition (section 13) covers the **expanded** code only. A macro is
   a dependency at expansion time, not at run time: changing a macro changes the hash of a
   definition only when that definition is expanded again.
8. As a consequence, a node that receives code by hash never needs the macros that produced it.

## Examples

```quux
;; a macro that evaluates its body twice
(define-macro twice [expression]
  (quasi-quote (sequence (unquote expression) (unquote expression))))

(twice (print-line "hello"))
;; expands to (sequence (core/print-line "hello") (core/print-line "hello"))
;; output: hello, hello

;; unique-symbol: the macro's local binding can never clash with names in `body`
(define-macro with-doubled [name expression body]
  (let [temporary (unique-symbol "doubled")]
    (quasi-quote
      (let [(unquote temporary) (* 2 (unquote expression))
            (unquote name)      (unquote temporary)]
        (unquote body)))))

(with-doubled result 21 (increment result))         ;=> 43

;; splicing: the argument is an unevaluated vector of strings
(define-macro print-all [lines]
  (quasi-quote
    (sequence (unquote-splicing
                (transform (function [line] (list (quote print-line) line)) lines)))))

(print-all ["first" "second"])
;; expands to (sequence (print-line "first") (print-line "second"))
```

## Errors

| Kind | When |
|---|---|
| `:wrong-argument-count` | a macro is called with the wrong number of arguments |
| `:wrong-type` | `unquote` or `unquote-splicing` appears outside `quasi-quote`, or splicing receives a non-sequence |

## Open points

- Full hygiene (`syntax-rules` style) is a rejected alternative for now. It can be added later
  without changing any hash, because hashing happens after expansion. **Open — closed in Phase 7**
  (tooling), if macro bugs show up.
- Variadic macro parameters. **Open — closed in Phase 1** (same open point as section 03).

## Used by programs

Indirectly by every program: `define-function`, `conditional`, `when`, `unless`, `with-scope`,
`let-parallel`, `thread-first` and `thread-last` are macros.
