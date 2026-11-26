# Contract: Ergonomics Program Layout

Decisions D-55, D-58 and D-59.

## Directory

```text
programs/ergonomics/NN-short-name/
├── main.qx                    # required
├── input.txt                  # optional: quux data, passed as the runner's INPUT argument
├── expected-output.txt        # required: exact standard output
├── expected-exit-status.txt   # optional: "0" or "1" followed by a newline; absent means 0
└── README.md                  # required
```

## `main.qx` rules

1. **Names.**
   - The program uses only names approved in `docs/naming.md`, plus names it defines itself.
     There are no reader shorthands.
   - `quux-name-check` must pass.
   - Local helpers and stubs have full-word kebab-case names.
2. **Entry point.**
   - `main` takes one argument, `input`, and is defined in the namespace that is current at the
     end of the file.
   - Its return value is printed by the runner, so it is part of the expected output.
3. **Determinism.**
   - Output does not depend on time, addresses or hash order. Maps and sets print in canonical
     order.
   - Each program prints the size of the data it processes (D-63).
4. **Comments.** The comments explain intent, as for a reader learning quux. They do **not**
   describe friction; friction goes in the README.
5. **Stubs (pending programs only).** Each missing primitive is one function, written like this:

   ```text
   (define-function split-into-words [text]
     (raise (error :user "missing: split text on whitespace"
                   {:needed "a core function that splits a string into a list of strings"})))
   ```

   The stub's message starts with `missing:`. The program calls the stub exactly where it would
   call the real function.

## `input.txt`

- The file holds one quux form, written as text. The test driver passes its contents unchanged as
  the second command-line argument.
- A pending raw-text program (03, 06) receives its text as **one quux string**. In the runnable
  partner program (02, 05), the same data arrives already split: a vector of strings or of rows.

## `README.md` sections (in this order)

1. `# NN — Title`
2. **Area**: the FR-001 area number and name. For a pair, the variant and the partner, for example
   "variant: prepared input; partner: 03".
3. **What it evaluates**: a bullet list of the core features exercised, with spec section
   references.
4. **How to run**: `quux programs/ergonomics/NN-short-name/main.qx` (and the input argument if
   any).
5. **Expected result**: a short description of what is printed, the returned value and the exit
   status. The exact text is in `expected-output.txt`.
6. **Status**:
   - one of `runnable`, `pending — missing: <stub name> (<what>)` (one per stub), or
     `pending — bug: <description>`;
   - a line `Verifier: every allocation` (the default) or `Verifier: amortized — <reason>`, which
     selects the mode of the verified test (contracts/test-driver.md).
7. **Friction observed**: a table with these columns:

   | ID | Location | What was awkward | Workaround | Origin | Review |
   |---|---|---|---|---|---|
   | P04-F1 | `insert` | … | … | writer | unreviewed |

   - IDs are `PNN-Fk`.
   - `Origin` is `writer` or `owner`.
   - `Review` is `unreviewed`, `kept`, `disputed: <reason>` or `withdrawn`.
8. **Mistakes made while writing** (optional): the `M-NN` IDs of mistakes made while writing this
   program, recorded in the report.

## Index `programs/ergonomics/README.md`

The index has one table row per program, with these columns: number, directory, area, variant and
partner, status, and number of friction notes. It also includes a one-paragraph note: these
programs are not part of the phase acceptance suite, which is the 12 reference programs.
