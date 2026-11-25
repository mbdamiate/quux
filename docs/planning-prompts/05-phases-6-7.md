# 05 — Phases 6 and 7: IR, LLVM backend and tooling

Paste `00-base.md` first and, right after it, the block below. Run this module close to reaching Phase 6, because the evaluation of the LLVM C API depends on versions and on the current state of the project.

```text
<input>
<previous_state>
[FILL IN: paste here the decision log, the open hypotheses and the state summary produced by module 04 (or 03, if you skipped distribution)]
</previous_state>

<plan_divergences>
[FILL IN: what changed relative to the previous plan. Write "none" if nothing changed]
</plan_divergences>
</input>

<module_task>
Turn Phases 6 and 7 into a detailed plan that one person can execute alone, in spare time, starting from the reality recorded in <input>. This module covers only these two phases. Start with the web research (see <mandatory_checks>) and only then design the IR.
</module_task>

<mandatory_checks>
Before writing, research and note with link and date:
- The current LLVM major, what the C API covers today (IR construction, optimization pipeline, object emission, JIT via ORC) and what exists only in the C++ API.
- LLVM support for garbage collection and safepoints (statepoints) and the state of its use in practice.
- Guarantee of tail calls in generated code (musttail) on the arm64 targets of macOS and Linux.
- LLVM coroutine intrinsics and the possibility of using them from the C API.
- Stability policy of the C API across versions and which majors are supported.
- Availability of LLVM in Homebrew and in the repositories of the Ubuntu LTS in use, and the cost of packaging libLLVM (static or dynamic, size, build time).
- JIT restrictions on macOS on Apple Silicon (MAP_JIT, write protection, code signing).
Whatever you cannot verify, label as [MEMORY].
</mandatory_checks>

<specific_content>
- Phase 6: IR design (SSA, CPS or ANF; representation of suspension points, cancellation, remote calls and safepoints), planned optimizations, and strategies for suspending tasks in native code (own state machines, LLVM coroutine intrinsics, separate stacks with context switching, or native only in functions without suspension), with the trade-offs of each for the GC. The backend is generated through the LLVM C API. Evaluate, based on the research: which parts of the C API cover IR construction, the optimization pipeline, object emission and the JIT (ORC), and which features exist only in the C++ API; LLVM support for garbage collection and safepoints (statepoints) and how this compares to an own stack map; guarantee of tail calls in generated code, for native TCO; stability of the C API across LLVM versions, supported versions and alignment with the pinned clang major; cost of linking libLLVM (static or dynamic, size, build time, packaging, including in the Ubuntu container) versus the lower-coupling alternative of generating textual IR or bitcode and invoking clang or llc as an external process. Consider the JIT restrictions on macOS on Apple Silicon when comparing AOT and JIT. Explain how native code coexists with content addressing (the hash identifies the function; native code is a derived, node-local artifact). Include, among the initial tasks, a minimal spike of the LLVM C API that generates and executes a simple function, to validate the integration before the full design.
- Phase 7: tooling (REPL, debugger for tasks, scopes and nodes, tests, network-failure simulation, packages), following the template. Prioritize what would make the development of the earlier phases easier, and indicate which tools already exist in embryonic form in what has been built.
- Revisit the hypotheses H1 to H7 that <previous_state> left open and that touch these phases.
</specific_content>

<delivery_protocol>
Deliver in a single part, in Markdown: the summarized research (with links), Phase 6 and Phase 7 following the template, and the module closing, per <module_closing>. If the response gets too long, stop after Phase 6 and wait for me to write "continue" before Phase 7.
</delivery_protocol>
```
