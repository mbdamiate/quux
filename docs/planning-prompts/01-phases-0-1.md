# 01 — Phases 0 and 1 (run now)

Paste `00-base.md` first and, right after it, the block below. This module has no previous state: it is the starting point and defines the decisions that all the others inherit.

```text
<module_task>
Turn Phases 0 and 1 of the outline into a detailed plan that one person can execute alone, in spare time. This module covers only these two phases. However, prepare the decisions that the following phases (especially Phase 5, distribution) will inherit.
</module_task>

<hypotheses_to_examine>
I noticed the points below in the context. Treat them as HYPOTHESES: confirm or refute each one with an argument, propose how to resolve it, indicate in which phase the decision gets closed, and add others you find. Number the new ones starting from H8.
- H1: Will the blocking prototype of Phase 1 be reused as the basis of the VM or discarded? The persistent structures, the reader and the macro expander are reusable; the threading model, probably not. Which module boundary in C avoids rework?
- H2: In C, the GC root discipline (shadow stack, handles, conservative GC) contaminates all the runtime code and is expensive to retrofit. Does it need to be decided already in Phase 1, even if the real GC only arrives in Phase 3? Which minimal convention must the prototype follow?
- H3: "Futures do not outlive their scope" versus agents (long-lived, with identity) and the future returned by send. What is the scope of that future? Can an agent be accessed from different scopes without violating the rule?
- H4: Content addressing versus closures that capture values with identity (futures, agents, scopes). Does the function's hash cover the environment? How should mutual recursion and cycles be handled in the hash computation? (Here it is enough to decide what the specification and the prototype must anticipate; the full protocol is left for Phase 5.)
- H7: Development is on macOS (arm64, 16 KiB pages, weak memory model) and there is a second target, Linux arm64 in Docker. What changes for the runtime between the two (absence of <threads.h>, of unnamed semaphores and of barriers on macOS; kqueue vs epoll; no thread affinity; ucontext deprecated on macOS, which affects stackful coroutines; JIT restrictions) and which minimal portability layer in C should exist from Phase 1? What risk remains from x86_64 not being a target?
Hypotheses H5 and H6 belong to Phase 5 and are not treated here; just record them as open in the closing.
</hypotheses_to_examine>

<specific_content>
- Phase 0: skeleton of the specification (sections and the question each one answers); for each of the 12 programs, what it tests in the design; how to write 2 or 3 of them in an alternative version with actors, for side-by-side comparison; and the decisions that affect distribution: function identity (by content or not), which values are serializable and how to mark those that are not (files, sockets, local resources), and the place of network errors in the error model.
- Phase 1: the first task is a smoke test of the base toolchain: compile a minimal program in each preset, on macOS and in the Docker Ubuntu container, and confirm that ASan detects a deliberate heap overflow, that TSan detects a deliberate race, and that LSan/valgrind in the container detects a deliberate leak (recording whether Docker capabilities had to be adjusted). Then: C project structure (modules, internal API conventions, portability layer, testing policy); Dockerfile and scripts for building and testing on Linux; minimal set of special forms and primitives; minimal root and allocation convention so as not to block the future GC (H2); open design questions, with your recommendation for each: cancellation semantics (cooperative or forced, at which points), the fate of a future whose scope has ended, whether and when futures escape scopes, propagation with several failing children, agent semantics (ordering, errors, reading the current state), error model (including timeouts and unreachable nodes), identity and serialization of scopes, equality and hash of functions and futures, modules compatible with content addressing, macro hygiene and numerics. Describe how to simulate several nodes in one process, with injection of failures and delays, to validate programs 11 and 12 without a real network.
- In both phases, when closing a decision that Phases 3 and 5 will inherit (error model, cancellation semantics, scope identity, serialization), mark it in the log as "inherited by: Phase 3" or "inherited by: Phase 5".
</specific_content>

<delivery_protocol>
Deliver in two parts, in Markdown. At the end of Part A, stop and wait for me to write "continue". Do not compress Part B to fit it into Part A.
- Part A: verification of hypotheses H1, H2, H3, H4 and H7 (and the new ones you find), clarifying questions that block the plan (at most 5; if I do not answer, proceed with the most reasonable hypothesis and record it), executive summary of what this module delivers, and Phase 0 following the template.
- Part B: Phase 1 following the template, and the module closing (decision log, open hypotheses, state summary), per <module_closing>.
</delivery_protocol>
```
