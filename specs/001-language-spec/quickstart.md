# Quickstart: validating feature 001 (language specification)

This feature has no runtime. Validation combines structural checks, which can be run as shell
commands, with one review pass. Run every command from the repository root.

## 1. Sections are complete (SC-002)

```bash
ls docs/spec/0[1-9]-*.md docs/spec/1[0-4]-*.md | wc -l   # expect 14
```

```bash
for f in docs/spec/0[1-9]-*.md docs/spec/1[0-4]-*.md; do grep -q '^\*\*Question:\*\*' "$f" || echo "MISSING Question: $f"; done
```

```bash
for f in docs/spec/0[1-9]-*.md docs/spec/1[0-4]-*.md; do grep -Eq '\*\*Decision:\*\* D-[0-9]+|Open — closed in Phase' "$f" || echo "NO DECISION/OPEN: $f"; done
```

Expected: 14 topic sections (`00-index.md` is the index and is not counted), and the loops
print nothing.

## 2. Programs are complete (SC-001)

```bash
for d in programs/[01][0-9]-*/; do for f in main.qx conditions.qx expected.qx README.md; do [ -f "$d$f" ] || echo "MISSING $d$f"; done; done
```

```bash
ls -d programs/[01][0-9]-*/ | wc -l            # expect 12
```

```bash
ls programs/04-*/actors.qx programs/10-*/actors.qx programs/12-*/actors.qx
```

Expected: there are 12 directories, nothing is reported missing, and three actor files exist.

## 3. Every assertion uses the contract forms

```bash
grep -ohE '^\s*\[?\((\w[-\w?]*)' programs/*/expected.qx | sed -E 's/.*\(//' | sort -u
```

Expected: only names from [contracts/reference-program.md](./contracts/reference-program.md).

## 4. Decisions and hypotheses are logged (SC-003, SC-004)

```bash
grep -cE '^\| D-[0-9]+' docs/decisions.md                      # expect 28
```

```bash
grep -E '^\| D-(07|09|10|11|13|14|19|20|21) ' docs/decisions.md   # Inherited-by column filled
```

```bash
grep -E '^\| H(3|4|8|9|10|11|12) ' docs/hypotheses.md
```

## 4b. Names are approved (Principle VIII)

```bash
grep -c '| proposed |' docs/naming.md        # expect 0 before writing docs/spec/ or programs/
```

## 5. Open-questions register (SC-003)

The register is `docs/spec/open-questions.md`. Check that it has an entry for each of these
items, and that each entry links to a `D-` or `H` record:

- cancellation;
- the scope-ended future;
- escape;
- multiple failures;
- agents;
- the error model;
- scope identity;
- function and future equality;
- modules;
- hygiene;
- numerics;
- deadlock;
- H3;
- H4.

## 6. Self-review pass (SC-007)

For each program, write the Phase 1 test outline in a scratch file using only `main.qx`,
`conditions.qx`, `expected.qx` and the referenced spec sections. Record every question
that comes up in `docs/spec/open-questions.md`, then answer it in the specification or mark it
with its closing phase. The pass ends when a full run over the 12 programs adds no new question.

## Stopping point

When steps 1–6 pass, Phase 0 is complete. The project can stop here with a coherent design
document, or continue with feature 002 (`/speckit-specify` input in
`docs/speckit-roadmap.md`).
