# 08 — Combinators

**Question:** How are futures combined, and how do combinators fail?

**Status:** Normative

## Answer

**Decision:** D-12 (combinators), D-11 (several failures), D-07 (states)

Every combinator creates its futures in the current scope (section 07), so everything it starts
is cancelled when the scope is cancelled.

| Form | Result | On failure |
|---|---|---|
| `(all futures)` | a future of a vector with every value, in the order of `futures` | fails with the first error and cancels the futures still `:empty` |
| `(first-success futures)` | a future of the first value that arrives | the others are cancelled once one succeeds; if **all** fail, fails with `:all-failed`, whose `:data` holds `:errors`, a vector of every error in the order of `futures` |
| `(then future function)` | a future of `(function (wait-for future))` | if `future` fails, fails with the same error and does not call `function` |
| `(with-timeout milliseconds future-or-function)` | a future of the same value | if the time runs out first, fails with `:timeout` and cancels the inner computation |
| `(let-parallel [name expression …] body …)` | starts every `expression` as a future, then binds each `name` to its value, in order, and evaluates `body` | the first failure fails the enclosing scope (section 07) |

Rules:

1. `with-timeout` accepts a future, or a function of no arguments that it starts as a future.
   When completion and the timeout race, the first transition wins (section 06, rule 5).
2. `let-parallel` is the sequential-looking form: the body reads like sequential code, but the
   bindings run in parallel. No async marker exists anywhere: any function can wait.
3. A combinator never retries anything.
4. The futures that a combinator cancels because it no longer needs them end `:failed` with
   `:cancelled`, but they do **not** fail their scope (section 07, rule 8). Without this rule,
   `first-success` would always fail the scope that holds its losers.

## Examples

```quux
;; all: a vector of results, in order
(with-scope [scope]
  (wait-for (all [(future (sleep 100) :user)
                  (future (sleep 150) :orders)])))
;=> [:user :orders], after about 150 ms

;; all: the first failure wins and the rest is cancelled
(with-scope [scope]
  (wait-for (all [(future (sleep 10) (raise (error :user "missing user" {})))
                  (future (sleep 10000) :orders)])))
;!! :user

;; first-success: the fastest replica answers; the others are cancelled
(with-scope [scope]
  (wait-for (first-success [(future (sleep 300) :replica-1)
                            (future (sleep 50)  :replica-2)
                            (future (sleep 200) :replica-3)])))
;=> :replica-2

;; first-success: everything fails
(with-scope [scope]
  (wait-for (first-success [(future (raise (error :user "a" {})))
                            (future (raise (error :user "b" {})))])))
;!! :all-failed   ; (get (error-data problem) :errors) holds both :user errors, in order

;; then
(with-scope [scope]
  (wait-for (then (future 20) increment)))           ;=> 21

;; with-timeout
(with-scope [scope]
  (wait-for (with-timeout 100 (function [] (sleep 5000) :late))))
;!! :timeout

(with-scope [scope]
  (wait-for (with-timeout 1000 (function [] (sleep 10) :early))))
;=> :early

;; let-parallel: reads sequentially, runs in parallel
(with-scope [scope]
  (let-parallel [user   (sleep 100)
                 orders (sleep 150)]
    [user orders]))
;=> [nothing nothing], after about 150 ms
```

`sleep` returns `nothing`; reference program 01 shows `let-parallel` with real lookups.

## Errors

| Kind | When |
|---|---|
| any kind | `all` and `then` pass through the error of the future that failed |
| `:all-failed` | every alternative of `first-success` failed |
| `:timeout` | the time of `with-timeout` ran out first |
| `:cancelled` | the enclosing scope was cancelled |

## Open points

- `then` sent to the node that holds a remote future (promise pipelining, as in E and Cap'n
  Proto). **Open — closed in Phase 5a** (H6).

## Used by programs

01 (`all`, `let-parallel`), 02 (`first-success`), 03 (`with-timeout`), 11 and 12 (remote
futures).
