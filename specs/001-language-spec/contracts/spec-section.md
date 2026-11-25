# Contract: specification section format (`docs/spec/NN-topic.md`)

Every section file MUST follow this skeleton. The checks in [quickstart.md](../quickstart.md)
search for the bold markers below.

```markdown
# NN — <Topic>

**Question:** <the single question this section answers>

**Status:** Normative | Partially open | Open — closed in Phase <N>

## Answer

**Decision:** D-xx[, D-yy]   <!-- at least one, unless Status is Open -->

<normative text using MUST / MUST NOT; one concept per paragraph>

## Examples

```quux
;; each example shows input and the observable result
(= #{1 2 3} #{3 2 1})   ;=> true
```

## Errors

<error kinds this topic can raise, with the condition for each (link to 12-errors.md)>

## Open points

- <question> — **Open — closed in Phase <N>** (H-n if tracked)

## Used by programs

<list of programs/NN-… that exercise this section>
```

Rules:

- Write only what the 12 programs need, or what the inherited decisions require. Anything
  else goes under "Open points" with the phase that closes it.
- Examples use `;=>` for a value result and `;!! :kind` for a raised error.
- Do not use durations or performance claims. Delays appear only as values injected into
  examples.
