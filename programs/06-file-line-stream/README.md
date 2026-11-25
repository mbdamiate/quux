# 06 — File line stream

**What this tests in the design:** a pull-driven pipeline (`file-lines` → `stream-transform` →
`stream-filter` → `stream-take` → `stream-reduce`) over a 10,000-line file.

- Exactly the first five multiples of seven come out, in file order.
- The file is read only as far as demand requires: the line stream is never more than one line
  ahead, so reading stops near line 35, not at line 10,000.
- When `stream-take` is done, the scope cancels the producer and closes the file.

**Specification sections:** 10 streams, appendix input and output.

**Decisions:** D-17, D-20.

**Hypotheses informed:** H11 (stream ownership by scope).
