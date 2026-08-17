# DepthBone GPU Offset Calculation Design

## Purpose

DepthBone refresh currently calculates DepthBone-driven GridDeformer and PathDeformer offsets on the CPU. The expensive
part is the per-vertex 3D calculation in `generateDepthBoneOffsets()`: each target vertex is lifted into 3D with depth
as Z, transformed by DepthBone skinning, then projected back to target-local XY offsets.

This design moves only that per-vertex 3D offset calculation to the GPU. The result is still read back as a CPU
`Vec2Array` and written to the same model state used today.

## Required Platform

The required implementation target is macOS OpenGL.

macOS OpenGL is limited to OpenGL 4.1. Therefore the required path must not depend on APIs newer than OpenGL 4.1.

Allowed required path:

- OpenGL 4.1 transform feedback.
- Vertex shader based calculation.
- Vertex attributes, uniforms, uniform blocks, texture buffers, and other OpenGL 4.1-compatible input paths.
- OpenGL 4.1-compatible buffer readback.

Forbidden required path:

- Compute shaders.
- SSBOs.
- Image load/store.
- OpenGL 4.2+ APIs.
- Vulkan, Metal, DirectX, CUDA, or newer OpenGL as the required implementation.

CPU fallback may exist for compatibility or diagnostics, but the normal GPU feature path must be macOS OpenGL 4.1
compatible.

## Current Threading Model

`ngFlushDepthBoneDirty()` is called from the main application loop. The existing all-keypoints refresh is not a worker
thread. It is a main-thread queue that processes a small amount of CPU work each frame.

The GPU implementation should keep OpenGL calls on the main thread. The main thread submits GPU work, polls completion
later, and applies completed readback results. It must not block on GPU completion in the same frame unless an explicit
synchronous command requires it.

## Existing CPU Contract

The observable behavior to preserve is:

1. Dirty DepthBone changes are queued by `ngMarkDepthBoneDirty()`.
2. `ngFlushDepthBoneDirty()` consumes dirty requests.
3. For each affected target/keypoint, offsets are produced in target vertex order.
4. Preview writes `deformable.deformation = offsets`.
5. Parameter refresh writes `DeformationParameterBinding.update(kp, offsets)`.
6. The same undo/action sink behavior is preserved.
7. Save/load/export continue to observe CPU model state, not temporary GPU buffers.

The GPU result must be shape-compatible with the current CPU result:

- `offsets.length == target.vertices.length`;
- one `vec2` per target vertex;
- index order matches `target.vertices`;
- values are target-local XY offsets;
- the array can be assigned to `deformable.deformation`;
- the array can be passed to `DeformationParameterBinding.update(kp, offsets)`.

## CPU/GPU Responsibility Split

Keep these on CPU:

- Dirty request collection and merge.
- Target, root, binding, parameter, and keypoint resolution.
- DepthBone runtime pose construction where it is not per-target-vertex heavy.
- Packet construction for GPU input.
- Undo/action construction.
- CPU model writeback.
- Save/load/export model state.

Move this to GPU:

- Per-target-vertex conversion from `(x, y, depth)` to a 3D rest point.
- Per-target-vertex source DepthBone influence evaluation.
- Per-target-vertex weighted skinning.
- Per-target-vertex conversion back to target-local XY offset.

## Data Flow

```text
DepthBone / target / depth change
  -> ngMarkDepthBoneDirty(...)
  -> ngFlushDepthBoneDirty()
  -> create GPU offset job from affected root/target/parameter/keypoint
  -> upload target vertices, scaled depths, transforms, runtime bones, source settings
  -> run transform feedback vertex shader
  -> store offset.xy into transform feedback buffer
  -> insert GL fence
  -> later frame: poll fence without blocking
  -> read back completed offsets into Vec2Array
  -> write deformable.deformation and/or parameter deform binding
```

## Transform Feedback Backend

The required backend is OpenGL 4.1 transform feedback.

One vertex shader invocation calculates one output offset.

Shader algorithm:

1. Read target vertex XY and scaled depth.
2. Build `restLocal = vec3(x, y, depth)`.
3. Transform to root space with `targetToRoot`.
4. Iterate source DepthBones.
5. Calculate projection, distance, radius, falloff, source weight, and score.
6. Keep the strongest `MAX_INFLUENCES` candidates in fixed-size shader-local arrays.
7. Normalize candidate scores.
8. Apply each candidate runtime bone `skinMatrix`.
9. Blend deformed root-space positions.
10. Transform the blended point back with `rootToTarget`.
11. Output `deformedLocal.xy - vertex.xy`.

The transform feedback varying is the final `vec2 offset`.

## GPU Input Packet

Each GPU job represents one target/keypoint calculation.

Required packet data:

- target identity for writeback;
- parameter/keypoint identity for writeback, if a parameter binding must be updated;
- target vertex XY array;
- scaled depth array;
- `targetToRoot`;
- `rootToTarget`;
- source DepthBone count;
- runtime bone data:
  - rest head;
  - rest tail;
  - world head;
  - world tail;
  - rest length;
  - parent index;
  - skin matrix;
  - lock-to-root and terminal flags needed by influence logic;
- source settings:
  - runtime bone index;
  - weight;
  - depth scale;
  - depth offset;
  - multiplier;
- influence rule:
  - max influences;
  - radius scale;
  - minimum radius;
  - falloff mode.

Texture buffers are the preferred way to pass variable-sized runtime arrays under OpenGL 4.1. Fixed-size shader limits
must be explicit. Unsupported counts must return a clear unsupported diagnostic or use an explicit CPU compatibility
path; they must not be silently truncated.

## Scheduling

`ngFlushDepthBoneDirty()` should become a scheduler for DepthBone GPU jobs:

1. Convert new dirty requests into GPU jobs.
2. Submit jobs that are ready and within the per-frame submission budget.
3. Poll existing jobs with `glClientWaitSync(..., timeout = 0)`.
4. Read back only completed jobs.
5. Apply completed CPU writebacks.
6. Keep incomplete jobs pending.

The main loop remains responsive because GPU completion is polled in later frames.

## Readback

Immediate blocking readback is not acceptable for the normal asynchronous refresh path.

Avoid:

```text
submit -> glFinish -> readback -> apply
```

Use:

```text
frame N:
  submit transform feedback
  insert fence

frame N+1 or later:
  poll fence with zero timeout
  if complete, read back offsets and apply
  if not complete, keep pending
```

Use a small ring of output buffers so the CPU does not read a buffer the GPU may still be writing and the GPU does not
overwrite a buffer whose result has not been applied.

## All-Keypoints Refresh

The existing all-keypoints queue splits CPU calculation across frames. In GPU mode, the expensive calculation is moved
to GPU jobs.

The all-keypoints path should:

1. Expand the affected parameter into keypoints.
2. Create one GPU job per affected target/keypoint.
3. Submit and read back jobs over multiple frames.
4. Update `DeformationParameterBinding.update(kp, offsets)` for each completed job.
5. Also update `deformable.deformation` when the completed job matches the current visual keypoint.

Per-frame limits should apply to GPU job submission, readback, and CPU writeback, not to CPU offset calculation.

## Preview And Apply Commands

Preview command:

- submit GPU offset jobs for requested targets;
- read back completed offsets;
- write only `deformable.deformation`;
- do not create undoable parameter binding changes.

Apply command:

- submit GPU offset jobs for requested targets;
- read back offsets;
- create or reuse `"deform"` bindings;
- call `DeformationParameterBinding.update(kp, offsets)`;
- create the same undoable actions as the CPU path.

If a command requires synchronous completion, it may run a bounded wait loop with UI/status feedback. The normal frame
refresh path should remain non-blocking.

## CPU Reference And Compatibility

The current CPU calculation should remain as a reference implementation during development and testing.

Allowed CPU uses:

- CPU/GPU parity tests.
- Diagnostics.
- Explicit compatibility path when GPU requirements are not met.

Disallowed normal GPU path behavior:

- silently using CPU offset generation because GPU work is pending;
- silently using CPU offset generation because a shader limit was exceeded;
- recomputing offsets on CPU after readback failure without reporting the failure.

## Validation

Required validation:

- GPU output matches CPU reference within tolerance for GridDeformer.
- GPU output matches CPU reference within tolerance for PathDeformer.
- Preview writes GPU offsets to `deformable.deformation`.
- Parameter refresh writes GPU offsets to `"deform"` binding values.
- All-keypoints refresh completes without normal-path CPU offset calculation.
- Undo/redo preserves existing action semantics.
- Save/load observes CPU model state after writeback.
- The required backend uses only macOS OpenGL 4.1-compatible APIs.
- The required backend does not reference compute shaders, SSBOs, image load/store, or OpenGL 4.2+ APIs.
