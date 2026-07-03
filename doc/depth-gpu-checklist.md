# DepthBone GPU Compatibility Checklist

Baseline: `d844870bf5875b5d0c592c338c37e11e0aa3fddc`.

Rule: the GPU implementation replaces only the offset calculation work that the
CPU+Worker path performed. Triggers, dirty queue semantics, target resolution,
CPU-side writeback destinations, action grouping, and undo behavior keep the
same meaning as the baseline.

## Summary

Current result: all checked items match the baseline semantics.

The implementation now keeps the CPU+Worker refresh path shape and swaps the
calculation body for GPU dispatch plus CPU readback:

- Dirty triggers and dirty queue entry points are the same as the baseline.
- `ngFlushDepthBoneDirty()` is called from the same update-loop points.
- If no GPU backend is available, dirty work remains pending instead of being
  consumed as an empty result.
- Once a GPU backend is available, the same refresh functions write readback
  results into the same CPU-side buffers and actions that the baseline used.
- The removed render-hook-only direct writeback path is gone.
- Preview and Apply commands use the same current deformation and binding
  action paths as the baseline.
- GPU shader influence projection now matches the CPU helper split:
  `pointSegmentProjection()` is unclamped, while segment distance clamps the
  projection internally.
- GPU shader influence accumulation rejects NaN scores with the same fallback
  meaning as the CPU finite-score guard.
- GPU source terminal flags are computed from the binding's source bones, not
  from every bone under the rig root.
- GPU source `depthOffset` is stored in the same scaled world-depth unit that
  the CPU path used after `depthScaleFor(target)`.

## Checklist

| # | Baseline CPU+Worker behavior | Current workspace behavior | Match | Evidence |
|---|---|---|---|---|
| 1 | `incUpdate()` and `incUpdateNoEv()` call `ngFlushDepthBoneDirty()` at the end of the update loop. | Same calls are restored. | Yes | `source/app.d` imports and calls `ngFlushDepthBoneDirty()` in both update loops. |
| 2 | `ngFlushDepthBoneDirty()` processes dirty requests without losing work. | If a GPU backend is not available, dirty requests remain pending; they are processed when the backend is available. | Yes | `ngFlushDepthBoneDirty()` returns before consuming requests when `hasDepthBoneGpuBackend()` is false. |
| 3 | PSD depth import wraps depth changes and refresh actions with `ngBeginDepthBoneRefreshActionSink(group)`. | Same action sink wrapping is used. | Yes | `source/nijigenerate/commands/depth/map.d` begins and ends the refresh action sink around import changes. |
| 4 | `PsdDepthImportRefreshJob.step()` calls `ngFlushDepthBoneDirty()`. | Same flush call is used. | Yes | `PsdDepthImportRefreshJob.step()` calls `ngFlushDepthBoneDirty()`. |
| 5 | PSD import pushes the `GroupAction` only after associated refresh work completes. | The job waits on pending refresh work for the same sink, including backend-waiting dirty requests. | Yes | `ngPendingDepthBoneRefreshWorkForSink(group)` counts dirty requests and all-keypoint jobs for the sink. |
| 6 | `ngDepthBoneBindingValueChangeHook` is registered to `ngDepthBoneBindingValueChanged()`. | Same hook is registered. | Yes | `shared static this()` assigns the hook. |
| 7 | DepthBone transform binding changes mark the related rig dirty. | Same dirty mark is performed. | Yes | `ngDepthBoneBindingValueChanged()` calls the same dirty mark path. |
| 8 | `ngMarkDepthBoneDirtyForArmedParameter()` uses the armed parameter and falls back to the last dirty parameter only when armed is null. | Same parameter selection is restored. | Yes | No extra root-affects filter remains in this function. |
| 9 | `ngMarkDepthBoneDirtyForTarget()` does not drop the armed parameter through extra root-affects filtering. | Same target dirty behavior is restored. | Yes | The extra armed-parameter filter was removed. |
| 10 | `depthBoneAffectedParameters()` includes the armed parameter unconditionally. | Same parameter collection is restored. | Yes | The armed parameter is appended without extra filtering. |
| 11 | `ngRefreshDepthBoneDeform()` updates `Deformable.deformation` and parameter deform bindings in the same refresh path. | Same path is used; only `generateDepthBoneOffsets()` uses GPU dispatch/readback. | Yes | The function writes `deformable.deformation`, creates/uses deform bindings, and pushes the same action path. |
| 12 | `ngRefreshDepthBoneDeform()` creates `ParameterChangeBindingsValueAction` and pushes it through `pushDepthBoneRefreshAction()`. | Same action construction is used. | Yes | Action creation and `pushDepthBoneRefreshAction(group)` are present. |
| 13 | `ngRefreshDepthBoneDeformKeypoints()` computes and records every requested keypoint. | Same all-keypoint loop and action construction are used. | Yes | The function loops over `keypoints` and writes each keypoint binding value. |
| 14 | `ngRefreshDepthBoneDeformAllKeypoints(param=null)` returns false when no affected parameter is found. | Same behavior is restored. | Yes | The `params.length == 0` path returns `false`. |
| 15 | `ngFlushDepthBoneDirtyImmediate()` loops over `ngFlushDepthBoneDirty()` until pending work is drained. | Same loop is used, with an explicit error if GPU backend is unavailable. | Yes | The loop remains; `enforce(hasDepthBoneGpuBackend())` prevents silent empty results or infinite spinning. |
| 16 | `DeformationViewport.paramValueChanged()` only synchronizes the editor. | Added dirty trigger is removed. | Yes | No call to the removed parameter-value dirty helper remains. |
| 17 | `incViewportNodeDeformNotifyParamValueChanged()` only calls `view.paramValueChanged()`. | Added dirty trigger is removed. | Yes | No extra DepthBone dirty call remains in viewport model deform code. |
| 18 | Parameter panel right-click calls `incViewportNodeDeformNotifyParamValueChanged()` when the controller parameter is armed. | Same UI synchronization is restored. | Yes | The parameter panel calls the viewport notification before refreshing bindings. |
| 19 | `ngCheckDepthBoneFingerprints()` exists, but baseline `ngFlushDepthBoneDirty()` does not call it. | Same: fingerprint helper exists, and dirty flush does not call it. | Yes | `ngFlushDepthBoneDirty()` starts from pending dirty requests, matching the baseline. |
| 20 | Preview command immediately writes offsets into `Deformable.deformation`. | Same current deformation writeback is used after GPU readback. | Yes | `PreviewDepthBoneDeformCommand` assigns `deformable.deformation = offsets`. |
| 21 | Apply command writes deform bindings and pushes the same undoable action structure. | Same binding and action path is used after GPU readback. | Yes | `ApplyDepthBoneDeformCommand` creates/updates `DeformationParameterBinding` and pushes `ParameterChangeBindingsValueAction`. |
| 22 | CPU+Worker work writes results back to the same CPU-side buffers and actions. | GPU readback feeds the same refresh/action paths; separate direct writeback queue is removed. | Yes | No `pendingDepthBoneGpuDispatches`, `applyDepthBoneReadback()`, or `refreshDepthBoneGpuTarget()` path remains. |
| 23 | Render hook does not own target resolution or a separate binding writeback path. | Render hook only supplies/caches a GPU backend and invokes the existing dirty flush path. | Yes | `ngDepthBoneBeforeRenderPlayback()` does not update bindings directly. |
| 24 | Unsupported or unavailable GPU must not silently produce a different result. | Work remains pending until a backend exists; immediate flush fails explicitly if no backend is available. | Yes | Backend absence no longer produces empty offsets as a valid result. |
| 25 | CPU terminal-lock selection uses unclamped parent-segment projection and checks `terminalProjection > 1.0`. | GLSL projection is now unclamped; only distance-to-segment clamps internally. | Yes | All three DepthBone GPU shader variants use the same projection semantics. |
| 26 | CPU terminal-source detection uses the binding source bone list, not all rig bones. | GPU packet source terminal flag is computed from `sourceBones`. | Yes | Regression covers a non-source descendant bone and keeps the source terminal flag enabled. |
| 27 | CPU source depth uses `(rawDepth * depthScale + depthOffset) * depthScaleFor(target)`. | GPU packet stores base depth as `rawDepth * depthScaleFor(target)` and source offset as `depthOffset * depthScaleFor(target)`, so shader `z * depthScale + depthOffset` is in the same unit. | Yes | Regression checks the packed source depth offset is scaled. |
| 28 | CPU influence accumulation skips non-finite scores before adding to `total`. | GLSL uses `!(score > 0.0)` and `!(total > eps)`, which rejects NaN without relying on optional GLSL finite helpers. | Yes | Regression CPU reference and all three GPU shader variants use the same NaN-rejecting comparison form. |

## Verification Performed

- `dub build --config=win32-full`: passed.
- `dub build --config=regression-tests`: passed.
- `out/nijigenerate-regression-tests.exe --only depthbone.gpu-packet`: passed.
- `out/nijigenerate-regression-tests.exe --only depthbone.preview-commands`: passed.
- `out/nijigenerate-regression-tests.exe --only depthbone.fit-z`: passed when run singly. A parallel run hit the known process-exit `nijigenerate-agent.log` lock, then passed when rerun alone.
