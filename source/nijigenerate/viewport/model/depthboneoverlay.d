/*
    Copyright © 2026, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
*/
module nijigenerate.viewport.model.depthboneoverlay;

import nijigenerate.commands.depth.bone : ngCachedDepthBoneSourceEffectivePivots,
    ngFlushDepthBoneEffectivePivotDirty, ngSetDepthBoneEffectivePivotSelection;
import nijigenerate.commands.depth.bone_status :
    DepthBoneUpdateState,
    DepthBoneUpdateStatus,
    ngDepthBoneUpdateStatuses;
import nijigenerate.core.dbg;
import nijigenerate.core.input : WorldToViewport;
import nijigenerate.core.window : incUiAccentColor;
import nijigenerate.ext.nodes.exdepthbone;
import nijigenerate.project : incActivePuppet, ngShowDepthBones;
import nijigenerate.widgets.button : incButtonColored;
import nijigenerate.widgets.label : incTextColored, incTextLabel;
import nijigenerate.widgets.tooltip : incTooltip;
import bindbc.imgui;
import nijilive;
import nijilive.core.nodes.deformable : Deformable;
import nijilive.core.nodes.deformer.grid : GridDeformer;
import nijilive.core.nodes.deformer.path : PathDeformer;
import std.algorithm.comparison : max, min;
import std.algorithm.sorting : sort;
import std.conv : to;
import std.format : format;
import std.math : isFinite;
import std.string : toStringz;

struct DepthBoneOverlayGeometry {
    Vec3Array lines;
    Vec3Array parentLines;
    Vec3Array childLines;
    Vec3Array points;
    Vec3Array selectedPoints;
}

private vec4 depthBoneUpdateColor(DepthBoneUpdateState state) {
    final switch (state) {
    case DepthBoneUpdateState.Detected: return vec4(1.0f, 0.55f, 0.1f, 1.0f);
    case DepthBoneUpdateState.Queued: return vec4(0.72f, 0.35f, 1.0f, 1.0f);
    case DepthBoneUpdateState.Processing: return vec4(0.1f, 0.9f, 1.0f, 1.0f);
    case DepthBoneUpdateState.Applied: return vec4(0.2f, 1.0f, 0.35f, 1.0f);
    case DepthBoneUpdateState.Stale: return vec4(1.0f, 0.45f, 0.1f, 1.0f);
    case DepthBoneUpdateState.Failed: return vec4(1.0f, 0.1f, 0.15f, 1.0f);
    }
}

private ImVec4 depthBoneUpdateImColor(DepthBoneUpdateState state) {
    auto color = depthBoneUpdateColor(state);
    return ImVec4(color.r, color.g, color.b, color.a);
}

private enum float DepthBoneUpdateOverlayOpacity = 0.5f;
private enum float DepthBoneUpdateFontScale = 0.82f;
private enum float DepthBoneUpdateHorizontalPadding = 4.0f;
private enum float DepthBoneUpdateVerticalPadding = 1.0f;
private immutable vec4 DepthBoneUpdateOutlineColor =
    vec4(0.62f, 0.62f, 0.62f, DepthBoneUpdateOverlayOpacity);

private string depthBoneUpdateStateLabel(DepthBoneUpdateState state) {
    final switch (state) {
    case DepthBoneUpdateState.Detected: return "DETECTED";
    case DepthBoneUpdateState.Queued: return "WAIT";
    case DepthBoneUpdateState.Processing: return "RUN";
    case DepthBoneUpdateState.Applied: return "OK";
    case DepthBoneUpdateState.Stale: return "STALE";
    case DepthBoneUpdateState.Failed: return "ERROR";
    }
}

private string depthBoneUpdateTargetKind(Node target) {
    if (cast(GridDeformer)target) return "G";
    if (cast(PathDeformer)target) return "P";
    return "D";
}

/** Build a compact target-space outline without traversing its descendants. */
bool buildDepthBoneUpdateTargetOutline(
    Node targetNode,
    out Vec3Array lines,
    out mat4 targetToWorld,
    out vec3 labelWorld,
) {
    lines = Vec3Array.init;
    targetToWorld = mat4.identity;
    labelWorld = vec3.init;
    auto target = cast(Deformable)targetNode;
    if (target is null || target.vertices.length == 0) return false;

    auto first = target.vertices[0].toVector();
    if (target.deformation.length > 0) first += target.deformation[0].toVector();
    auto minPoint = first;
    auto maxPoint = first;
    foreach (i; 1 .. target.vertices.length) {
        auto point = target.vertices[i].toVector();
        if (i < target.deformation.length) point += target.deformation[i].toVector();
        minPoint.x = min(minPoint.x, point.x);
        minPoint.y = min(minPoint.y, point.y);
        maxPoint.x = max(maxPoint.x, point.x);
        maxPoint.y = max(maxPoint.y, point.y);
    }

    return buildDepthBoneUpdateTargetOutline(
        targetNode, true, minPoint, maxPoint, lines, targetToWorld, labelWorld);
}

private bool buildDepthBoneUpdateTargetOutline(
    Node targetNode,
    bool hasLocalBounds,
    vec2 localBoundsMin,
    vec2 localBoundsMax,
    out Vec3Array lines,
    out mat4 targetToWorld,
    out vec3 labelWorld,
) {
    lines = Vec3Array.init;
    targetToWorld = mat4.identity;
    labelWorld = vec3.init;
    if (targetNode is null || !hasLocalBounds) return false;

    auto minPoint = localBoundsMin;
    auto maxPoint = localBoundsMax;
    auto span = maxPoint - minPoint;
    auto margin = max(4.0f, max(span.x, span.y) * 0.03f);
    minPoint -= vec2(margin, margin);
    maxPoint += vec2(margin, margin);
    lines = Vec3Array([
        vec3(minPoint.x, minPoint.y, 0), vec3(maxPoint.x, minPoint.y, 0),
        vec3(maxPoint.x, minPoint.y, 0), vec3(maxPoint.x, maxPoint.y, 0),
        vec3(maxPoint.x, maxPoint.y, 0), vec3(minPoint.x, maxPoint.y, 0),
        vec3(minPoint.x, maxPoint.y, 0), vec3(minPoint.x, minPoint.y, 0),
    ]);
    targetToWorld = targetNode.getDynamicMatrix();
    labelWorld = (targetToWorld * vec4(maxPoint.x, minPoint.y, 0, 1)).xyz;
    return true;
}

private string depthBoneUpdateProgress(ref DepthBoneUpdateStatus status) {
    if (status.expectedWork == 0) return null;
    return " %s/%s".format(status.appliedWork, status.expectedWork);
}

float depthBoneUpdateProgressFraction(ref DepthBoneUpdateStatus status) {
    if (status.expectedWork == 0) return 0.0f;
    return min(1.0f,
        cast(float)status.appliedWork / cast(float)status.expectedWork);
}

string depthBoneUpdateViewportLabel(ref DepthBoneUpdateStatus status) {
    return status.target is null ? null : status.target.name;
}

private ImVec2 depthBoneUpdateProgressSize(
    ref DepthBoneUpdateStatus status,
) {
    auto label = depthBoneUpdateViewportLabel(status);
    ImVec2 labelSize;
    ImFont_CalcTextSizeA(
        &labelSize,
        igGetFont(),
        igGetFontSize() * DepthBoneUpdateFontScale,
        float.max,
        0.0f,
        label.ptr,
        label.ptr + label.length);
    return ImVec2(
        labelSize.x + DepthBoneUpdateHorizontalPadding * 2.0f,
        labelSize.y + DepthBoneUpdateVerticalPadding * 2.0f);
}

private void drawDepthBoneUpdateProgress(
    ref DepthBoneUpdateStatus status,
    ImDrawList* drawList,
    ImVec2 screenPosition,
) {
    if (drawList is null) return;
    auto size = depthBoneUpdateProgressSize(status);
    auto bottomRight = ImVec2(
        screenPosition.x + size.x,
        screenPosition.y + size.y);
    auto fraction = depthBoneUpdateProgressFraction(status);
    auto fillRight = screenPosition.x + size.x * fraction;
    auto rounding = min(4.0f, size.y * 0.25f);
    auto backgroundColor = igGetColorU32(
        ImVec4(0.16f, 0.16f, 0.16f, DepthBoneUpdateOverlayOpacity));
    auto fillColor = igGetColorU32(
        incUiAccentColor(DepthBoneUpdateOverlayOpacity));
    auto borderColor = igGetColorU32(
        ImVec4(0.62f, 0.62f, 0.62f, DepthBoneUpdateOverlayOpacity));
    auto textColor = igGetColorU32(
        ImVec4(1.0f, 1.0f, 1.0f, DepthBoneUpdateOverlayOpacity));

    ImDrawList_AddRectFilled(
        drawList, screenPosition, bottomRight, backgroundColor, rounding);
    if (fraction > 0.0f) {
        ImDrawList_AddRectFilled(
            drawList,
            screenPosition,
            ImVec2(fillRight, bottomRight.y),
            fillColor,
            min(rounding, (fillRight - screenPosition.x) * 0.5f));
    }
    ImDrawList_AddRect(
        drawList,
        screenPosition,
        bottomRight,
        borderColor,
        rounding,
        ImDrawFlags.RoundCornersAll,
        1.0f);

    auto label = depthBoneUpdateViewportLabel(status);
    ImVec2 labelSize;
    auto font = igGetFont();
    auto fontSize = igGetFontSize() * DepthBoneUpdateFontScale;
    ImFont_CalcTextSizeA(
        &labelSize,
        font,
        fontSize,
        float.max,
        0.0f,
        label.ptr,
        label.ptr + label.length);
    ImDrawList_AddText(
        drawList,
        font,
        fontSize,
        ImVec2(
            screenPosition.x + (size.x - labelSize.x) * 0.5f,
            screenPosition.y + (size.y - labelSize.y) * 0.5f),
        textColor,
        label.ptr,
        label.ptr + label.length);
}

/**
    Draw target progress in the viewport image's screen-space overlay phase.

    viewportOrigin intentionally matches incBeginViewportToolArea's explicit
    position origin so existing WorldToViewport placement remains unchanged.
*/
void drawDepthBoneUpdateProgressOverlays(
    ImDrawList* drawList,
    ImVec2 viewportOrigin,
    ImRect viewportRect,
) {
    if (drawList is null) return;
    auto puppet = incActivePuppet();
    if (puppet is null) return;
    auto statuses = ngDepthBoneUpdateStatuses(puppet);
    if (statuses.length == 0) return;

    auto clipMin = ImVec2(
        min(viewportRect.Min.x, viewportRect.Max.x),
        min(viewportRect.Min.y, viewportRect.Max.y));
    auto clipMax = ImVec2(
        max(viewportRect.Min.x, viewportRect.Max.x),
        max(viewportRect.Min.y, viewportRect.Max.y));
    if (!clipMin.x.isFinite || !clipMin.y.isFinite ||
        !clipMax.x.isFinite || !clipMax.y.isFinite ||
        clipMax.x <= clipMin.x || clipMax.y <= clipMin.y) return;

    ImDrawList_PushClipRect(drawList, clipMin, clipMax, true);
    foreach (ref status; statuses) {
        if (status.target is null || status.localOutline.length == 0 ||
            !status.hasLabelWorld) continue;
        auto viewportPoint = WorldToViewport(
            status.labelWorld.x, status.labelWorld.y);
        auto screenPosition = ImVec2(
            viewportOrigin.x + viewportPoint.x + 6.0f,
            viewportOrigin.y + viewportPoint.y - 6.0f);
        if (!screenPosition.x.isFinite || !screenPosition.y.isFinite) continue;
        drawDepthBoneUpdateProgress(status, drawList, screenPosition);
    }
    ImDrawList_PopClipRect(drawList);
}

/** Draw event-driven update state around affected GridDeformer/PathDeformer targets. */
void drawDepthBoneUpdateTargets() {
    auto puppet = incActivePuppet();
    if (puppet is null) return;
    foreach (status; ngDepthBoneUpdateStatuses(puppet)) {
        auto outline = status.localOutline;
        if (outline.length == 0 || status.target is null) continue;
        inDbgSetBuffer(outline);
        inDbgDrawLines(
            DepthBoneUpdateOutlineColor, status.target.getDynamicMatrix());
    }
}

private string depthBoneUpdateTooltip(ref DepthBoneUpdateStatus status) {
    auto parameter = status.parameter is null ? "(none)" : status.parameter.name;
    auto text = "%s [%s]  %s%s\nParameter: %s  Key: (%s,%s)\nReason: %s".format(
        status.target.name,
        depthBoneUpdateTargetKind(status.target),
        depthBoneUpdateStateLabel(status.state),
        depthBoneUpdateProgress(status),
        parameter,
        status.keypoint.x,
        status.keypoint.y,
        status.reason.length == 0 ? "(none)" : status.reason);
    if (status.retryCount > 0) text ~= "\nRetry: %s".format(status.retryCount);
    if (status.detail.length > 0) text ~= "\nDetail: " ~ status.detail;
    return text;
}

/** Draw the bottom detail summary; target overlays are drawn with the viewport image. */
void drawDepthBoneUpdateStatusUi() {
    auto puppet = incActivePuppet();
    if (puppet is null) return;
    auto statuses = ngDepthBoneUpdateStatuses(puppet);
    if (statuses.length == 0) return;
    statuses.sort!((a, b) => a.target.name < b.target.name);

    size_t detected;
    size_t queued;
    size_t processing;
    size_t applied;
    size_t stale;
    size_t failed;
    foreach (ref status; statuses) {
        final switch (status.state) {
        case DepthBoneUpdateState.Detected: detected++; break;
        case DepthBoneUpdateState.Queued: queued++; break;
        case DepthBoneUpdateState.Processing: processing++; break;
        case DepthBoneUpdateState.Applied: applied++; break;
        case DepthBoneUpdateState.Stale: stale++; break;
        case DepthBoneUpdateState.Failed: failed++; break;
        }

    }

    auto summary = "Depth  wait:%s  run:%s  ok:%s  stale:%s  error:%s".format(
        queued + detected, processing, applied, stale, failed);
    auto summaryState = failed > 0 ? DepthBoneUpdateState.Failed :
        (processing > 0 ? DepthBoneUpdateState.Processing :
        (queued + detected > 0 ? DepthBoneUpdateState.Queued :
        (stale > 0 ? DepthBoneUpdateState.Stale : DepthBoneUpdateState.Applied)));
    if (incButtonColored(
        summary.toStringz,
        ImVec2(0, 26),
        depthBoneUpdateImColor(summaryState))) {
        igOpenPopup("DepthBoneUpdateDetails");
    }
    incTooltip("Depth Bone target update status. Click for details.");

    if (igBeginPopup("DepthBoneUpdateDetails")) {
        incTextLabel("Depth Bone Updates");
        foreach (ref status; statuses) {
            incTextColored(
                depthBoneUpdateImColor(status.state),
                "%s [%s]  %s%s".format(
                    status.target.name,
                    depthBoneUpdateTargetKind(status.target),
                    depthBoneUpdateStateLabel(status.state),
                    depthBoneUpdateProgress(status)));
            incTextLabel(depthBoneUpdateTooltip(status));
            igSeparator();
        }
        igEndPopup();
    }
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
