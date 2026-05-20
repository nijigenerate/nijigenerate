# Depth Bone Auto Refresh Plan

## Goal

Depth Bone rigging should automatically refresh all bound deformers when a relevant Depth Bone, binding, target depth, or target mesh setting changes.

The refresh must not run when nothing has changed. It must run at most once for the same changed rig state in one update cycle.

## Core Design

Use a dirty request queue.

Commands and hooks that actually change Depth Bone related state must only mark a refresh request. They must not directly recalculate every target immediately.

The application update loop flushes the dirty queue once after normal UI/command processing. If the queue is empty, no refresh work is done.

```text
No change:
  ngFlushDepthBoneDirty()
  -> dirty queue is empty
  -> return immediately

Relevant change:
  command/hook calls ngMarkDepthBoneDirty(root, param, keypoint, reason, scope)
  ngFlushDepthBoneDirty()
  -> consume unique dirty requests
  -> recalculate all bindings for each dirty root/parameter/scope
```

## Dirty Key

Dirty requests are deduplicated by:

```text
DepthRigRoot uuid
Parameter uuid
Scope
Keypoint coordinate for Keypoint scope
```

There are two scopes:

```text
Keypoint     : update only the current parameter keypoint
AllKeypoints : update every keypoint of the parameter
```

`Keypoint` is only for direct Bone pose edits in Deform mode. `AllKeypoints` is for changes to shared generation inputs: depths/depth-ops, target vertices, target/root transform, Bone Source list/order/settings, Influence Rule, Bone rest, constraint, and `allowParentToTargets`.

This means repeated changes to the same rig/keypoint or rig/parameter during one update cycle still produce only one refresh. If `AllKeypoints` exists for the same rig/parameter, narrower `Keypoint` requests are absorbed.

The refresh target is the full set of `DepthRigRoot.bindings` for that root. The system should not try to partially update only one target at first, because dependency can spread through:

- source Bone list
- Bone hierarchy
- `allowParentToTargets`
- distance based influence scoring
- source depth offset/scale
- target mesh and depth bounds

## API

Add a small auto-refresh module, preferably under:

```text
source/nijigenerate/commands/depth/autorefresh.d
```

Required API:

```d
void ngMarkDepthBoneDirty(
    ExDepthRigRoot root,
    Parameter parameter,
    vec2u keypoint,
    string reason,
    DepthBoneDirtyScope scope = DepthBoneDirtyScope.Keypoint
);

void ngMarkDepthBoneDirtyForArmedParameter(
    ExDepthRigRoot root,
    string reason
);

void ngFlushDepthBoneDirty();
```

`ngMarkDepthBoneDirtyForArmedParameter` reads the active armed parameter. If no armed parameter exists, it does nothing.

`ngFlushDepthBoneDirty` returns immediately when there are no dirty requests.

## Refresh Operation

When a dirty request is flushed:

1. Find every binding in the target `DepthRigRoot`.
2. Resolve each binding target node.
3. Skip invalid or missing targets.
4. Recalculate offsets with `generateDepthBoneOffsets(root, &binding, target, parameter, keypoint)`.
5. Create or reuse the target's `deform` parameter binding.
6. Write recalculated offsets to the requested keypoint for `Keypoint`, or to every `Parameter.axisPoints` combination for `AllKeypoints`.
7. Push one grouped action for the refresh.

For `AllKeypoints`, `deformable.deformation` used for immediate viewport display is set from the currently active closest keypoint, while every keypoint binding is updated.

The refresh action should be separate from the configuration action in the first implementation. If later needed, command actions and auto-refresh actions can be merged into a single group, but correctness of one refresh per dirty cycle comes first.

## Change Sources

The following places should mark dirty.

### Binding And Source Changes

- `AddDepthBoneSourceCommand`
- `RemoveDepthBoneSourceCommand`
- Source reorder operation in GridDeformer inspector
- Source reorder operation in PathDeformer inspector
- `SetDepthBoneSourceSettingsCommand`

### Influence Rule Changes

- `SetDepthBoneInfluenceRuleCommand`

### Bone Definition Changes

- `SetDepthBoneRestCommand`
- `SetDepthBoneConstraintCommand`
- Bone node transform changes that already call the Depth Bone auto-apply hook

### Target Data Changes

- Depth map apply from Depth Edit mode
- Target vertices change
- Target transform change
- DepthRigRoot transform change

Target data hooks can be added after the first pass if no existing central hook is available. The first pass must cover all explicit Depth Bone commands and existing Bone transform hooks.

## UI Drag Behavior

For UI `DragFloat` values, there are two acceptable levels.

First pass:

- Let the command run whenever ImGui reports a changed value.
- Each command marks dirty.
- The dirty queue ensures only one refresh for that update cycle.

Later refinement:

- Use `igIsItemDeactivatedAfterEdit()` for high-frequency drag controls.
- During dragging, keep only UI-local pending values.
- Commit one command when the edit is finished.

The first pass is sufficient for avoiding repeated recalculation inside the same update cycle.

## Armed Parameter Behavior

Auto-refresh only writes to a parameter when an armed parameter exists.

If no armed parameter exists and no previous parameter context is available:

- resolve Parameters that already have `deform` bindings for DepthRig targets
- run `AllKeypoints` refresh for those Parameters
- if no such Parameter exists, no deform binding is created and recalculation may run preview-only

Preview remains separate from auto-refresh.

## Undo Behavior

Initial behavior:

- The original configuration command pushes its own action.
- The refresh flush pushes one `Auto Refresh Depth Bone Deform` action.

This keeps the implementation simple and keeps recalculated deformation undoable.

Future behavior:

- Merge the configuration action and refresh action when both happen in the same update cycle.
- This is optional and should be implemented only after the dirty queue behavior is stable.

## Flush Location

Call `ngFlushDepthBoneDirty()` once near the end of the normal application update cycle, after UI panels and commands have run.

The flush location must satisfy:

- It runs once per frame/update.
- It runs after inspector commands can mark dirty.
- It does not run before active command processing finishes.
- It is cheap when there is no dirty request.

Candidate locations:

- after panel update
- at the end of project/app update

The final location should match existing update flow and avoid recursive command execution.
