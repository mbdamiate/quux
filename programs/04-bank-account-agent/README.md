# 04 — Bank account agent

**What this tests in the design:** ten concurrent withdrawals of 20 against a balance of 100.
`transact` runs the check and the debit in one update, and updates run one at a time, so exactly
five succeed and the balance is never negative. The refusals are replies, not errors.

**Specification sections:** 09 agents (`transact`, ordering), 08 combinators (`all`).

**Decisions:** D-15, D-16.

**Hypotheses informed:** none. An actor version is in `actors.qx` (User Story 4).
