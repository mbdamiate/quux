# quux Language Specification — Index

quux is a Lisp dialect implemented in C. It is immutable by default, uses canonical persistent
collections, guarantees proper tail calls, and builds concurrency on futures inside structured
scopes, with agents, streams and deterministic data parallelism. Distribution is a future goal;
the decisions that it depends on are fixed here.

This specification is **normative by example**. Each section answers one question, cites the
decisions (`D-xx`) that settle it, and shows examples. The decisions are recorded in
[docs/decisions.md](../decisions.md), with full records in
[specs/001-language-spec/research.md](../../specs/001-language-spec/research.md). Every public
name comes from the approved glossary [docs/naming.md](../naming.md) (constitution Principle
VIII).

## Sections

| # | File | Question |
|---|---|---|
| 01 | [01-values-and-equality.md](./01-values-and-equality.md) | What values exist, and when are two values equal? |
| 02 | [02-collections.md](./02-collections.md) | Which collections exist, and what does "canonical" guarantee? |
| 03 | [03-evaluation-and-tail-calls.md](./03-evaluation-and-tail-calls.md) | How are expressions evaluated, and which calls never grow the stack? |
| 04 | [04-macros.md](./04-macros.md) | How are macros written and expanded, and how do they relate to code identity? |
| 05 | [05-numerics.md](./05-numerics.md) | Which numbers exist, and what happens on overflow and inexact division? |
| 06 | [06-futures.md](./06-futures.md) | What is a future, which states can it be in, and what does waiting for it do? |
| 07 | [07-scopes-and-cancellation.md](./07-scopes-and-cancellation.md) | How do scopes bound futures, and how does cancellation work? |
| 08 | [08-combinators.md](./08-combinators.md) | How are futures combined, and how do combinators fail? |
| 09 | [09-agents.md](./09-agents.md) | How does state that changes over time work? |
| 10 | [10-streams.md](./10-streams.md) | How are asynchronous sequences consumed with backpressure? |
| 11 | [11-data-parallelism.md](./11-data-parallelism.md) | How is parallel work kept deterministic? |
| 12 | [12-errors.md](./12-errors.md) | What is an error, and which kinds exist? |
| 13 | [13-serialization-and-identity.md](./13-serialization-and-identity.md) | Which values can leave a node, how is a function identified, and how are remote things referenced? |
| 14 | [14-modules.md](./14-modules.md) | How are names organized and bound when code is identified by content? |

Supporting documents:

- [appendix-input-output.md](./appendix-input-output.md): the input and output primitives used
  by programs 6 and 10. It is not one of the 14 topics.
- [open-questions.md](./open-questions.md): the register of open questions and hypotheses.
- [_section-template.md](./_section-template.md): the format every section follows.

## Conventions

- **Section format**: Question, Status, Answer with `**Decision:**`, Examples, Errors, Open
  points, Used by programs. See
  [contracts/spec-section.md](../../specs/001-language-spec/contracts/spec-section.md).
- **Examples**: `;=>` shows the value of an expression; `;!! :kind` shows that it raises an error
  of that kind.

  ```quux
  (increment 41)              ;=> 42
  (increment 9223372036854775807)   ;!! :overflow
  ```

- **Normative words**: MUST, MUST NOT, MAY, as in RFC 2119.
- **Open points**: written as `**Open — closed in Phase N**`, with a hypothesis `H-n` when one is
  tracked in [docs/hypotheses.md](../hypotheses.md).
- **No reader shorthands**: there is no `'x`, `` `x ``, `~x`, `~@x` or `@x`. Quoting and waiting
  are always written as forms (naming rules R5 and R6).
- **Delays in examples** are injected values that make ordering observable. They are never
  performance claims.

## Reference programs

The 12 reference programs in [programs/](../../programs/) are the acceptance suite of every phase.
Each section lists the programs that exercise it.
