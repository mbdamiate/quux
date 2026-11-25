# 02 — Phase 2: dependency gate and toolchain validation

Paste `00-base.md` first and, right after it, the block below. Run this module **after** finishing the Phase 1 prototype, because the library research must be done at the moment of the decision, not months earlier.

Fill in the two input fields (`<previous_state>` and `<prototype_results>`) and the reference date in the executor profile.

```text
<input>
<previous_state>
[FILL IN: paste here the decision log, the open hypotheses and the state summary produced by module 01]
</previous_state>

<prototype_results>
[FILL IN: what the prototype showed. Include: smoke test result (what worked or not under ASan, TSan, LSan, valgrind and Docker, and which adjustments were needed); which prototype components survived and which were discarded; where you felt pain (allocation, hashing of persistent nodes, errors, concurrency); which libraries you have already used or tested; the C standard and the compilers you actually used]
</prototype_results>

<plan_divergences>
[FILL IN: what changed relative to the plan of module 01. Write "none" if nothing changed]
</plan_divergences>
</input>

<module_task>
Turn Phase 2 into a detailed plan that one person can execute alone, in spare time, starting from the reality recorded in <input> and not from the original plan. This module covers only Phase 2. Start with the web research (see <mandatory_checks>) and only then write the matrix and the spikes.
</module_task>

<mandatory_checks>
Before writing, research and note with link and date (the reference date is the one in the executor profile):
- For each need in the matrix, the candidate libraries in C that you know of and others that the search reveals: latest version, date of the last commit or release, maintenance status, license and support for macOS arm64 and Linux arm64.
- The current major of LLVM and clang in Homebrew and the availability of clang and LLVM in the repositories of the chosen Ubuntu LTS, to decide the toolchain version alignment.
- The current state of LeakSanitizer and ThreadSanitizer on macOS with Apple Silicon and inside Docker on Ubuntu arm64.
- Multi-thread support of the candidate garbage collectors (including those that can be embedded in C), and whether any of them serves the shared-heap model of the decisions already made.
- Maturity of the QUIC, binary serialization, cryptographic hash and node-to-node authentication libraries available in C.
Whatever you cannot verify, label as [MEMORY].
</mandatory_checks>

<specific_content>
- Phase 2: decision gate for C dependencies and validation of the base toolchain. For each need (persistent structures, multi-threaded GC for shared heap, work-stealing scheduler, lexer/parser, REPL, asynchronous I/O with kqueue and epoll, TCP/QUIC transport, binary serialization, cryptographic hash for content addressing, node-to-node authentication, LLVM via C API), build a matrix with the decision adopt library / write / postpone, suggested criteria and weights (maturity, maintenance, license, thread-safety, macOS and Linux portability, integration cost, lock-in risk, ease of testing) and which prototype data (<prototype_results>) feeds each criterion. List candidate libraries in C with maintenance status and relevant limitations, relying on the research. Propose 3 to 5 spikes (for example, minimal multi-threaded GC, work-stealing deque, hash and serialization of persistent nodes, echo over the chosen transport), each with limited scope and an acceptance criterion. Revise the base toolchain with the smoke test data (C standard, compilers, sanitizers, Docker, macOS and Linux portability policy) and say what would change it. Say which evidence would make you change an adopt-or-write decision.
- For the "LLVM via C API" item of the matrix, it is enough to decide whether the dependency enters now or is postponed and which major version to pin; the detailed evaluation of the API is left for module 05.
- Revisit the hypotheses H1 to H7 that <previous_state> left open and that touch this phase.
</specific_content>

<delivery_protocol>
Deliver in a single part, in Markdown: the summarized research (with links), Phase 2 following the template (with the complete matrix and the spikes) and the module closing, per <module_closing>.
</delivery_protocol>
```
