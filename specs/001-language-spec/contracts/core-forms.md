# Contract: special forms and core primitives used by the specification

Every name here is approved in `docs/naming.md` (D-27). This is the normative minimum that the 12 programs may use. Feature 002 chooses which of these
are special forms in the evaluator and which are macros or primitives. Everything listed here
must exist by the end of Phase 1.

## Special forms (evaluation semantics)

| Form | Notes |
|---|---|
| `quote`, `if`, `sequence`, `let`, `let-functions`, `function`, `define` | Tail positions per D-05 |
| `try` / `catch` / `finally`, `raise` | `catch` takes a keyword kind or a predicate (D-13) |
| `define-macro`, `quasi-quote`, `unquote`, `unquote-splicing` | D-06 |
| `namespace` | D-23 |

## Derived forms (macros)

`define-function`, `conditional`, `case`, `when`, `and`, `or`, `thread-first`, `thread-last`, `unless`, `not`, `let-parallel` (D-12),
`with-scope` (D-08), `shield-from-cancellation` (D-09)

## Primitives

| Area | Names | Decisions |
|---|---|---|
| Equality and hash | `equal?`, `not-equal?`, `numerically-equal?`, `hash`, `same-identity?`, `nothing?` | D-02 |
| Numbers | `+ - * / quotient modulo remainder increment decrement absolute-value minimum maximum zero? positive? negative? < > <= >=` | D-04 |
| Collections | `list vector map-of set-of prepend add put remove-key get has-key? count empty? first rest as-sequence transform filter reject reduce pour-into range element-at to-string` | D-03 |
| Futures | `future`, `spawn`, `wait-for`, `state`, `cancel!`, `check-cancellation`, `sleep` | D-07..D-09 |
| Combinators | `all`, `first-success`, `then`, `with-timeout` | D-12 |
| Agents | `agent`, `update`, `transact`, `state` | D-15, D-16 |
| Streams | `make-stream`, `emit`, `stream-transform`, `stream-filter`, `stream-take`, `stream-reduce`, `close!`, `file-lines` | D-17 |
| Parallelism | `parallel-transform`, `parallel-reduce`, `fork-join` | D-18 |
| Errors | `error`, `error-kind`, `error-data`, `unique-symbol` (macros) | D-13, D-06 |
| Identity and distribution | `function-hash`, `serializable?`, `node`, `spawn-on` | D-19..D-22 |
| I/O (programs 6 and 10 only) | `open-file`, `listen`, `accept`, `read-line`, `write-line`, `print-line` | — |
| Test support | `tag` | D-25 |

`tag` is an identity function at run time when no conditions are loaded.

## Actor library (hypothetical, only in `actors.qx`; D-24)

`spawn-actor`, `receive`, `send-message`, `reply`, `link`, `supervise`
