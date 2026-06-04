# Testing Policy

Regression tests must prove behavior, not only preserve test counts.

## Branch Coverage Policy

- Branch coverage means measured execution of every branch in production code by the test suite.
- Regression catalog audits, source inventories, and scenario ownership checks are not branch coverage.
- LDC `--cov` line-count reports are not sufficient branch coverage evidence.
- If branch coverage instrumentation is unavailable, the branch coverage command must fail instead of treating audits as a pass.

## Regression Audit Policy

- Every branch in production code represents a supported input, state, or failure variation and must have an owning regression scenario.
- Regression audits are accounting guards only. They must not replace behavioral tests or branch coverage measurement.
- Runnable placeholder tests, contract-only tests without behavior, and tests that only assert an id or catalog entry exists are not acceptable.
- When a bug is found in one feature path, check related paths proactively. A pattern that failed once is assumed to be capable of failing elsewhere.

## Input Combination Policy

- Tests must cover diverse combinations of valid inputs, boundary inputs, empty inputs, duplicate inputs, malformed inputs, missing resources, and stateful undo/redo sequences.
- Table-driven or generated matrix tests are preferred when a feature has modes, node types, parameter shapes, processors, settings, or UI-selectable variants.
- The goal is exhaustive combination coverage where feasible. When exhaustive coverage is impractical, the test must cover representative classes and document what remains manual or UI-only.

## UI Path Policy

- Automated headless tests are valid for model, command, parser, serialization, settings, and source-level invariants.
- A GUI/computer-use scenario is required when the risk is in the visible UI path: dialogs, menus, panels, pointer input, keyboard input, viewport rendering, timeline interactions, or visible state transitions.
- Stubbing a UI path is not a substitute for verifying that path. A stubbed test may support backend coverage, but it must not be counted as replacing a GUI-path obligation.

## Regression Catalog Policy

- The catalog must distinguish:
  - automated runtime tests that execute behavior,
  - computer-use scenarios that require visible UI verification,
  - regression-audit scenarios that guard test ownership,
  - pending work, which should remain zero for the committed catalog.
- Previous regression coverage must remain represented by real test implementations. Moving, splitting, or consolidating tests is acceptable only when each prior behavior is still executed somewhere traceable.

## Verification Policy

- Before accepting regression changes, run scenario accounting and the relevant automated tests.
- Before claiming branch coverage, run a real branch coverage command. If the current toolchain cannot produce branch data, do not claim branch coverage.
- For broad changes, run the full automated regression suite. If a test requires permissions, rerun with the required permission instead of treating the sandbox failure as a pass.
- Test changes must be reviewed against source behavior and the scenario catalog, not only against total counts.
