# Contract: harness report

Decision: D-46. Printed by `quux-test program` on standard output, and written as quux data with
`--report FILE`.

## Text form

```text
program 03-timeout-and-errors   seed 7f3a…   target macos-arm64 asan-ubsan
  held    (raises :user)                         observed :user
  held    (future-error :slow :cancelled)        observed :cancelled
  held    (elapsed-under 1000)                   observed 61 ms
  result  pass (3/3)
  notes   primary-error-raced false; foreign-stream-advances 0; fetches 0
```

A failing assertion prints `FAILED` instead of `held`, with the observed value. A run stopped by
the watchdog prints `result  timeout after <n> ms`.

## Data form (`--report FILE`)

```quux
{:program "03-timeout-and-errors"
 :seed "7f3a…"
 :target {:system :macos :architecture :arm64 :preset :asan-ubsan}
 :assertions [{:assertion (raises :user) :held true :observed :user} …]
 :result :pass
 :notes {:primary-error-raced false
         :foreign-stream-advances 0
         :fetches 0}}
```

The keys of the data form are harness-internal. They are not quux language names, but they
still follow R1 (full words), and they are approved in `docs/naming.md` section J (J5).

## Evidence fields

| Note | Feeds |
|---|---|
| `:primary-error-raced` | H9 (D-49) |
| `:foreign-stream-advances` | H11 (D-42) |
| `:fetches` | H4 / Phase 5c |
