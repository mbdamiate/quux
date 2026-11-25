# Language plan: modular prompts

This set splits the single planning prompt into a reusable **base block** and **six modules**, so you can run each stage when you reach it. The reason: library information ages (Phase 2 and Phase 6 depend on fresh research) and the Phase 1 prototype will invalidate assumptions that the later phases would inherit.

## How to use

1. Open a new conversation, with web search turned on.
2. Paste the content of the code block in `00-base.md`.
3. Right after, paste the content of the code block of the module of the day.
4. Fill in the `[FILL IN: ...]` fields (including the reference date in the executor profile).
5. At the end of the module, save the **decision log** and the **state summary** that the conversation produces. They are the input of the next module, in the `<previous_state>` field.

## Order and when to run

| Module | Covers | When to run | Input |
|---|---|---|---|
| `01-phases-0-1.md` | Specification, 12 programs, blocking prototype | **Now** | None |
| `02-phase-2.md` | Dependency gate, spikes and toolchain | When the prototype is ready | State from module 01, prototype results |
| `03-phases-3-4.md` | VM, scheduler, GC, asynchronous I/O, optional actors | When Phase 2 is closed | State from module 02, divergences |
| `04-phase-5.md` | Distribution (5a to 5f), in two parts | When the VM runs programs 1 to 10 | State from module 03, divergences |
| `05-phases-6-7.md` | IR, LLVM backend, tooling | Close to reaching Phase 6 | State from module 04, divergences |
| `06-consolidation.md` | Milestone roadmap, stopping points, risks, decision index | When you want to close the complete plan | States from all modules |

You can run `06` earlier, with only the modules you already have, to get a provisional view of the roadmap.

## Shared conventions

- **Decisions:** IDs `D-xx`, numbered continuously across modules (the previous state says where it stopped).
- **Tasks:** `T<phase>.<n>`, with relative size S, M or L and no duration estimate.
- **Hypotheses:** `H1` to `H7`, the design tensions that each module confirms, refutes or transfers. They were renamed (previously T1 to T7) so as not to collide with task IDs.
- **Confidence labels:** `[FACT: source]`, `[MEMORY]`, `[OPINION]`, `[ESTIMATE]` (only for probabilities and relative sizes).
- **No time estimates**, in any module.

## Plan divergences

From module 02 onward, there is a `<plan_divergences>` field. Use it to record what changed during implementation (a reverted decision, a library that did not work out, a phase that grew). The next module starts from reality, not from the original plan.

## Assumptions you can adjust

- Linux arm64 in Docker is the second target; x86_64 is out of scope.
- The Phase 1 prototype is in C, with pthreads.
- Each module can stop midway and wait for "continue" when the response gets long (`01` and `04` already ask for this).
