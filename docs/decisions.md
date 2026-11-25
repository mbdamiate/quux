# Decision log (D-xx)

Project-wide, cumulative log. IDs are continuous across all features and never restart.
Each decision is closed in exactly one feature; its full rationale lives in that feature's
`specs/<feature>/research.md`. This table is the index (replaces the `<previous_state>`
copy-paste of the original prompt modules).

States: `open` | `recommended` | `adopted` | `superseded by D-xx`.

| ID | Decision | Closed in (feature / phase) | State | Inherited by |
|----|----------|-----------------------------|-------|--------------|
| D-01 | Clojure-like S-expression syntax (`[]` `{}` `#{}` keywords, `@`) | 001 / Phase 0 | recommended | Phase 1 |
| D-02 | Structural `=` for data; functions by content hash + env; identity for futures, agents, scopes; NaN reflexive in `=` | 001 / Phase 0 | recommended | Phase 3, Phase 5 |
| D-03 | Canonical CHAMP maps/sets, trie vectors (RRB postponed); two hashes: fast value hash + lazy cryptographic content hash | 001 / Phase 0 | recommended | Phase 1, Phase 2, Phase 5 |
| D-04 | 64-bit ints with `:overflow` error, IEEE doubles, no bignums yet | 001 / Phase 0 | adopted (Q3) | Phase 1, Phase 6 |
| D-05 | General proper tail calls; tail positions defined per form | 001 / Phase 0 | recommended | Phase 1, Phase 3, Phase 6 |
| D-06 | Non-hygienic `defmacro` + qualifying syntax-quote + gensym; hash after full expansion | 001 / Phase 0 | recommended | Phase 1, Phase 5 |
| D-07 | Future states empty/filled/failed; cancelled = failed with `:cancelled` | 001 / Phase 0 | recommended | Phase 3, Phase 5 |
| D-08 | `with-scope`, implicit current scope, `future`/`spawn`, root scope, join on exit | 001 / Phase 0 | recommended | Phase 1, Phase 3 |
| D-09 | Cooperative cancellation at defined points; `check-cancelled`, `shield`, `finally` | 001 / Phase 0 | recommended | Phase 3, Phase 4, Phase 5, Phase 6 |
| D-10 | Futures may escape only settled (follows from join) | 001 / Phase 0 | recommended | Phase 3, Phase 5 |
| D-11 | First failure (completion order) primary; other failures suppressed | 001 / Phase 0 | recommended | Phase 3, Phase 5 |
| D-12 | `all`, `any` (`:all-failed`), `then`, `timeout`, `async`, `let-par`; no await keyword | 001 / Phase 0 | recommended | Phase 1, Phase 3 |
| D-13 | Error = value with `:kind` etc.; standard kinds incl. network/timeout; no implicit retries | 001 / Phase 0 | recommended | Phase 1, Phase 3, Phase 5 |
| D-14 | Wait-for cycle detection → `:deadlock` (local + simulated nodes; distributed: timeouts) | 001 / Phase 0 | adopted (Q1) | Phase 3, Phase 5 |
| D-15 | Agents: `update`/`transact`, per-sender FIFO, failed update keeps state, not scope-owned | 001 / Phase 0 | recommended | Phase 1, Phase 3, Phase 5 |
| D-16 | Send future in sender scope; on cancel: queued → withdrawn, running → commits; future cancelled (resolves H3) | 001 / Phase 0 | adopted (Q2) | Phase 3, Phase 5 |
| D-17 | Streams: rest is a future, pull-driven, `:ahead` bound (1), owned by creator scope | 001 / Phase 0 | recommended | Phase 1, Phase 4, Phase 5 |
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
