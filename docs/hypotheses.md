# Hypotheses (H-n)

Design tensions tracked until confirmed, refuted or transferred. New ones continue from H8.
In each feature, open hypotheses that touch it become `[NEEDS CLARIFICATION]` items in
`spec.md` or research items in `research.md`.

| ID | Hypothesis | Examine in | State |
|----|-----------|------------|-------|
| H1 | Is the blocking Phase 1 prototype reused as the base of the VM or discarded? Reader, persistent structures and macro expander look reusable; the threading model probably not. Which C module boundary avoids rework? | 002 (Phase 1) | open |
| H2 | The GC root discipline (shadow stack, handles, conservative) contaminates all runtime C code and is expensive to retrofit. Must it be decided in Phase 1? Which minimal convention must the prototype follow? | 002 (Phase 1) | open |
| H3 | "Futures do not outlive their scope" vs agents (long-lived, with identity) and the future returned by `send`. What is that future's scope? Can an agent be accessed from different scopes without violating the rule? | 001 (Phases 0–1) | **resolved** (D-15, D-16): the send future belongs to the sender scope; agents are outside the scope tree |
| H4 | Content addressing vs closures capturing identity values (futures, agents, scopes). Does the function hash cover the environment? How are mutual recursion and cycles hashed? (Phase 1 decides what spec/prototype anticipate; full protocol in 5c.) | 001, revisited in 5c | **partly resolved** (D-06, D-19): the hash excludes the env, cycles are hashed per SCC, hashing happens after macro expansion; fetch, cache and reload transferred to 5c |
| H5 | "Effectively once via idempotence" (5d) requires the agent owner to store applied operation ids. For how long? Relation to GC and restarts? | Phase 5 (5d) | open |
| H6 | Promise pipelining (5a) vs structured cancellation (5b): does cancelling a scope cancel in-flight chained calls on other nodes? | Phase 5 (5a/5b) | open |
| H7 | macOS arm64 vs Linux arm64 (no `<threads.h>`, unnamed semaphores or barriers on macOS; kqueue vs epoll; no thread affinity; ucontext deprecated; JIT restrictions). Minimal portability layer from Phase 1? Linux x86_64 is now a CI-only target (D-28): what does the portability layer need for two architectures (atomics, alignment, page size, `musttail` and computed goto on x86_64), and how are CI-only failures reproduced without a local x86_64 machine? | 002 (Phase 1) | open |
| H8 | Deadlock detection (D-14) needs a wait-for structure. Can a work-stealing VM keep it cheap (for example, checking only on the slow suspension path)? | Phase 3 | open |
| H9 | Primary error by completion order (D-11) is nondeterministic. Is a deterministic order needed for test oracles or replay? | Phase 1, Phase 7 | open |
| H10 | Two hashes per structure (D-03). Cache both, or compute the content hash only on demand? | Phase 2 (spike) | open |
| H11 | Streams owned by the creator scope (D-17). Is handing a live stream to a sibling scope a common need? | Phase 1 (programs 6, 7, 10) | open |
| H12 | A fixed reduction tree (D-18) vs work-stealing balance on irregular fork/join. Is the loss acceptable? | Phase 3 | open |
