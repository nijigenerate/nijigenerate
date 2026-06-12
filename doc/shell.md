---
Title: Shell Panel (AI-generated)
Status: Draft
---

# Shell Panel

> This document is AI-generated, based on automated reading of the source code under `nijigenerate/source/nijigenerate/` (notably `panels/shell/shell.d`, `core/selector/*`, and `widgets/output.d`). Please review and amend as needed.

## Overview
The Shell Panel provides a command-driven, CSS-like query interface to browse and interact with project resources: Nodes, Parameters, and Bindings. It offers live preview while typing and records an execution history when pressing Enter. Results are shown as a tree and support in-place interactions such as selection, context menus, and drag-and-drop for reordering or reparenting nodes.

Primary goals:
- Fast, precise lookup of Nodes, Parameters, and Bindings
- Structural queries (descendants vs. direct children)
- Attribute-based filtering by name, uuid, and typeId
- Immediate interaction with results (select, inspect, reparent, reorder)

## When to use
- Quickly find a node/parameter/binding by exact name or UUID
- Inspect all descendants under a subtree
- Find all Parameters or Bindings associated with a Node
- Focus on active (armed) Bindings only
- Build repeatable queries you can run again from history

## Quick Start
1. Open a project and ensure a puppet/model is loaded (the panel requires an active puppet).
2. Open the Shell panel.
3. Click the input field at the top, type a command (e.g. `Node`).
4. Observe the live preview updating as you type.
5. Press Enter to record the result in history. Expand the newest entry (e.g. `Output [0]`) to inspect results.
6. Double‑click any result item to insert ` <TypeId>#<uuid>` back into the input. Right‑click items for context menus. Drag nodes to reorder/reparent.

## Command anatomy
The query language draws inspiration from CSS selectors.

- Types: `Node`, `Parameter`, `Binding`, `*`
- Selectors:
  - `.name` — exact name match; use quoted strings for special characters: `."Eye"`
  - `#uuid` — numeric UUID, e.g. `#123456`
- Attributes: `[attr=value]`
  - `[name="Foo"]`, `[uuid=123]`, `[typeId=Node]`
- Relationships:
  - `A B` — B is a descendant of A (any depth)
  - `A > B` — B is a direct child of A
- Pseudo-classes:
  - `Binding:active` — only bindings under the currently armed parameter
- Multiple queries: separate by comma `,`

Notes:
- Name comparisons are exact (no fuzzy matching).
- UUID must be numeric.
- Some queries require an active puppet to be loaded.

## Live preview and execution
- While typing, the panel shows a live preview of the current query result.
- Press Enter to run the query: the result is captured as an entry in the history, and the input is cleared.
- Each history entry is expandable to reveal its result tree.

## Result view interactions
Results are rendered as a tree (via `ListOutput` or `IconTreeOutput`) and support:
- Double-click an item to insert ` <TypeId>#<uuid>` back into the command input
- Right-click to open contextual actions (nodes, parameters, bindings)
- Drag-and-drop nodes to reorder or reparent (uses built-in history-enabled move operations)
- Selecting nodes/parameters from the tree integrates with global selection

Behavior hints:
- Root or `Part` nodes may show thumbnails; regular nodes show icons and names
- Parameter items provide inline edit/view controls
- Binding items indicate whether the binding is active at the current parameter point

## Command examples with expected output
Below, commands are shown in code blocks, followed by a concise description of the output you should see in the panel.

```text
Node
```
Output:
- A tree of all nodes. The puppet root appears at the top; matching nodes are listed under their parents.
- Some ancestor items may show dimmed text (added for context).

Supported types you can query directly:
`Part`, `Composite`, `DynamicComposite`, `Mask`, `MeshGroup`, `PathDeformer`, `SimplePhysics`, `Camera`

```text
Part."Eye"
```
Output:
- All `Part` nodes with the exact name `Eye`.
- Ancestors appear (dimmed) to provide hierarchy context. Right‑click for node actions.

```text
Node."Head" > Part
```
Output:
- Direct children of type `Part` under the node named `Head` (one level deep).

```text
Node."Head" > Part
```
Output:
- Direct children of type `Part` under the node named `Head` (one level deep).

```text
Node."Head" Part
```
Output:
- All descendant `Part` nodes (any depth) under `Head`.

```text
Node."Head" Part
```
Output:
- All descendant `Part` nodes (any depth) under `Head`.

```text
Node#123456 > *
```
Output:
- Everything directly under the node whose UUID is `123456`, regardless of type.
- Use this to quickly inspect a known node by numeric ID.

```text
Node."Head" > Parameter
```
Output:
- Parameters associated with the node `Head`.
- Parameter entries show inline controls for quick inspection.

```text
Node."Head" > Binding
```
Output:
- All bindings affecting the node `Head`.
- Binding entries indicate whether they are currently active at the current parameter point.

```text
Node."Head" > Binding:active
```
Output:
- Only bindings tied to the currently armed parameter for `Head`.
- Use this to focus on what is active right now.

```text
Parameter."MouthOpen"
```
Output:
- The parameter named `MouthOpen`.
- Double‑click to inject its selector into the input for chaining.

```text
Parameter."MouthOpen" > Binding
```
Output:
- All bindings under the parameter `MouthOpen`.
- Right‑click to open binding‑related actions.

```text
Part."EyeL", Part."EyeR"
```
Output:
- Results for both queries are returned together. Useful to compare or batch‑select symmetrical parts.

```text
Parameter[name="日本語の文字列"]
```
Output:
- Parameters whose name exactly matches `日本語の文字列`.
- Use quotes for non‑ASCII or spaced names.

## Tips
- Use quotes for names containing spaces, punctuation, or non-ASCII characters: `."日本語"`
- Double-click from results to quickly build longer, precise commands
- Use commas to fan-out broader searches in one go

## Troubleshooting
- No results shown while typing:
  - Ensure a puppet/model is loaded (Shell requires an active puppet).
  - Try a broad command first (e.g. `Node`) to verify the panel is working.
  - Use exact names and quotes for special characters (e.g. `Part."Eye"`).
- History not updating:
  - Press Enter after typing a command to capture it in the history list.
- UUID filters failing:
  - `#uuid` must be numeric (e.g. `#123456`).
- I see dimmed items:
  - Dimmed entries are ancestors automatically added to give you hierarchy context; matching items are the non-dimmed ones.

## Limitations
- Exact match only; no fuzzy search or regex
- `#uuid` accepts only numeric values
- Requires an active puppet for most resource queries

## Source reference
- `nijigenerate/panels/shell/shell.d`
- `nijigenerate/core/selector/{tokenizer.d, parser.d, query.d, resource.d, treestore.d}`
- `nijigenerate/widgets/output.d`

---
Generated by AI.
