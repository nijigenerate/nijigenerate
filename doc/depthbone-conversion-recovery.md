# DepthBone target conversion and refresh recovery

Converting a GridDeformer to a plain Node used to preserve its UUID in the
DepthRigRoot binding list. A pending all-keypoint refresh then tried to build a
GPU packet for that non-deformable node and terminated the editor.

Node replacement now records DepthRig binding changes in the same undoable
operation. Grid/Path replacements keep their sources and update the target kind;
other replacements remove the target binding. Undo restores the original binding,
including source settings and influence multipliers. Unrelated targets are kept.

The editor frame loop uses `ngFlushDepthBoneDirtyForFrame()`. If a refresh raises
an Exception, pending producers and GPU work are canceled, owning asynchronous
actions are marked failed, and a notification displays the error. The next frame
does not retry the discarded work. The primary edit remains undoable. This does
not roll back updates that completed before the failure.

`ngFlushDepthBoneDirty()` and `ngFlushDepthBoneDirtyImmediate()` still propagate
errors to explicit command callers after cleanup, so a failed command cannot be
reported as successful. Runtime Errors are not swallowed by the frame wrapper.

Regression coverage:

- `depthbone.cleanup`: conversion with a pending refresh, unaffected target
  updates, deletion, and undo/redo of the complete rig binding state.
- `depthbone.gpu-all-keypoints`: keypoint and all-keypoint failures, action
  settlement, error notification, absence of partial-batch writeback, and a
  successful fresh refresh after the invalid reference is removed.
