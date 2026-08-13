/*
    Copyright © 2026, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.model.depthboneoverlay;

import nijigenerate.commands.depth.bone : ngCachedDepthBoneSourceEffectivePivots,
    ngFlushDepthBoneEffectivePivotDirty, ngSetDepthBoneEffectivePivotSelection;
import nijigenerate.core.dbg;
import nijigenerate.ext.nodes.exdepthbone;
import nijigenerate.project : ngShowDepthBones;
import nijilive;

private void appendDashedLine(ref Vec3Array lines, vec3 start, vec3 end) {
    enum segmentCount = 16;
    auto delta = end - start;
    foreach (segment; 0 .. segmentCount) {
        if ((segment & 1) != 0) continue;
        auto t0 = cast(float)segment / segmentCount;
        auto t1 = cast(float)(segment + 1) / segmentCount;
        lines ~= start + delta * t0;
        lines ~= start + delta * t1;
    }
}

/** Keep the overlay cache aligned with the global selection outside drawing. */
void depthBoneEffectivePivotSelectionChanged(Node[] nodes) {
    ExDepthBone bone;
    foreach (node; nodes) {
        if (auto candidate = cast(ExDepthBone)node) {
            bone = candidate;
            break;
        }
    }

    ExDepthRigRoot root;
    for (Node cursor = bone; cursor !is null; cursor = cursor.parent) {
        if (auto candidate = cast(ExDepthRigRoot)cursor) {
            root = candidate;
            break;
        }
    }
    if (!ngShowDepthBones) {
        root = null;
        bone = null;
    }

    ngSetDepthBoneEffectivePivotSelection(root, bone);
    // Selection changes are user-visible immediately and happen only once per event.
    ngFlushDepthBoneEffectivePivotDirty();
}

/** Draw BoneSource effective yaw pivots without reusing DepthBone styling. */
void drawDepthBoneEffectivePivots(ExDepthRigRoot root, ExDepthBone selectedBone = null) {
    if (root is null || selectedBone is null) return;
    ngSetDepthBoneEffectivePivotSelection(root, selectedBone);

    Vec3Array selectedLines;
    Vec3Array selectedPoints;

    foreach (pivot; ngCachedDepthBoneSourceEffectivePivots(root, selectedBone)) {
        if (abs(pivot.rotationPivotXShift) <= 1e-4f) continue;
        appendDashedLine(selectedLines, pivot.bonePoint, pivot.effectivePoint);
        selectedPoints ~= pivot.effectivePoint;
    }

    // DepthBones are blue/yellow solid filled points. Effective pivots are
    // yellow-green dashed connectors ending in ring markers, so they remain distinct.
    if (selectedLines.length > 0) {
        inDbgSetBuffer(selectedLines);
        inDbgDrawLines(vec4(0.65f, 1.0f, 0.15f, 1.0f), root.transform.matrix);
    }
    if (selectedPoints.length > 0) {
        inDbgPointsSize(14);
        inDbgSetBuffer(selectedPoints);
        inDbgDrawPoints(vec4(0.65f, 1.0f, 0.15f, 1.0f), root.transform.matrix);
        inDbgPointsSize(6);
        inDbgSetBuffer(selectedPoints);
        inDbgDrawPoints(vec4(1.0f, 1.0f, 1.0f, 1.0f), root.transform.matrix);
    }
    inDbgPointsSize(4);
}
