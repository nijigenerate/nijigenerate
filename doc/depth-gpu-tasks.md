# DepthBone GPU Offset Calculation Tasks

This task list implements `doc/depth-gpu.md`.

## Phase 1: Preserve The Existing Contract

- [x] Identify all current call sites that expect `Vec2Array` offsets from `generateDepthBoneOffsets()`.
- [x] Document the current write targets:
  - [x] `deformable.deformation = offsets`;
  - [x] `deformable.notifyChange(...)`;
  - [x] `DeformationParameterBinding.update(kp, offsets)`;
  - [x] `ParameterBindingAddAction`;
  - [x] `ParameterChangeBindingsValueAction`;
  - [x] `depthBoneRefreshActionSink`.
- [x] Add regression coverage for preview writeback.
- [x] Add regression coverage for parameter binding writeback.
- [x] Add regression coverage for undo/redo behavior around generated deform bindings.

## Phase 2: Split CPU Reference From Runtime Packet Construction

- [x] Rename or wrap the current CPU calculation as the CPU reference path.
- [x] Extract reusable runtime construction from CPU offset generation.
- [x] Build a packet structure for one target/keypoint job.
- [x] Include target vertices in the packet.
- [x] Include scaled per-vertex depths in the packet.
- [x] Include `targetToRoot` and `rootToTarget` in the packet.
- [x] Include runtime bone skin matrices in the packet.
- [x] Include source settings and influence rule data in the packet.
- [x] Include target/parameter/keypoint routing data for writeback.
- [x] Add packet construction tests for GridDeformer.
- [x] Add packet construction tests for PathDeformer.

## Phase 3: Add macOS OpenGL 4.1 Capability Gate

- [x] Add a capability query for transform feedback support.
- [x] Add a capability query for texture buffer support.
- [x] Add a capability query for sync/fence support available under OpenGL 4.1.
- [x] Add diagnostics for unsupported required features.
- [x] Ensure the required path does not use compute shaders.
- [x] Ensure the required path does not use SSBOs.
- [x] Ensure the required path does not use image load/store.
- [x] Ensure the required path does not use OpenGL 4.2+ APIs.
- [x] Ensure unsupported capability does not silently run the old CPU refresh path.

## Phase 4: Implement GPU Resource Layout

- [x] Define the vertex/depth input buffer layout.
- [x] Define the runtime bone texture buffer layout.
- [x] Define the source settings texture buffer layout.
- [x] Define uniform locations or uniform block layout for transforms and rule values.
- [x] Define the transform feedback output buffer layout as `vec2 offset`.
- [x] Add explicit limits:
  - [x] maximum source bones per job;
  - [x] maximum retained influences;
  - [x] maximum buffer sizes;
  - [x] supported falloff modes.
- [x] Add clear diagnostics for packet values that exceed GPU limits.

## Phase 5: Implement Transform Feedback Shader

- [x] Add an OpenGL 4.1-compatible vertex shader for DepthBone offset calculation.
- [x] Configure transform feedback varying for `vec2 offset`.
- [x] Implement rest point construction from vertex XY and scaled depth.
- [x] Implement target-local to root-space transform.
- [x] Implement source DepthBone iteration.
- [x] Implement projection, distance, radius, falloff, and score calculation.
- [x] Implement fixed-size top-influence selection.
- [x] Implement score normalization.
- [x] Implement weighted skin-matrix application.
- [x] Implement root-space to target-local transform.
- [x] Output `deformedLocal.xy - vertex.xy`.
- [x] Add shader compile/link diagnostics.
- [x] Add a static or regression check that the shader path does not require forbidden OpenGL features.

## Phase 6: Implement GPU Job Submission

- [x] Add a `DepthBoneGpuJob` type.
- [x] Add a GPU solver object owned by the main-thread OpenGL context.
- [x] Upload packet buffers for one target/keypoint job.
- [x] Bind transform feedback output buffer.
- [x] Enable rasterizer discard.
- [x] Issue `glBeginTransformFeedback`.
- [x] Issue `glDrawArrays(GL_POINTS, 0, vertexCount)`.
- [x] Issue `glEndTransformFeedback`.
- [x] Restore rasterizer discard state.
- [x] Insert a GL fence.
- [x] Track job state without blocking.

## Phase 7: Implement Non-Blocking Readback

- [x] Poll job fences with zero timeout.
- [x] Leave incomplete jobs pending.
- [x] Read back only completed transform feedback buffers.
- [x] Convert readback data to `Vec2Array`.
- [x] Validate result length equals `target.vertices.length`.
- [x] Validate target/keypoint routing before applying.
- [x] Add a ring of output buffers or equivalent protection against buffer reuse hazards.
- [x] Report readback failures instead of falling back silently.

## Phase 8: Integrate With `ngFlushDepthBoneDirty()`

- [x] Convert dirty requests into GPU jobs.
- [x] Preserve request merging and superseding behavior.
- [x] Keep OpenGL submit/poll/readback on the main thread.
- [x] Add a per-frame job submission budget.
- [x] Add a per-frame readback/writeback budget.
- [x] Apply completed preview results to `deformable.deformation`.
- [x] Apply completed parameter results to `"deform"` bindings.
- [x] Preserve action sink behavior.
- [x] Keep pending GPU jobs across frames.
- [x] Ensure the normal GPU path does not call CPU offset generation.

## Phase 9: Replace All-Keypoints CPU Calculation

- [x] Expand all-keypoints requests into target/keypoint GPU jobs.
- [x] Submit all-keypoints GPU jobs over multiple frames.
- [x] Read back all-keypoints results over multiple frames.
- [x] Update `DeformationParameterBinding.update(kp, offsets)` for every completed keypoint.
- [x] Update `deformable.deformation` for the current visual keypoint.
- [x] Preserve existing grouping of undo/action changes.
- [x] Remove normal-path CPU offset chunks from all-keypoints refresh.
- [x] Add regression coverage that all-keypoints refresh avoids CPU offset generation in GPU mode.

## Phase 10: Update Preview And Apply Commands

- [x] Route `PreviewDepthBoneDeform` through GPU job submission.
- [x] Write preview results only to `deformable.deformation`.
- [x] Route `ApplyDepthBoneDeform` through GPU job submission.
- [x] Write apply results to parameter `"deform"` bindings.
- [x] Preserve existing undoable action behavior for apply.
- [x] Provide bounded synchronous completion only where command semantics require it.
- [x] Provide status/diagnostic output for unsupported GPU mode.

## Phase 11: Compatibility And Diagnostics

- [x] Keep CPU reference available for tests.
- [x] Add explicit compatibility mode if CPU fallback is intentionally supported.
- [x] Ensure compatibility mode is visible in diagnostics.
- [x] Ensure shader limit failures are reported.
- [x] Ensure GPU capability failures are reported.
- [x] Ensure readback failures are reported.
- [x] Avoid silent CPU recomputation in the normal GPU path.

## Phase 12: Validation

- [ ] Compare GPU and CPU reference offsets for GridDeformer within tolerance.
- [ ] Compare GPU and CPU reference offsets for PathDeformer within tolerance.
- [x] Test multiple source bones.
- [x] Test source `weight`.
- [x] Test source `depthScale`.
- [x] Test source `depthOffset`.
- [x] Test `maxInfluences`.
- [x] Test `minimumRadius`.
- [x] Test `radiusScale`.
- [x] Test linear falloff.
- [x] Test non-linear falloff.
- [x] Test lock-to-root terminal behavior.
- [x] Test preview-only refresh.
- [x] Test parameter keypoint refresh.
- [x] Test all-keypoints refresh.
- [x] Test undo/redo after GPU writeback.
- [x] Test save/load after GPU writeback.
- [x] Test unsupported OpenGL capability diagnostics.
- [x] Test that forbidden OpenGL features are not referenced by the required path.

## Completion Criteria

- [ ] The required GPU path runs on macOS OpenGL 4.1.
- [ ] Per-vertex DepthBone offset calculation is done by transform feedback in the normal GPU path.
- [ ] GPU output is read back as CPU `Vec2Array`.
- [ ] Preview writes the same CPU target as the old path.
- [ ] Parameter refresh writes the same CPU binding values as the old path.
- [ ] All-keypoints refresh no longer performs normal-path CPU offset calculation.
- [x] Undo/save/load behavior remains compatible with the existing CPU model.
- [x] Unsupported GPU capability is reported clearly.
- [x] Compute shaders, SSBOs, image load/store, and OpenGL 4.2+ APIs are not required.
