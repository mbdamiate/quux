# Contract: `docs/ergonomics-report.md`

Decisions D-61, D-62 and D-64. The entity fields are defined in [data-model.md](../data-model.md).

## Sections (in this order)

1. **Method**:
   - the programs and their status;
   - how the expected outputs were derived (D-60);
   - the review checkpoint: its date, and how many notes the owner added, disputed and withdrew;
   - the counts per class and per recommendation.
2. **Friction entries**: one subsection per entry, headed `### E-NN — <short title>`, with these
   fields:
   - **Sources**: the `PNN-Fk` notes and, where it applies, the `M-NN` mistakes.
   - **Class**: one of the five classes.
   - **Observed**: what happened, with a minimal quux excerpt.
   - **Recommendation**: add, rename, document, or keep as is.
   - **Rationale**: each claim labeled `[FACT: source]`, `[OPINION]` or `[ESTIMATE]`. Comparisons
     with Clojure, Scheme or Racket link their documentation (D-64).
   - **Record**: the W-xx or D-xx entry, or the spec section changed by a "document"
     recommendation.
   - **Decision item**: the `K-NN` item, for add and rename.
3. **Mistakes log**: a table with the columns `M-NN`, code, kind and message, pointed to the cause
   (yes, partly or no), program, and load time.
4. **Bugs found**: for each bug, the program, the symptom, the fix commit and its regression test.
   A bug that was not fixed instead names the feature 002 task created for it.
5. **Performance**: a table with each program's time in the `release` preset on macOS arm64, its
   data size, and a flag for anything over 1 second or for `:stack-exhausted` (D-63).
6. **Owner decision list**: at most 15 items, each headed `### K-NN — <proposal>`, with:
   - the proposal, its signature and one example;
   - the friction removed (`E-NN` entries);
   - the programs affected;
   - the alternatives considered (always including "keep as is");
   - **Owner answer**, which is empty until the owner answers.

## Coverage rules (checked before the report is final)

- Every friction note whose review status is `kept` or `disputed` appears in exactly one entry.
  Every entry has at least one source.
- Every entry with recommendation add or rename has a W-xx entry and a `K-NN` item. Every proposed
  name in it has a section K row with status `proposed`.
- No proposed name appears in any program, specification section or source file.
