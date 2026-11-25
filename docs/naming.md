# quux naming glossary

This is the normative list of public quux names and of the abbreviations allowed in C code
(constitution Principle VIII, decision D-27).

**Status values:**

- `approved`: may be used in the specification, the programs and the code.
- `proposed`: waiting for the owner's review.
- `rejected`: must not be used.

New names enter as `proposed` and must be approved before use. The whole table was reviewed
name by name by the owner on 2026-11-24.

---

## A. General rules

| # | Rule | Decision | Status |
|---|---|---|---|
| R1 | Case style | `kebab-case` with full words (`define-function`) | approved |
| R2 | Predicates | Suffix `?` (`equal?`, `empty?`, `nothing?`, `serializable?`) | approved |
| R3 | Operations with a visible effect on identity values | Suffix `!` (`cancel!`, `close!`) | approved |
| R4 | Mathematical operators | Kept as notation: `+ - * / < > <= >=` | approved |
| R5 | Reader shorthands for quoting | **All removed.** No `'x`, `` `x ``, `~x` or `~@x`. Write `(quote x)`, `(quasi-quote …)`, `(unquote x)`, `(unquote-splicing x)` | approved |
| R6 | `@x` shorthand for waiting on a future | **Removed.** Always `(wait-for x)` | approved |
| R7 | Absence of value | `nothing` (Lisp `nil`); `true`, `false`. Predicate: `nothing?` | approved |
| R8 | Literals | Kept as notation: `[]` vector, `{}` map, `#{}` set, `:name` keyword | approved |
| R9 | Source file extension | `.qx`, the only accepted extension (an approved exception, see below) | approved |
| R10 | Special floating-point values | `not-a-number`, `infinity`, `negative-infinity` | approved |

## Approved exceptions for quux names

| Name | Why it is allowed | Approved |
|---|---|---|
| `.qx` (file extension) | Owner's preference for short extensions; it is the only accepted extension | 2026-11-24 |

## B. Special forms and core macros

| Lisp tradition | quux name | Status |
|---|---|---|
| `quote` | `quote` | approved |
| `if` | `if` | approved |
| `do` / `begin` / `progn` | `sequence` | approved |
| `let` | `let` | approved |
| `letfn` / `letrec` / `labels` | `let-functions` | approved |
| `fn` / `lambda` | `function` | approved |
| `def` | `define` | approved |
| `defn` | `define-function` | approved |
| `defmacro` | `define-macro` | approved |
| `ns` | `namespace` | approved |
| `:require` (in `ns`) | `:import` | approved |
| `cond` | `conditional` | approved |
| `case` | `case` | approved |
| `when` / `unless` | `when` / `unless` | approved |
| `and`, `or`, `not` | `and`, `or`, `not` | approved |
| `->` / `->>` | `thread-first` / `thread-last` | approved |
| `try` / `catch` / `finally` | `try` / `catch` / `finally` | approved |
| `throw` | `raise` | approved |
| `` ` `` / `~` / `~@` (syntax-quote, unquote, splicing) | `quasi-quote` / `unquote` / `unquote-splicing` (forms only; R5) | approved |
| `gensym` | `unique-symbol` | approved |
| `x#` (auto-gensym) | removed; use `unique-symbol` explicitly | approved |

## C. Equality, hashing, numbers

| Lisp tradition | quux name | Status |
|---|---|---|
| `=` / `equal?` | `equal?` | approved |
| `not=` | `not-equal?` | approved |
| `==` (numeric across types) | `numerically-equal?` | approved |
| `identical?` / `eq?` | `same-identity?` | approved |
| `hash` | `hash` | approved |
| `+ - * / < > <= >=` | kept (R4) | approved |
| `quot` | `quotient` | approved |
| `mod` / `rem` | `modulo` / `remainder` | approved |
| `inc` / `dec` | `increment` / `decrement` | approved |
| `abs` | `absolute-value` | approved |
| `min` / `max` | `minimum` / `maximum` | approved |
| `zero?` `pos?` `neg?` | `zero?` `positive?` `negative?` | approved |

## D. Collections and sequences

| Lisp tradition | quux name | Status |
|---|---|---|
| `list` / `vector` | `list` / `vector` | approved |
| `hash-map` / `hash-set` | `map-of` / `set-of` (the types are named `map` and `set`) | approved |
| `cons` | `prepend` | approved |
| `car`/`first`, `cdr`/`rest` | `first`, `rest` | approved |
| `conj` | `add` | approved |
| `assoc` / `dissoc` | `put` / `remove-key` | approved |
| `get` | `get` | approved |
| `contains?` | `has-key?` | approved |
| `count` / `empty?` | `count` / `empty?` | approved |
| `seq` | `as-sequence` | approved |
| `map` (the function) | `transform` | approved |
| `filter` / `remove` | `filter` / `reject` | approved |
| `reduce` | `reduce` | approved |
| `into` | `pour-into` | approved |
| `range` | `range` | approved |
| `nth` | `element-at` | approved |
| `str` | `to-string` | approved |
| `println` | `print-line` | approved |

## E. Futures, scopes, cancellation, combinators

| Earlier draft | quux name | Status |
|---|---|---|
| `future`, `spawn`, `state` | `future`, `spawn`, `state` | approved |
| `deref` / `@` | `wait-for` | approved |
| `with-scope` | `with-scope` | approved |
| `cancel!` | `cancel!` (R3) | approved |
| `check-cancelled` | `check-cancellation` | approved |
| `shield` | `shield-from-cancellation` | approved |
| `sleep` | `sleep` | approved |
| `all` | `all` | approved |
| `any` | `first-success` | approved |
| `then` | `then` | approved |
| `timeout` | `with-timeout` | approved |
| `async` | removed (it duplicates `future`) | approved |
| `let-par` | `let-parallel` | approved |

## F. Agents, streams, parallelism

| Earlier draft | quux name | Status |
|---|---|---|
| `agent` | `agent` | approved |
| `update` / `transact` | `update` / `transact` | approved |
| `stream-from` | `make-stream` | approved |
| `emit` | `emit` | approved |
| `smap` / `sfilter` / `stake` / `sreduce` | `stream-transform` / `stream-filter` / `stream-take` / `stream-reduce` | approved |
| `close!` | `close!` (R3) | approved |
| `:ahead` | `:maximum-ahead` | approved |
| `lines` | `file-lines` | approved |
| `pmap` / `preduce` | `parallel-transform` / `parallel-reduce` | approved |
| `fork-join` | `fork-join` | approved |

## G. Errors, identity, distribution, test support

| Earlier draft | quux name | Status |
|---|---|---|
| `error`, `error-kind`, `error-data`, key `:message` | unchanged | approved |
| Error kinds `:arity`, `:type`, `:unbound`, `:inexact` | `:wrong-argument-count`, `:wrong-type`, `:undefined-name`, `:inexact-division` | approved |
| Error kinds `:timeout`, `:cancelled`, `:deadlock`, `:overflow`, `:all-failed`, `:node-unreachable`, `:transport`, `:remote`, `:not-serializable`, `:user` | unchanged | approved |
| (new, found while writing sections 02 and 05) | `:division-by-zero`, `:index-out-of-range` | approved |
| (new, found while writing sections 04 and 11) | namespace `core` (holds the core forms); `fork-join` split markers `:leaf`, `:split` | approved |
| (new, found while writing programs 05, 06, 08 and 10) | `connect`, `parse-integer`, error kind `:resource-closed`, harness reference function `sequential-main` | approved |
| `fn-hash` | `function-hash` | approved |
| `serializable?`, `node`, `spawn-on`, `tag` | unchanged | approved |
| `open-file`, `listen`, `accept`, `read-line`, `write-line` | unchanged | approved |
| Actor library: `spawn-actor`, `receive`, `send-msg`, `reply`, `link`, `supervise` | `send-msg` → `send-message`; the rest unchanged | approved |
| Assertion `max-ahead` | `maximum-ahead` | approved |
| `main.qx`, `conditions.qx`, `expected.qx`, `actors.qx` | unchanged (R9) | approved |

## I. States, error fields, test harness (added after `/speckit-analyze`, finding C1)

| Group | Names | Status |
|---|---|---|
| I1. Future states | `:empty`, `:filled`, `:failed` | approved |
| I2. Scope states | `:running`, `:cancelling`, `:joined-successfully`, `:joined-failed` | approved |
| I3. Error map keys | `:kind`, `:message`, `:data`, `:origin` (with `:node`, `:scope`), `:cause`, `:suppressed`, `:errors` (on `:all-failed`), `:cycle` (on `:deadlock`) | approved |
| I4. Predicates used in examples | `even?`, `odd?` | approved |
| I5. Program entry point | `main` | approved |
| I6. Assertions in `expected.qx` | `returns`, `returns-satisfying`, `raises`, `raises-one-of`, `suppressed-includes`, `future-state`, `future-error`, `agent-final`, `never`, `maximum-ahead`, `elapsed-under`, `elapsed-at-least`, `output-lines`, `same-as-sequential`, `deterministic`, `applied-once` | approved |
| I7. Keys in `conditions.qx` | `:input`, `:delays`, `:fail`, `:after-milliseconds`, `:nodes`, `:name`, `:crash-after`, `:completed`, `:network`, `:drop`, `:from`, `:to`, `:message`, `:acknowledgement`, `:occurrence`, `:latency-milliseconds`, `:files`, `:generate`, `:numbered-lines`, `:count`, `:bounds`, `:wall-clock-milliseconds` | approved |

---

## H. C code: abbreviations allowed as exceptions

The default rule is full words in `snake_case`, with the prefix `quux_` on exported symbols
(`quux_value_allocate`, `quux_future_fill`, `quux_scope_cancel`).

| Abbreviation | Meaning | Status |
|---|---|---|
| `i`, `j`, `k` | loop counters | approved |
| `champ`, `hamt`, `rrb` | names of data-structure algorithms | approved |
| `utf8`, `llvm`, `posix`, `ieee` | names of standards and external projects | approved |
| Names from libc, POSIX, pthreads and libraries (`malloc`, `pthread_mutex_t`, `LLVMBuildAdd`) | imposed from outside; wrap them behind full-word names at module boundaries where practical | approved |
| `ssa`, `ir` | **not allowed**: write `static_single_assignment`, `intermediate_representation` | approved |
| `gc`, `id`, `tco` | **not allowed**: write `garbage_collector`, `identifier`, `tail_call` | approved |
| `ptr`, `len`, `buf`, `ctx`, `tmp`, `val`, `obj`, `env`, `fn`, `args` | **not allowed**: write `pointer`, `length`, `buffer`, `context`, `temporary`, `value`, `object`, `environment`, `function`, `arguments` | approved |
