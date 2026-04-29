# MCP API Roles

This document defines the public MCP surface of `nijigenerate` in terms that a client can follow without reading server code.

## Core Split

- `resources` are for exploration and reading.
- `tools` are for actions and mutation.

If an endpoint can change app state or trigger side effects, it belongs under `tools`.
If an endpoint only returns current state, it belongs under `resources`.

## Explore / Read / Act

Use this flow consistently:

1. List or explore with `resources/list` or `resource://nijigenerate/resources/find?selector=...`
2. Read with `resource://nijigenerate/resources/{uuid}`
3. Act with `tools/call`

In `nijigenerate`, `resources/list` is the main state traversal entrypoint. It lists currently readable resource instances for the active puppet.
For broad discovery, the recommended first query is `resource://nijigenerate/resources/find?selector=*`.

## Resource Endpoints

- `resource://nijigenerate/resources/find?selector=...`
  - Explores current resource instances by selector.
  - The `selector` query parameter uses URL-encoded `nijigenerate.core.selector` syntax.
  - Returns a hierarchical tree with basic fields.
  - For broad discovery, start with `resource://nijigenerate/resources/find?selector=*`.
  - The response includes `guidePrompt: "selectors/guide"` and `usagePrompt: "resources/find"` so clients can discover the matching prompts.
- `resource://nijigenerate/resources/{uuid}`
  - Reads one resource instance by UUID.
  - Nodes include richer serialized state than parameters.

## Discovery Endpoints

- `resources/list`
  - Lists currently readable resource instances for the active puppet.
  - Includes concrete UUID-based URIs and the selector exploration endpoint.
- `resources/templates/list`
  - Lists parameterized read definitions such as `resource://nijigenerate/resources/{uuid}`.
  - This is a capability-discovery endpoint.

## Prompt Endpoints

- `selectors/guide`
  - Describes selector syntax and how hierarchy is returned from `resources/find`.
- `resources/find`
  - Describes how to call `resource://nijigenerate/resources/find?selector=...`
  - Recommends `selector=*` as the first-pass discovery query.
  - Links clients back to `selectors/guide` for the selector language itself and `resources/guide` for the workflow.

## Tool Endpoints

- `tools/list`
  - Lists action definitions.
- `tools/call`
  - Executes an action.
  - Inputs may reference resources by UUID, but the tool remains an action endpoint, not a read endpoint.

## Tool Context

Tool calls may include an optional `context` object. If omitted or `null`, the active app state is used.

- `context.nodes`: Node UUID array.
- `context.parameters`: Parameter UUID array.
- `context.armedParameters`: Parameter UUID array used as the armed parameter context.
- `context.parameterValue`: Parameter-axis values, `[x]` for 1D parameters or `[x, y]` for 2D parameters.

`parameterValue` is resolved against the first armed parameter, or the first parameter if no armed parameter is supplied. Values must exactly match existing key values. MCP clients must not pass key point indexes; `keyPoint` is an internal command-context detail, not part of the MCP input surface. The deprecated `paramValue` spelling is accepted only as a compatibility alias.

## Deform Binding Tools

- `ModelCommand_SetDeformBinding`
  - Sets raw `deform` binding offsets for the current parameter key position.
  - `values` is a flattened `[dx0, dy0, dx1, dy1, ...]` array and must match the target vertex count.
- `ModelCommand_SetTRSBinding`
  - Sets `deform` binding offsets by applying local translation, rotation, and scale to each target's vertices.
  - `translation` is `[x, y]` and defaults to `[0, 0]`.
  - `scale` is `[x, y]` and defaults to `[1, 1]`.
  - `rotationDegrees` rotates counter-clockwise and defaults to `0`.
  - `pivot` is optional `[x, y]`; if omitted, each target uses the center of its local vertex bounds.
  - Use `context.parameterValue` to choose the parameter key position.

## Naming Rules

- Resource descriptions should start with `Read:` or `Explore:` where practical.
- Tool descriptions should start with `Action:`.
- Do not describe a resource endpoint as a tool.
- Do not use `list` alone for both definition lists and instance lists without qualification.
