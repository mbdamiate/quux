# 07 — Backpressure

**What this tests in the design:** a producer that could emit one item per millisecond is
connected to a consumer that takes 50 ms per item. Because demand comes only from advancing the
stream, `emit` suspends and the producer never runs more than `:maximum-ahead` (1) items ahead.
The run is paced by the consumer, and the infinite producer is cancelled when the scope ends.

**Specification sections:** 10 streams, 07 scopes (producer cancelled at scope end).

**Decisions:** D-17.

**Hypotheses informed:** H11.
