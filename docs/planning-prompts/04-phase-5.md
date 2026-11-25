# 04 — Phase 5: distribution (5a to 5f)

Paste `00-base.md` first and, right after it, the block below. This is the largest module and the one that depends most on the decisions made earlier (errors, scopes, serialization, filling futures by message). Run it when the VM already executes programs 1 to 10.

Delivery comes in two parts (5a to 5c and 5d to 5f), with a pause in the middle.

```text
<input>
<previous_state>
[FILL IN: paste here the decision log, the open hypotheses and the state summary produced by module 03]
</previous_state>

<plan_divergences>
[FILL IN: what changed relative to the previous plan (for example, an error or scope decision that changed while implementing the VM). Write "none" if nothing changed]
</plan_divergences>
</input>

<module_task>
Turn Phase 5 into a detailed plan that one person can execute alone, in spare time, starting from the reality recorded in <input>. This module covers only Phase 5, with one section per sub-stage (5a to 5f). Use web search to verify the current state of the libraries and protocols you cite (QUIC, serialization, hash, authentication, transport).
</module_task>

<hypotheses_to_examine>
Confirm, refute or transfer each hypothesis below, with an argument, and add others you find (number the new ones starting from H8, continuing the existing ones from <previous_state>).
- H4 (revisited): Content addressing versus closures that capture values with identity (futures, agents, scopes). What did Phase 1 decide about the environment hash and about mutual recursion and cycles? What still needs to be resolved for sub-stage 5c?
- H5: "Effectively once via idempotence" (5d) requires the agent's owner to store ids of operations already applied. For how long? How does this relate to GC and to restarts?
- H6: Promise pipelining (5a) and structured cancellation (5b) interact: does cancelling a scope cancel in-flight chained calls on other nodes?
</hypotheses_to_examine>

<specific_content>
For each sub-stage (5a to 5f), detail, beyond the template:
- protocol: message format; ids of nodes, tasks, scopes and agents; lifecycle of a remote future;
- failure semantics: what the program observes when a node goes down, the network partitions or a message is lost; when delivery is at-most-once, at-least-once or effectively-once via idempotence;
- distributed GC: leases versus distributed reference counting, and how much structured concurrency allows avoiding a general distributed collector;
- content addressing: computation of hashes of functions and dependencies, per-node code cache, protocol for fetching missing code, interaction with code reloading and with macros;
- transfer of persistent structures by node hash: when it pays off and when it is better to send the whole value;
- task placement: explicit choice first; criteria for a future automatic placement, with data locality as the priority;
- security: node-to-node authentication, restrictions on executing received code, and care with insecure deserialization in C;
- tests: injected failures and deterministic simulation, including nodes in Docker Ubuntu containers on a virtual network with injected network failures, and reuse of the Phase 1 node simulator.
When a topic does not apply to a sub-stage, say so in one line instead of repeating content from another. Make clear what must be avoided, especially hiding from the programmer that a call is remote (the fallacies of distributed computing).
Phase 5f is optional: evaluate, based on the reference programs and on what Phase 4 concluded about actors, whether it is justified, and say what would decide it.
</specific_content>

<delivery_protocol>
Deliver in two parts, in Markdown. At the end of Part A, stop and wait for me to write "continue".
- Part A: summarized research (with links), verification of hypotheses H4, H5 and H6, and sub-stages 5a, 5b and 5c.
- Part B: sub-stages 5d, 5e and 5f and the module closing, per <module_closing>.
</delivery_protocol>
```
