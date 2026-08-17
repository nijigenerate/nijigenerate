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

struct DepthBoneOverlayGeometry {
    Vec3Array lines;
    Vec3Array parentLines;
    Vec3Array childLines;
    Vec3Array points;
    Vec3Array selectedPoints;
}

/** Classify DepthBone points and links relative to the selected bone. */
DepthBoneOverlayGeometry buildDepthBoneOverlayGeometry(
    ExDepthRigRoot root,
    ExDepthBone selectedBone = null,
) {
    DepthBoneOverlayGeometry geometry;
    if (root is null) return geometry;

    auto rootToLocal = root.transform.matrix.inverse;

    vec3 bonePoint(ExDepthBone bone) {
        auto world = bone.transform.matrix * vec4(0, 0, 0, 1);
        return (rootToLocal * world).xyz;
    }

    foreach (bone; root.depthBones()) {
        auto point = bonePoint(bone);
        if (bone is selectedBone) {
            geometry.selectedPoints ~= point;
        } else {
            geometry.points ~= point;
        }

        if (auto parentBone = cast(ExDepthBone)bone.parent) {
            auto parentPoint = bonePoint(parentBone);
            auto line = [parentPoint, point];
            if (bone is selectedBone) {
                // The selected node owns its incoming parent link.
                geometry.parentLines ~= line;
            } else if (parentBone is selectedBone) {
                // Child links start at the selected node and end at each child.
                geometry.childLines ~= line;
            } else {
                geometry.lines ~= line;
            }
        }
    }
    return geometry;
}

/** Draw the DepthBone hierarchy with an explicit selected node and directed link colors. */
void drawDepthBones(ExDepthRigRoot root, ExDepthBone selectedBone = null) {
    auto geometry = buildDepthBoneOverlayGeometry(root, selectedBone);
    if (root is null) return;

    if (geometry.lines.length > 0) {
        inDbgSetBuffer(geometry.lines);
        inDbgDrawLines(vec4(0.55f, 0.75f, 1.0f, 1.0f), root.transform.matrix);
    }
    if (geometry.parentLines.length > 0) {
        inDbgSetBuffer(geometry.parentLines);
        inDbgDrawLines(vec4(1.0f, 1.0f, 0.0f, 1.0f), root.transform.matrix);
    }
    if (geometry.childLines.length > 0) {
        inDbgSetBuffer(geometry.childLines);
        inDbgDrawLines(vec4(0.2f, 1.0f, 0.8f, 1.0f), root.transform.matrix);
    }
    if (geometry.points.length > 0) {
        inDbgPointsSize(4);
        inDbgSetBuffer(geometry.points);
        inDbgDrawPoints(vec4(0.55f, 0.75f, 1.0f, 1.0f), root.transform.matrix);
    }
    if (geometry.selectedPoints.length > 0) {
        // A ring makes the selected node unambiguous at the junction of its links.
        inDbgPointsSize(12);
        inDbgSetBuffer(geometry.selectedPoints);
        inDbgDrawPoints(vec4(1.0f, 0.9f, 0.2f, 1.0f), root.transform.matrix);
        inDbgPointsSize(5);
        inDbgSetBuffer(geometry.selectedPoints);
        inDbgDrawPoints(vec4(1.0f, 1.0f, 1.0f, 1.0f), root.transform.matrix);
    }
    inDbgPointsSize(4);

    drawDepthBoneEffectivePivots(root, selectedBone);
}

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
