# Command Palette Result Model Migration Tasks

Status legend: `[ ]` todo, `[>]` in progress, `[x]` done, `[?]` blocked.

## Core type and interface changes
- [x] Add `CommandResult`, `ExCommandResult!(T)`, and `ResourceResult!(R)` (with `ResourceChange` enum) to `source/nijigenerate/commands/base.d`.
- [x] Update `Command.run` to return `CommandResult`; update `ExCommand!(T...)` to return `ExCommandResult!(Payload)` and adjust constructors/helpers for backward compatibility.

## Dispatcher updates
- [>] Palette/shortcut dispatcher consumes `CommandResult.succeeded` (and message if provided) instead of void; ensure UI handles failures gracefully (palette/shortcut updated; MCP updated to return JSON status/payload).
- [x] MCP server command execution surfaces `succeeded/message` and, when available, resource info from `ResourceResult` (payload encoded to JSON).
- [x] Extract MCP arg/context handling into `api/mcp/helpers.d` and simplify server dispatch to use the helpers.

## Command migrations
- [x] Simple commands: convert to return `CommandResult` with proper success flag (broad pass done).
- [x] Typed commands (`ExCommand` derivatives): define per-command payload types and return `ExCommandResult!(Payload)` (broad pass done).
- [>] Resource commands (create/delete/load): apply `ResourceResult!(R)` (parameters, param groups, bindings, deform set, puppet load/import converted; node/other resources pending).

## Testing and verification
- [ ] Add representative unit/integration tests for: a simple command, a typed command, and resource create/delete commands to validate new return contracts.
- [ ] Smoke-test palette and MCP flows to ensure callers handle the new return types without regressions.

## Migration hygiene
- [ ] Provide temporary aliases/helpers for backward compatibility during rollout.
- [ ] Document any behavior changes for callers (e.g., dialogs still show messages, but `succeeded` must be set accurately).
