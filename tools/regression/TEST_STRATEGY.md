# Regression Test Strategy

The regression suite must prove runtime behavior, not inflate scenario counts.

## Scenario Types

- `automated`: headless runtime test. It must execute the feature path and assert state, undo/redo, serialization, rendering data, or command output.
- `computer-use`: GUI runtime test. It must launch a prepared scenario, perform explicit input actions, assert observable post-action state, and preserve an artifact.
- `Regression Audit`: inventory or source audit only. It must never count as runtime behavior or branch audit.

## Required Runtime Contract

Every runtime scenario must have:

1. Setup: deterministic model, mode, panel/window, fixture, or command context.
2. Action: concrete command, key, click, drag, text input, file operation, or UI gesture.
3. Assertion: post-action state, history boundary, resource list, file output, render data, visible UI state, or process exit.
4. Artifact: test output, log, saved file, screenshot, serialized data, or captured command result when the behavior is external.

## Coverage Rules

- A scenario row without an executable runner is not audit.
- A scenario that only starts the app is not computer-use audit.
- A source scan is an audit, not runtime coverage or branch audit.
- A feature family is incomplete until each user-facing input class has a runtime scenario or a computer-use scenario with an explicit interaction recipe.
- `--require-all` must fail while any runtime scenario is only `computer-use` or `pending`.

## Computer Use Rules

Computer-use scenarios must:

- Have a manifest entry.
- Have an explicit driver recipe.
- Execute keyboard or pointer input.
- Assert the app is still visible after input.
- Require process exit success.
- Write a per-scenario artifact log.

The driver must reject unknown or recipe-less scenarios.
