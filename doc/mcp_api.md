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
- `resource://nijigenerate/bindings/get?parameter=<parameter-uuid>&target=<target-uuid>&name=<binding-name>`
  - Reads one parameter binding by stable descriptor.
  - Returns the binding target, interpolation mode, set count, and serialized `data`.
  - `axisValues` maps `data.values[x][y]` indexes to parameter-axis values.
  - `data.values` contains the binding value grid. For `deform` bindings this is the deformation offset data.
  - Use Binding URIs returned by `resources/list` or `resources/find`; do not use Binding pseudo-UUIDs as input.

## Discovery Endpoints

- `resources/list`
  - Lists currently readable resource instances for the active puppet.
  - Includes concrete UUID-based URIs, Binding descriptor URIs, and the selector exploration endpoint.
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
- `context.bindings`: Binding descriptor array. Each item is `{ "target": <Node-or-Parameter UUID>, "name": "<binding name>" }` and requires `context.parameters[0]`.

`parameterValue` is the primary MCP input for parameter key selection. It is stored as a parameter-axis value and each command resolves it against the command's actual target parameter after that parameter is selected. Values must exactly match existing key values. MCP clients must not pass key point indexes; `keyPoint` is an internal command-context detail, not part of the MCP input surface. The deprecated `paramValue` spelling is accepted only as a compatibility alias.

`bindings` identifies parameter bindings structurally instead of using Binding resource pseudo-UUIDs. A Binding is resolved from `context.parameters[0]`, the target resource UUID, and the binding name. This is the stable MCP input form for commands such as `BindingCommand_RemoveBinding` and `BindingCommand_SetInterpolation`.

The same descriptor is used for reading binding data as a resource:

```text
resource://nijigenerate/bindings/get?parameter=123&target=456&name=deform
```

## Deform Binding Tools

- `ModelCommand_SetDeformBinding`
  - Sets raw `deform` binding offsets for the current parameter key position.
  - `values` is a flattened `[dx0, dy0, dx1, dy1, ...]` array and must match the target vertex count.
- `ModelCommand_SetTRSBinding`
  - Sets node transform `ValueParameterBinding`s, not `deform`.
  - `translation` writes `transform.t.x` and `transform.t.y`.
  - `scale` writes `transform.s.x` and `transform.s.y`.
  - `rotationDegrees` writes `transform.r.z` in radians after degree conversion.
  - `applyRotation=true` is required to write an explicit zero rotation.
  - Use `context.parameterValue` to choose the parameter key position.

## Binding Selection Tools

- `BindingCommand_RemoveBinding`
  - Removes only the bindings specified by `context.bindings` from `context.parameters[0]`.
  - Example: `{ "context": { "parameters": [123], "bindings": [{ "target": 456, "name": "deform" }] } }`.
- `BindingCommand_SetInterpolation`
  - Changes interpolation only for the bindings specified by `context.bindings`.
  - Binding resource pseudo-UUIDs are inspection-only and must not be used as tool input.

## Naming Rules

- Resource descriptions should start with `Read:` or `Explore:` where practical.
- Tool descriptions should start with `Action:`.
- Do not describe a resource endpoint as a tool.
- Do not use `list` alone for both definition lists and instance lists without qualification.
