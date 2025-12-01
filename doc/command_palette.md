# Command Palette Result Model Revision

This document outlines how to evolve the command palette so that commands return structured results instead of `void`.

## Goals
- Make every command report success/failure explicitly.
- Let typed commands (`ExCommand!(T...)`) return a typed payload alongside the success flag.
- Standardize the result shape for commands that create or delete resources so the palette (and MCP) can surface the affected resource.

## Proposed Types
- `CommandResult`
  - `bool succeeded;`
  - Optional: `string message;` (for human-facing details; can stay empty for now).
- `ExCommandResult!(T)` (for commands deriving from `ExCommand!(T...)`)
  - `bool succeeded;`
  - `T result;` (or a small struct wrapping multiple fields if the command already has multiple outputs).
- Resource-oriented results  
  - `CreateResult!(R)` — `bool succeeded; R[] created; string message;`
  - `DeleteResult!(R)` — `bool succeeded; R[] deleted; string message;`
  - `LoadResult!(R)`   — `bool succeeded; R[] loaded;  string message;`

## Interface Changes
- `interface Command` → `CommandResult run(Context context);`
- `abstract class ExCommand!(T...)` → overrides `run` and returns `ExCommandResult!(Payload)` where `Payload` is the per-command return type.
  - For commands that do not need to return data, use `CommandResult` directly with `succeeded=true/false`.
  - For commands that already convey information via dialogs, the dialog remains but `succeeded` must still be set accurately.

## Resource-Oriented Commands
- Creation commands: return `CreateResult!(R)` with `created` populated.
- Deletion commands: return `DeleteResult!(R)` with `deleted` populated (before disposal so callers can reference it).
- Load commands: return `LoadResult!(R)` with `loaded` populated.
- These results let the palette or MCP server broadcast resource additions/removals without re-querying state.

## Migration Plan
1) Add the new result structs and enums in `commands/base.d` (or a nearby shared module).
2) Change `Command.run` to return `CommandResult`; update `ExCommand` signature accordingly.
3) Incrementally migrate commands:
   - Simple commands: return `CommandResult(succeeded: <bool>)`.
   - Typed commands: define a per-command payload type and return `ExCommandResult!(Payload)`.
   - Resource commands: return `CreateResult!` / `DeleteResult!` / `LoadResult!` as above.
4) Update command dispatchers (palette, shortcuts, MCP) to check `succeeded` and optionally surface `message` or resource info.
5) Add minimal tests for representative commands (simple, typed, resource create/delete) to validate the new contract.

## Notes
- Keep the API backward-compatible where feasible by providing helper constructors or `alias`es during the migration (e.g., `alias VoidResult = CommandResult;`).
- Avoid breaking call sites abruptly; migrate core dispatchers first so existing commands can start returning richer results without crashing older code paths.
