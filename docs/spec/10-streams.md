# 10 — Streams

**Question:** How are asynchronous sequences consumed with backpressure?

**Status:** Normative

## Answer

**Decision:** D-17 (streams), D-09 (stream advance is a cancellation point)

### Shape

1. A stream is either `nothing` (the end) or a cell whose `first` is a value and whose `rest` is
   a **future of the next stream** (`Stream` in
   [data-model.md](../../specs/001-language-spec/data-model.md)).
2. `(first stream)` returns the value. `(rest stream)` returns the future of the rest, and
   `(wait-for (rest stream))` **advances** the stream. Advancing is a cancellation point.

### Producing

3. `(make-stream producer-function)` and `(make-stream producer-function options)` start
   `producer-function` as a task in the **current scope** and return the stream. The producer
   receives one argument, a sink; `(emit sink value)` delivers the next value. When the producer
   returns, the stream ends; when it raises, advancing past the last delivered value raises the
   same error.
4. The option `:maximum-ahead` (`Stream.maximum-ahead`, default 1) bounds how far the producer
   may run ahead of the consumer: `emit` suspends while the producer has delivered
   `:maximum-ahead` values that the consumer has not yet asked for. Demand comes only from
   advancing, so backpressure needs no other protocol. `emit` is a cancellation point while it
   waits.
5. The stream belongs to the scope that created it (`Stream.owner-scope`). When that scope's
   body returns, or when `(close! stream)` is called, the producer is cancelled; advancing past
   the last delivered value then raises `:cancelled`. The cancellation of a producer at the end
   of its scope does not fail the scope (section 07, rule 4).

### Operations

| Operation | Result |
|---|---|
| `(stream-transform function stream)` | a stream of `(function value)` for each value; `function` may wait |
| `(stream-filter predicate stream)` | a stream of the values that satisfy `predicate` |
| `(stream-take count stream)` | a stream of the first `count` values; after the last one it stops pulling and closes its source |
| `(stream-reduce function initial stream)` | a future of the left fold over the whole stream |
| `(file-lines path)` | a stream of the lines of a file (see the input and output appendix) |

Each operation is lazy and pull-driven: it asks its source for a value only when its own consumer
advances. Every operation keeps the `:maximum-ahead` bound of its source.

## Examples

```quux
;; a producer of 1, 2, 3
(define-function produce-three [sink]
  (emit sink 1)
  (emit sink 2)
  (emit sink 3))

(with-scope [scope]
  (wait-for (stream-reduce + 0 (make-stream produce-three))))       ;=> 6

;; an infinite producer is safe: stream-take stops pulling, and the scope cancels the producer
(define-function produce-forever [sink]
  (let-functions [(loop [n] (emit sink n) (loop (increment n)))]
    (loop 0)))

(with-scope [scope]
  (wait-for (stream-reduce add []
              (stream-take 3
                (stream-filter even?
                  (stream-transform increment (make-stream produce-forever)))))))
;=> [2 4 6]

;; backpressure: the producer is at most 1 value ahead of a slow consumer
(with-scope [scope]
  (let [numbers (make-stream produce-forever {:maximum-ahead 1})]
    (sleep 1000)          ; meanwhile the producer has emitted at most 1 value
    (first numbers)))     ;=> 0
```

`add` with a vector appends, so the fold builds the vector in order (section 02).

## Errors

| Kind | When |
|---|---|
| `:cancelled` | advancing a stream whose producer was cancelled (scope ended or `close!`) |
| any kind | the producer raised; the error appears when the consumer advances past the last delivered value |

## Open points

- H11: whether handing a live stream to a sibling scope is a common need that the ownership rule
  makes awkward. **Open — closed in Phase 1** (programs 06, 07, 10).
- Streams between machines, where `:maximum-ahead` becomes a credit window. **Open — closed in
  Phase 5e.**

## Used by programs

06 (file lines), 07 (backpressure), 10 (connection lines), 12 (stream between machines).
