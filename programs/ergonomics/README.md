# Ergonomics programs (feature 003)

These programs use the sequential core of quux the way a user would, for everyday tasks. Each one
records the places where writing it was awkward ("Friction observed" in its README). The
findings are classified in `docs/ergonomics-report.md`.

They are **not** part of the phase acceptance suite. That suite is the 12 reference programs in
`programs/01-…` to `programs/12-…`. The ergonomics programs run under the CTest label
`ergonomics`, or `pending` while they need something the core does not have (D-55..D-59, in
`specs/003-core-ergonomics/research.md`).

| # | Directory | Area | Variant and partner | Status | Friction notes |
|---|---|---|---|---|---|
| 01 | `01-nested-data` | 1. nested data transformation | — | runnable | 8 |
| 02 | `02-text-prepared-input` | 2. text processing | prepared input; partner 03 | planned | — |
| 03 | `03-text-raw` | 2. text processing | raw text; partner 02 | planned | — |
| 04 | `04-tree-and-parser` | 3. recursion and patterns | — | planned | — |
| 05 | `05-errors-prepared-input` | 4. error handling | prepared input; partner 06 | planned | — |
| 06 | `06-errors-reading-text` | 4. error handling | raw text; partner 05 | planned | — |
| 07 | `07-macros` | 5. user-written macros | — | planned | — |
| 08 | `08-namespaces` | 6. namespaces | — | planned | — |
| 09 | `09-numerics` | 7. numerics | — | planned | — |
| 10 | `10-common-mistakes` | 8. common mistakes | — | planned | — |

Run one program:

```bash
build/debug/source/main/quux programs/ergonomics/01-nested-data/main.qx
```

Run them all as tests:

```bash
ctest --preset debug -L ergonomics
```
