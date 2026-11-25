# Decision log (D-xx)

Project-wide, cumulative log. IDs are continuous across all features and never restart.
Each decision is closed in exactly one feature; its full rationale lives in that feature's
`specs/<feature>/research.md`. This table is the index (replaces the `<previous_state>`
copy-paste of the original prompt modules).

States: `open` | `recommended` | `adopted` | `superseded by D-xx`.

| ID | Decision | Closed in (feature / phase) | State | Inherited by |
|----|----------|-----------------------------|-------|--------------|
| D-01 | S-expression syntax with `[]` `{}` `#{}` literals and keywords; no reader shorthands (`'` `` ` `` `~` `@` removed by D-27) | 001 / Phase 0 | recommended | Phase 1 |
| D-02 | Structural `=` for data; functions by content hash + env; identity for futures, agents, scopes; NaN reflexive in `=` | 001 / Phase 0 | recommended | Phase 3, Phase 5 |
| D-03 | Canonical CHAMP maps/sets, trie vectors (RRB postponed); two hashes: fast value hash + lazy cryptographic content hash | 001 / Phase 0 | recommended | Phase 1, Phase 2, Phase 5 |
| D-04 | 64-bit ints with `:overflow` error, IEEE doubles, no bignums yet | 001 / Phase 0 | adopted (Q3) | Phase 1, Phase 6 |
| D-05 | General proper tail calls; tail positions defined per form | 001 / Phase 0 | recommended | Phase 1, Phase 3, Phase 6 |
| D-06 | Non-hygienic `defmacro` + qualifying syntax-quote + gensym; hash after full expansion | 001 / Phase 0 | recommended | Phase 1, Phase 5 |
| D-07 | Future states empty/filled/failed; cancelled = failed with `:cancelled` | 001 / Phase 0 | recommended | Phase 3, Phase 5 |
| D-08 | `with-scope`, implicit current scope, `future`/`spawn`, root scope, join on exit | 001 / Phase 0 | recommended | Phase 1, Phase 3 |
| D-09 | Cooperative cancellation at defined points; `check-cancelled`, `shield`, `finally` | 001 / Phase 0 | recommended | Phase 3, Phase 4, Phase 5, Phase 6 |
| D-10 | Futures may escape only settled (follows from join) | 001 / Phase 0 | recommended | Phase 3, Phase 5 |
| D-11 | First failure (completion order) primary; other failures suppressed; cancellations caused by the scope or by a combinator do not fail the scope (amended in US2) | 001 / Phase 0 | recommended | Phase 3, Phase 5 |
| D-12 | `all`, `first-success` (`:all-failed`), `then`, `with-timeout`, `let-parallel`; no async marker, `wait-for` suspends | 001 / Phase 0 | recommended | Phase 1, Phase 3 |
| D-13 | Error = value with `:kind` etc.; standard kinds incl. network/timeout, plus `:division-by-zero`, `:index-out-of-range`, `:resource-closed` (amended); no implicit retries | 001 / Phase 0 | recommended | Phase 1, Phase 3, Phase 5 |
| D-14 | Wait-for cycle detection → `:deadlock` (local + simulated nodes; distributed: timeouts) | 001 / Phase 0 | adopted (Q1) | Phase 3, Phase 5 |
| D-15 | Agents: `update`/`transact`/`update-once`, per-sender FIFO, failed update keeps state, not scope-owned | 001 / Phase 0 | recommended | Phase 1, Phase 3, Phase 5 |
| D-16 | Send future in sender scope; on cancel: queued → withdrawn, running → commits; future cancelled (resolves H3) | 001 / Phase 0 | adopted (Q2) | Phase 3, Phase 5 |
| D-17 | Streams: rest is a future, pull-driven, `:maximum-ahead` bound (1), owned by creator scope; producers cancelled when the scope's body returns; `emit` is a cancellation point (amended) | 001 / Phase 0 | recommended | Phase 1, Phase 4, Phase 5 |
| D-18 | Deterministic parallelism via fixed reduction tree; parallel work in child scope | 001 / Phase 0 | adopted (Q3) | Phase 3, Phase 5 |
| D-19 | Function identity by content: expanded AST, de Bruijn, deps by hash, SCC group hashing; closure = hash + env | 001 / Phase 0 | recommended | Phase 1, Phase 5 |
| D-20 | Serialization classes: by value / by remote reference / local-only (`:not-serializable` at sender) | 001 / Phase 0 | recommended | Phase 1, Phase 5 |
| D-21 | Scope id = 128-bit random; reference = id + owner + cancel capability; remote proxy scopes | 001 / Phase 0 | recommended | Phase 3, Phase 5 |
| D-22 | Remote calls explicit only (`spawn-on`, explicit remote agents/streams) | 001 / Phase 0 | recommended | Phase 5 |
| D-23 | Namespaces map names → hashes; redefinition = new hash; reloading policy deferred | 001 / Phase 0 | recommended | Phase 1, Phase 5, Phase 7 |
| D-24 | Actors outside the core; actor versions of programs 4, 10, 12 for comparison | 001 / Phase 0 | recommended | Phase 4, Phase 5 |
| D-25 | Fault injection in `conditions.qx`, outside program source | 001 / Phase 0 | recommended | Phase 1, Phase 5, Phase 7 |
| D-26 | Spec in `docs/spec/NN-topic.md`; programs in `programs/NN-name/` with `expected.qx` as quux data | 001 / Phase 0 | recommended | Phase 1 |
| D-27 | Readable names: full words for quux names and C identifiers, no Lisp heritage; name-by-name approval in `docs/naming.md` (constitution VIII, v1.1.0) | 001 / Phase 0 | adopted (owner) | all phases |

## Pre-existing decisions (from the base block, not numbered)

These were fixed before planning and are encoded in the constitution:

1. Validate the design first with a blocking runtime (thread per future).
2. Then a VM with explicit per-task stack + work-stealing (alternatives compared, not discarded).
3. Shared heap with multi-threaded GC exploiting immutability; root discipline evaluated.
4. Distribution via immutability + content-addressed code (Unison-style), compared with
   Erlang-style and bytecode+environment shipping.
5. C is the only implementation language.
6. Own IR with explicit suspension/cancellation points → LLVM via C API; the VM remains.
| D-28 | Linux x86_64 added as a target, built and tested only in GitHub Actions CI; macOS arm64 and Linux arm64 stay local; native x86_64 backend decided in Phase 6 (constitution v1.2.0) | 001 / Phase 0 | adopted (owner) | Phase 1, Phase 2, Phase 3, Phase 6 |
| D-29 | Compiler pin: clang 22 everywhere (Homebrew `llvm@22`, Ubuntu 26.04 `clang-22`), gcc as second compiler, `-std=gnu17` | 002 / Phase 1 | recommended | Phase 2, Phase 6 |
| D-30 | Linux container: Ubuntu 26.04 LTS arm64, source mounted at `/work`; sanitizer adjustments recorded in `docs/toolchain-record.md` | 002 / Phase 1 | recommended | Phase 2, Phase 5 |
| D-31 | CI on GitHub Actions only on Linux x86_64 (`ubuntu-26.04`), four presets, valgrind and gcc builds; arm64 targets checked locally; single-test re-run for CI-only failures | 002 / Phase 1 | adopted (owner) | all phases |
| D-32 | Modules with one public header and reuse tags (keep / adapt / replace); executor interface as the VM seam (H1) | 002 / Phase 1 | recommended | Phase 3, Phase 4, Phase 5 |
| D-33 | C conventions: `quux_<module>_<words>`, explicit `quux_context`, fallible functions return `bool` + pending error, no `longjmp` | 002 / Phase 1 | recommended | Phase 3 |
| D-34 | Precise roots: per-task shadow stack of root frames holding addresses, global roots, write-barrier hook, root verifier (H2); amended: free all allocations at shutdown, `:resource-exhausted` on allocation failure | 002 / Phase 1 | recommended | Phase 3, Phase 6 |
| D-35 | Tagged 64-bit value: heap pointer, 61-bit small integers, constants; boxed large integers and floats | 002 / Phase 1 | recommended | Phase 3, Phase 6 |
| D-36 | Lists, 32-way vector trie, canonical CHAMP maps/sets with canonical collision nodes and cached node value hashes | 002 / Phase 1 | recommended | Phase 3, Phase 5 |
| D-37 | Provisional hashes: XXH3-64 fixed seed (value), portable BLAKE3 (content), vendored behind `quux_hashing_*` | 002 / Phase 1 | provisional | Phase 2 (H10), Phase 5 |
| D-38 | Full expansion, then a tree walker with a tail-call loop; depth limit raises `:stack-exhausted` | 002 / Phase 1 | recommended | Phase 3 |
| D-39 | `:rest` parameter marker (W4); vector and map patterns in `let` and parameters, lowered by the expander (W5) | 002 / Phase 1 | adopted (owner Q1, Q2; map spelling approved in naming J2) | Phase 3 |
| D-40 | Blocking executor: thread per future, park on condition variable, global wait-for graph for `:deadlock`, `:resource-exhausted` on thread failure | 002 / Phase 1 | recommended | Phase 3 (H8) |
| D-41 | Agents: FIFO queue, on-demand worker thread, `update-once` applied-operations map | 002 / Phase 1 | recommended | Phase 3, Phase 5 |
| D-42 | Streams per D-17; foreign advances counted as H11 evidence | 002 / Phase 1 | recommended | Phase 4, Phase 5 |
| D-43 | Fixed chunk size (512) and fixed reduction tree for parallel operations | 002 / Phase 1 | recommended | Phase 3 (H12), Phase 5 |
| D-44 | Blocking POSIX input/output through `poll` with a per-task wake-up pipe for cancellation | 002 / Phase 1 | recommended | Phase 4 |
| D-45 | Simulator: nodes with root scopes; every inter-node interaction is an encoded message on one network thread; drops, latency, crashes from `conditions.qx` | 002 / Phase 1 | recommended | Phase 5, Phase 7 |
| D-46 | `quux-test` (program, examples, unit), process per program, watchdog, report; CTest labels | 002 / Phase 1 | recommended | all phases |
| D-47 | `quux-name-check` with the quux reader; clang-tidy naming; clang-format check | 002 / Phase 1 | recommended | all phases |
| D-48 | Portability layer: the only place with platform headers and `#if` on system or architecture; no semaphores, barriers or `<threads.h>` | 002 / Phase 1 | recommended | Phase 3, Phase 4, Phase 6 |
| D-49 | Primary error stays in completion order in Phase 1; race evidence recorded (H9) | 002 / Phase 1 | adopted (owner Q3) | Phase 7 |
| D-50 | Kind predicates (`list?`, `vector?`, `map?`, …, `floating-point?`) approved in `docs/naming.md` J3 (W6) | 002 / Phase 1 | adopted (owner) | all phases |
| D-51 | Error kinds `:stack-exhausted` and `:resource-exhausted` (naming J4) | 002 / Phase 1 | adopted (owner) | Phase 3 |
| D-52 | Line-based REPL and file runner, no line-editing library | 002 / Phase 1 | recommended | Phase 7 |
| D-53 | Pre-push git hook runs every preset on macOS arm64 and in the Linux arm64 container and blocks the push on failure; skips label `pending` and presets recorded as unavailable in the container (with a warning); documentation-only pushes run only the name check | 002 / Phase 1 | adopted (owner) | all phases |
| D-54 | Error kind `:syntax` for reader errors (naming J8, W11); an integer literal outside 64 bits stays `:overflow` | 002 / Phase 1 | adopted (owner) | Phase 3 |
| D-55 | Ergonomics programs: one directory per program under `programs/ergonomics/` with `main.qx`, optional `input.txt`, `expected-output.txt`, optional `expected-exit-status.txt`, `README.md` | 003 / between Phase 1 and Phase 3 | recommended | Phase 3 |
| D-56 | Test driver compares stdout and exit status exactly; new `quux-test run` with verifier options; file runner shared as `quux_session_run_file` | 003 | recommended | Phase 3 |
| D-57 | No CI or local-check change: the `ergonomics` label is picked up by label exclusion; valgrind label on Linux debug; TIMEOUT 30 | 003 | recommended | — |
| D-58 | Pending variants call local stubs that raise `:user` "missing: …"; expected output is the intended final output | 003 | recommended | — |
| D-59 | Ten programs: 01 nested data, 02/03 text pair, 04 tree and token-vector parser, 05/06 error pair, 07 macros, 08 namespaces, 09 numerics, 10 common mistakes | 003 | recommended | — |
| D-60 | Expected outputs derived from the specification before the first run; mismatches are bugs, spec ambiguities or program errors, never pasted over | 003 | recommended | Phase 3 |
| D-61 | Stable IDs: friction notes `PNN-Fk`, report entries `E-NN`, mistakes `M-NN`, decision items `K-NN` | 003 | recommended | — |
| D-62 | Add/rename recommendations become W-xx (from W12) and section K `proposed` rows; owner approvals become D-xx | 003 | recommended | Phase 3 |
| D-63 | Performance evidence from CTest times in `release` on macOS arm64; over 1 s or `:stack-exhausted` under 10,000 elements is a finding | 003 | recommended | — |
| D-64 | Other Lisps' idioms cited with links as evidence of need; quux names stay full-word and owner-approved | 003 | recommended | — |
