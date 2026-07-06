module nijigenerate.viewport.depth.draw.viewport;

import bindbc.imgui;
import nijigenerate.core.dbg;
import nijigenerate.core.input : WorldToViewport, incInputGetMousePosition, incInputIsMouseClicked;
import nijigenerate.ext.nodes.exdepthmapped : DepthMappedNode;
import nijigenerate.viewport.base;
import nijigenerate.viewport.depth.camera : unprojectDepthPoint;
import nijigenerate.viewport.depth.common.session;
import nijigenerate.viewport.depth.common.targetview : DepthTargetView;
import nijigenerate.viewport.depth.draw.composer;
import nijigenerate.viewport.depth.draw.diagnostics;
import nijigenerate.viewport.depth.draw.gpu;
import nijigenerate.viewport.depth.draw.layer;
import nijigenerate.viewport.depth.draw.session;
import nijigenerate.viewport.depth.renderer;
import nijigenerate.widgets;
import nijigenerate.widgets.viewport;
import nijilive;
import nijilive.math : Vec3Array, vec2, vec3, vec4;
import i18n;
import std.conv : to;

struct DepthDrawViewportRenderGeometry {
    DepthTargetRenderLine[] normalImageLines;
    DepthTargetRenderLine[] rawDepthLines;
    DepthTargetRenderLine[] coverageLines;
    DepthDrawViewportCoveragePoint[] coveragePoints;
    DepthTargetRenderMesh[] targetMeshes;
    ulong[] targetMeshGridUuids;
    DepthTargetRenderLine[] targetLines;
    DepthTargetRenderLine[] layerPlaneLines;
    DepthTargetRenderLine[] selectedLayerLines;
    DepthTargetRenderLine[] depthRangeLines;
    DepthTargetRenderLine[] gapLines;
    DepthTargetRenderLine[] overlapGapLines;
    DepthDrawViewportGapHandle[] gapHandles;
    DepthDrawViewportTextLabel[] textLabels;
    DepthTargetRenderPoint[] missingPoints;
    DepthTargetRenderPoint[] winningPoints;
    DepthDrawViewportWinningPointGroup[] winningPointGroups;

    DepthDrawViewportRenderGeometryStats stats() const {
        DepthDrawViewportRenderGeometryStats result;
        result.normalImageLines = normalImageLines.length;
        result.rawDepthLines = rawDepthLines.length;
        result.coverageLines = coverageLines.length;
        result.coveragePoints = coveragePoints.length;
        result.targetMeshes = targetMeshes.length;
        foreach (mesh; targetMeshes) result.targetSurfaceTriangles += mesh.indices.length / 3;
        result.targetMeshGridUuids = targetMeshGridUuids.length;
        result.targetLines = targetLines.length;
        result.layerPlaneLines = layerPlaneLines.length;
        result.selectedLayerLines = selectedLayerLines.length;
        result.depthRangeLines = depthRangeLines.length;
        result.gapLines = gapLines.length;
        result.overlapGapLines = overlapGapLines.length;
        result.gapHandles = gapHandles.length;
        result.textLabels = textLabels.length;
        result.missingPoints = missingPoints.length;
        result.winningPoints = winningPoints.length;
        result.winningPointGroups = winningPointGroups.length;
        return result;
    }
}

struct DepthDrawViewportRenderGeometryStats {
    size_t normalImageLines;
    size_t rawDepthLines;
    size_t coverageLines;
    size_t coveragePoints;
    size_t targetMeshes;
    size_t targetSurfaceTriangles;
    size_t targetMeshGridUuids;
    size_t targetLines;
    size_t layerPlaneLines;
    size_t selectedLayerLines;
    size_t depthRangeLines;
    size_t gapLines;
    size_t overlapGapLines;
    size_t gapHandles;
    size_t textLabels;
    size_t missingPoints;
    size_t winningPoints;
    size_t winningPointGroups;
}

struct DepthDrawViewportMeshRenderPlan {
    ulong targetGridUuid;
    bool hasTarget;
    size_t drawableChildren;
    bool willAttemptTexture;
    bool hasMesh;
}

struct DepthDrawViewportCoveragePoint {
    string layerId;
    vec2 documentPoint;
    float alpha;
    DepthTargetRenderPoint renderPoint;
}

struct DepthDrawViewportGapHandle {
    string backLayerId;
    string frontLayerId;
    bool overlap;
    vec2 documentPoint;
    float backDepth;
    float frontDepth;
    DepthTargetRenderPoint renderPoint;
}

struct DepthDrawViewportTextLabel {
    string text;
    vec2 point;
    string backLayerId;
    string frontLayerId;
    bool overlap;
}

struct DepthDrawViewportWinningPointGroup {
    string layerId;
    vec4 color;
    DepthTargetRenderPoint[] points;
}

class DepthDrawViewport : Viewport {
private:
    Node[] selection;
    DepthViewSession viewSession;
    DepthDrawSession drawSession;
    DepthTargetRenderer renderer;
    DepthTextureMeshRenderer meshRenderer;
    DepthTargetOffscreenTextureRenderer offscreenTextureRenderer;
    int documentWidth = 1;
    int documentHeight = 1;
    DepthDrawComposeResult[ulong] previewResults;
    DepthDrawGpuTargetComposeJob[ulong] pendingGpuPreviewJobs;
    string gpuPreviewErrorMessage;

    void syncTargets() {
        viewSession.clear();
        foreach (node; selection) {
            auto target = cast(Deformable)node;
            if (target is null || cast(DepthMappedNode)target is null) continue;
            viewSession.ensureTarget(target);
        }
        if (drawSession !is null && viewSession.targetByGrid(drawSession.selectedGridUuid) !is null) {
            viewSession.selectTarget(drawSession.selectedGridUuid);
        }
    }

    float selectedDepthDisplayScale() {
        auto target = viewSession.selectedTarget();
        return target is null ? 1.0f : target.depthDisplayScale();
    }

    const(DepthDrawComposeResult)* selectedPreviewResult(ref DepthDrawComposeResult result) {
        if (drawSession is null) return null;
        if (auto selected = drawSession.selectedGridUuid in previewResults) {
            result = *selected;
            return &result;
        }
        foreach (gridUuid, preview; previewResults) {
            result = preview;
            return &result;
        }
        return null;
    }

    vec2 rectMin(DepthDrawRect rect) {
        return vec2(cast(float)rect.left, cast(float)rect.top);
    }

    vec2 rectMax(DepthDrawRect rect) {
        return vec2(cast(float)(rect.left + rect.width), cast(float)(rect.top + rect.height));
    }

    bool findDepthSpaceLayer(ref DepthDrawDepthSpaceSummary depthSpace, string layerId, out DepthDrawDepthSpaceLayerPlane plane) {
        foreach (candidate; depthSpace.layers) {
            if (candidate.layerId != layerId) continue;
            plane = candidate;
            return true;
        }
        return false;
    }

    vec2 gapMarkerPoint(DepthDrawDepthSpaceLayerPlane backPlane, DepthDrawDepthSpaceLayerPlane frontPlane) {
        auto backMin = rectMin(backPlane.bounds);
        auto backMax = rectMax(backPlane.bounds);
        auto frontMin = rectMin(frontPlane.bounds);
        auto frontMax = rectMax(frontPlane.bounds);

        auto minX = backMin.x > frontMin.x ? backMin.x : frontMin.x;
        auto maxX = backMax.x < frontMax.x ? backMax.x : frontMax.x;
        auto minY = backMin.y > frontMin.y ? backMin.y : frontMin.y;
        auto maxY = backMax.y < frontMax.y ? backMax.y : frontMax.y;
        if (minX > maxX) {
            minX = (backMin.x + frontMin.x) * 0.5f;
            maxX = (backMax.x + frontMax.x) * 0.5f;
        }
        if (minY > maxY) {
            minY = (backMin.y + frontMin.y) * 0.5f;
            maxY = (backMax.y + frontMax.y) * 0.5f;
        }
        return vec2((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    }

    Vec3Array toLineBuffer(DepthTargetRenderLine[] lines) {
        Vec3Array points;
        foreach (line; lines) {
            points ~= vec3(line.p0.x, line.p0.y, 0.0f);
            points ~= vec3(line.p1.x, line.p1.y, 0.0f);
        }
        return points;
    }

    Vec3Array toPointBuffer(DepthTargetRenderPoint[] points) {
        Vec3Array buffer;
        foreach (point; points) {
            buffer ~= vec3(point.point.x, point.point.y, 0.0f);
        }
        return buffer;
    }

    void drawLines(DepthTargetRenderLine[] lines, vec4 color, float width = 1.0f) {
        if (lines.length == 0) return;
        auto points = toLineBuffer(lines);
        inDbgSetBuffer(points);
        inDbgLineWidth(width);
        inDbgDrawLines(color);
        inDbgLineWidth(1.0f);
    }

    void drawPoints(DepthTargetRenderPoint[] points, vec4 color, float size = 4.0f) {
        if (points.length == 0) return;
        auto buffer = toPointBuffer(points);
        inDbgSetBuffer(buffer);
        inDbgPointsSize(size);
        inDbgDrawPoints(color);
        inDbgPointsSize(1.0f);
    }

    void drawRenderGeometry(ref DepthDrawViewportRenderGeometry geometry, Camera viewportCamera) {
        foreach (i, mesh; geometry.targetMeshes) {
            Texture texture;
            if (i < geometry.targetMeshGridUuids.length) {
                auto target = viewSession.targetByGrid(geometry.targetMeshGridUuids[i]);
                if (target !is null) texture = offscreenTextureRenderer.render(target.getTarget());
            }
            if (texture !is null) {
                meshRenderer.draw(texture, mesh.positions, mesh.uvs, mesh.indices, viewportCamera);
            } else {
                meshRenderer.drawSolid(mesh.positions, mesh.indices, viewportCamera, vec4(0.25f, 0.5f, 1.0f, 0.18f));
            }
        }
        drawLines(geometry.normalImageLines, vec4(0.65f, 0.65f, 0.65f, 0.45f), 1.0f);
        drawLines(geometry.rawDepthLines, vec4(0.95f, 0.95f, 0.95f, 0.55f), 1.0f);
        drawLines(geometry.coverageLines, vec4(0.2f, 0.95f, 0.2f, 0.6f), 1.0f);
        DepthTargetRenderPoint[] coverageRenderPoints;
        foreach (point; geometry.coveragePoints) coverageRenderPoints ~= point.renderPoint;
        drawPoints(coverageRenderPoints, vec4(0.2f, 1.0f, 0.25f, 0.9f), 3.0f);
        drawLines(geometry.layerPlaneLines, vec4(0.15f, 0.55f, 1.0f, 0.55f), 1.0f);
        drawLines(geometry.selectedLayerLines, vec4(1.0f, 1.0f, 0.15f, 1.0f), 2.0f);
        drawLines(geometry.depthRangeLines, vec4(1.0f, 0.65f, 0.15f, 0.8f), 1.0f);
        drawLines(geometry.gapLines, vec4(0.2f, 1.0f, 0.9f, 0.9f), 2.0f);
        drawLines(geometry.overlapGapLines, vec4(1.0f, 0.05f, 0.05f, 1.0f), 3.0f);
        DepthTargetRenderPoint[] gapHandlePoints;
        foreach (handle; geometry.gapHandles) gapHandlePoints ~= handle.renderPoint;
        drawPoints(gapHandlePoints, vec4(0.95f, 1.0f, 0.1f, 1.0f), 7.0f);
        drawLines(geometry.targetLines, vec4(0.55f, 0.55f, 0.55f, 0.75f), 1.0f);
        drawPoints(geometry.missingPoints, vec4(1.0f, 0.1f, 0.1f, 1.0f), 6.0f);
        foreach (group; geometry.winningPointGroups) {
            drawPoints(group.points, group.color, 5.0f);
        }
        if (geometry.winningPointGroups.length == 0) {
            drawPoints(geometry.winningPoints, vec4(0.1f, 0.9f, 0.35f, 1.0f), 5.0f);
        }
    }

    void addWinningPoint(ref DepthDrawViewportRenderGeometry geometry, string layerId, vec4 color, DepthTargetRenderPoint point) {
        geometry.winningPoints ~= point;
        foreach (ref group; geometry.winningPointGroups) {
            if (group.layerId != layerId) continue;
            group.points ~= point;
            return;
        }
        DepthDrawViewportWinningPointGroup group;
        group.layerId = layerId;
        group.color = color;
        group.points ~= point;
        geometry.winningPointGroups ~= group;
    }

    void addCoveragePoints(
        ref DepthDrawViewportRenderGeometry geometry,
        ref const(DepthDrawLayer) layer,
        float representativeDepth,
        float depthDisplayScale
    ) {
        if (!layer.hasNormalCoverage() || layer.width <= 0 || layer.height <= 0) return;
        auto stepX = cast(float)layer.bounds.width / cast(float)layer.width;
        auto stepY = cast(float)layer.bounds.height / cast(float)layer.height;
        foreach (y; 0 .. layer.height) {
            foreach (x; 0 .. layer.width) {
                auto pixelIndex = cast(size_t)(y * layer.width + x);
                auto rgbaIndex = pixelIndex * 4 + 3;
                if (rgbaIndex >= layer.normalCoverage.length) continue;
                auto alphaByte = layer.normalCoverage[rgbaIndex];
                if (alphaByte == 0) continue;
                auto documentPoint = vec2(
                    cast(float)layer.bounds.left + (cast(float)x + 0.5f) * stepX,
                    cast(float)layer.bounds.top + (cast(float)y + 0.5f) * stepY
                );
                DepthDrawViewportCoveragePoint point;
                point.layerId = layer.id;
                point.documentPoint = documentPoint;
                point.alpha = cast(float)alphaByte / 255.0f;
                point.renderPoint = renderer.buildPoint(
                    documentPoint,
                    representativeDepth,
                    depthDisplayScale,
                    viewSession.camera,
                    3.0f
                );
                geometry.coveragePoints ~= point;
            }
        }
    }

public:
    this(DepthDrawSession drawSession = null) {
        this.viewSession = new DepthViewSession();
        this.drawSession = drawSession is null ? new DepthDrawSession() : drawSession;
        this.renderer = new DepthTargetRenderer();
        this.meshRenderer = new DepthTextureMeshRenderer();
        this.offscreenTextureRenderer = new DepthTargetOffscreenTextureRenderer();
    }

    DepthViewSession depthViewSession() {
        return viewSession;
    }

    DepthDrawSession depthDrawSession() {
        return drawSession;
    }

    DepthTargetRenderer targetRenderer() {
        return renderer;
    }

    void setDepthDrawSession(DepthDrawSession session) {
        drawSession = session is null ? new DepthDrawSession() : session;
        syncTargets();
    }

    void setDocumentSize(int width, int height) {
        documentWidth = width > 0 ? width : 1;
        documentHeight = height > 0 ? height : 1;
        if (drawSession !is null) drawSession.markAllPreviewDirty();
    }

    DepthDrawComposeResult previewResult(ulong gridUuid) {
        auto result = gridUuid in previewResults;
        return result is null ? DepthDrawComposeResult() : *result;
    }

    DepthDrawComposeResult composePreview(ulong gridUuid) {
        if (drawSession is null) return DepthDrawComposeResult();
        auto target = viewSession.targetByGrid(gridUuid);
        if (target is null) return DepthDrawComposeResult();
        auto result = ngPreviewDepthDrawTarget(drawSession, target, documentWidth, documentHeight);
        previewResults[gridUuid] = result;
        return result;
    }

    bool composeSelectedPreviewForUpdate() {
        if (drawSession is null ||
            drawSession.display.useGpuPreview ||
            !drawSession.isTargetPreviewDirty(viewSession.selectedGridUuid)) {
            return false;
        }
        composePreview(viewSession.selectedGridUuid);
        return true;
    }

    string gpuPreviewError() const {
        return gpuPreviewErrorMessage;
    }

    size_t pendingGpuPreviewJobCount() const {
        return pendingGpuPreviewJobs.length;
    }

    bool submitGpuPreview(ulong gridUuid) {
        if (drawSession is null) return false;
        if ((gridUuid in pendingGpuPreviewJobs) !is null) return false;
        auto target = viewSession.targetByGrid(gridUuid);
        if (target is null) return false;

        DepthDrawGpuTargetComposeJob job;
        string error;
        if (!ngSubmitDepthDrawGpuTargetCompose(drawSession, target, documentWidth, documentHeight, job, error)) {
            gpuPreviewErrorMessage = error.length ? error : "DepthDraw GPU preview submit failed";
            return false;
        }
        pendingGpuPreviewJobs[gridUuid] = job;
        gpuPreviewErrorMessage = null;
        return true;
    }

    size_t pollGpuPreviews() {
        size_t completed;
        foreach (gridUuid; pendingGpuPreviewJobs.keys) {
            auto job = gridUuid in pendingGpuPreviewJobs;
            if (job is null) continue;
            DepthDrawGpuTargetComposePollResult pollResult;
            string error;
            if (!ngPollDepthDrawGpuTargetCompose(*job, pollResult, error)) {
                gpuPreviewErrorMessage = error.length ? error : "DepthDraw GPU preview poll failed";
                pendingGpuPreviewJobs.remove(gridUuid);
                continue;
            }
            if (!pollResult.ready) continue;
            pendingGpuPreviewJobs.remove(gridUuid);
            previewResults[gridUuid] = pollResult.result;
            auto target = viewSession.targetByGrid(gridUuid);
            if (target !is null && pollResult.result.depths.length > 0) {
                target.replaceWorkingDepths(pollResult.result.depths);
            }
            if (drawSession !is null) drawSession.clearTargetPreviewDirty(gridUuid);
            completed++;
        }
        return completed;
    }

    size_t composeDirtyGpuPreviews() {
        if (drawSession is null) return 0;
        auto completed = pollGpuPreviews();
        foreach (gridUuid; drawSession.dirtyTargetGridIds()) {
            if (viewSession.targetByGrid(gridUuid) is null) continue;
            if (submitGpuPreview(gridUuid)) completed++;
        }
        return completed;
    }

    bool selectLayerPlaneAtDocumentPoint(vec2 documentPoint) {
        if (drawSession is null) return false;
        auto selected = drawSession.selectLayerPlaneAtDocumentPoint(documentPoint);
        if (selected && viewSession.targetByGrid(drawSession.selectedGridUuid) !is null) {
            viewSession.selectTarget(drawSession.selectedGridUuid);
        }
        return selected;
    }

    vec2 depthViewPointToDocumentPoint(vec2 depthViewPoint, float representativeDepth = 0.0f) {
        return unprojectDepthPoint(
            depthViewPoint,
            -representativeDepth * selectedDepthDisplayScale(),
            viewSession.camera
        );
    }

    bool selectLayerPlaneAtDepthViewPoint(vec2 depthViewPoint, float representativeDepth = 0.0f) {
        return selectLayerPlaneAtDocumentPoint(depthViewPointToDocumentPoint(depthViewPoint, representativeDepth));
    }

    bool selectTargetAtDepthViewPoint(vec2 depthViewPoint, float radius = 14.0f) {
        if (drawSession is null) return false;
        DepthTargetView bestTarget;
        auto bestDistance = radius;
        foreach (target; viewSession.targets) {
            if (target is null || target.getTarget() is null) continue;
            auto mesh = renderer.buildMesh(target, viewSession.camera);
            foreach (point; mesh.positions) {
                auto distance = (point - depthViewPoint).length();
                if (distance > bestDistance) continue;
                bestDistance = distance;
                bestTarget = target;
            }
        }
        if (bestTarget is null || bestTarget.getTarget() is null) return false;
        auto gridUuid = bestTarget.getTarget().uuid;
        if (!drawSession.selectTargetGrid(gridUuid)) return false;
        return viewSession.selectTarget(gridUuid);
    }

    size_t composeDirtyPreviews() {
        if (drawSession is null) return 0;
        if (drawSession.display.useGpuPreview) return composeDirtyGpuPreviews();
        size_t composed;
        foreach (gridUuid; drawSession.dirtyTargetGridIds()) {
            if (viewSession.targetByGrid(gridUuid) is null) continue;
            composePreview(gridUuid);
            composed++;
        }
        return composed;
    }

    DepthDrawViewportRenderGeometry collectRenderGeometry() {
        DepthDrawViewportRenderGeometry geometry;
        if (drawSession is null) return geometry;

        foreach (target; viewSession.targets) {
            if (target is null) continue;
            auto targetNode = target.getTarget();
            if (targetNode is null) continue;
            if (drawSession.display.showComposite) {
                auto targetMesh = renderer.buildMesh(target, viewSession.camera);
                geometry.targetMeshes ~= targetMesh;
                geometry.targetMeshGridUuids ~= targetNode.uuid;
                geometry.targetLines ~= renderer.buildGridLines(target.getVertices(), targetMesh.positions);
            }

            auto result = targetNode.uuid in previewResults;
            if (result is null) continue;

            auto summary = ngDepthDrawCompositePreviewSummary(*result);
            auto vertices = target.getVertices();
            foreach (point; summary.points) {
                if (!drawSession.display.showComposite) continue;
                if (point.vertexIndex >= vertices.length) continue;
                auto renderPoint = renderer.buildPoint(
                    vertices[point.vertexIndex],
                    point.depth,
                    target.depthDisplayScale(),
                    viewSession.camera,
                    point.missing ? 6.0f : 5.0f
                );
                if (point.missing) {
                    if (drawSession.display.showMissingVertices) geometry.missingPoints ~= renderPoint;
                } else if (drawSession.display.showWinningLayer) {
                    addWinningPoint(geometry, point.winningLayerId, point.winningLayerColor, renderPoint);
                }
            }
        }

        DepthDrawComposeResult preview;
        auto previewPointer = selectedPreviewResult(preview);
        auto depthSpace = ngDepthDrawDepthSpaceSummary(drawSession, previewPointer);
        auto depthDisplayScale = selectedDepthDisplayScale();
        foreach (plane; depthSpace.layers) {
            if (!plane.visible || !plane.enabled) continue;
            auto minPoint = rectMin(plane.bounds);
            auto maxPoint = rectMax(plane.bounds);
            auto sourcePlaneLines = renderer.buildPlaneLines(
                minPoint,
                maxPoint,
                plane.representativeDepth,
                depthDisplayScale,
                viewSession.camera
            );
            auto sourceLayer = drawSession.layerById(plane.layerId);
            if (sourceLayer !is null) {
                if (drawSession.display.showNormalImage && sourceLayer.rgba.length > 0) {
                    geometry.normalImageLines ~= sourcePlaneLines;
                }
                if (drawSession.display.showRawDepth && sourceLayer.hasDepthPixels()) {
                    geometry.rawDepthLines ~= sourcePlaneLines;
                }
                if (drawSession.display.showCoverage && sourceLayer.hasNormalCoverage()) {
                    geometry.coverageLines ~= sourcePlaneLines;
                    addCoveragePoints(geometry, *sourceLayer, plane.representativeDepth, depthDisplayScale);
                }
            }
            if (drawSession.display.showLayerPlanes) {
                geometry.layerPlaneLines ~= sourcePlaneLines;
                if (plane.selected) {
                    geometry.selectedLayerLines ~= sourcePlaneLines;
                }
            }
            if (drawSession.display.showDepthRanges && plane.hasDepthRange) {
                geometry.depthRangeLines ~= renderer.buildRangeLines(
                    minPoint,
                    maxPoint,
                    plane.minDepth,
                    plane.maxDepth,
                    depthDisplayScale,
                    viewSession.camera
                );
            }
        }
        if (drawSession.display.showDepthRanges) {
            foreach (gap; depthSpace.gaps) {
                if (!gap.valid) continue;
                DepthDrawDepthSpaceLayerPlane backPlane;
                DepthDrawDepthSpaceLayerPlane frontPlane;
                if (!findDepthSpaceLayer(depthSpace, gap.backLayerId, backPlane) ||
                    !findDepthSpaceLayer(depthSpace, gap.frontLayerId, frontPlane)) {
                    continue;
                }
                auto point = gapMarkerPoint(backPlane, frontPlane);
                auto line = renderer.buildDepthLine(
                    point,
                    gap.backDepth,
                    point,
                    gap.frontDepth,
                    depthDisplayScale,
                    viewSession.camera
                );
                if (gap.overlap) geometry.overlapGapLines ~= line;
                else geometry.gapLines ~= line;
                DepthDrawViewportGapHandle handle;
                handle.backLayerId = gap.backLayerId;
                handle.frontLayerId = gap.frontLayerId;
                handle.overlap = gap.overlap;
                handle.documentPoint = point;
                handle.backDepth = gap.backDepth;
                handle.frontDepth = gap.frontDepth;
                handle.renderPoint = renderer.buildPoint(
                    point,
                    (gap.backDepth + gap.frontDepth) * 0.5f,
                    depthDisplayScale,
                    viewSession.camera,
                    7.0f
                );
                geometry.gapHandles ~= handle;
                DepthDrawViewportTextLabel label;
                label.backLayerId = gap.backLayerId;
                label.frontLayerId = gap.frontLayerId;
                label.overlap = gap.overlap;
                label.point = handle.renderPoint.point;
                label.text = gap.overlap
                    ? "Overlap: " ~ gap.backLayerId ~ " / " ~ gap.frontLayerId
                    : "Gap: " ~ gap.backLayerId ~ " -> " ~ gap.frontLayerId;
                geometry.textLabels ~= label;
            }
        }
        return geometry;
    }

    DepthDrawViewportMeshRenderPlan[] collectMeshRenderPlan(ref DepthDrawViewportRenderGeometry geometry) {
        DepthDrawViewportMeshRenderPlan[] plans;
        foreach (i, mesh; geometry.targetMeshes) {
            DepthDrawViewportMeshRenderPlan plan;
            plan.hasMesh = mesh.positions.length > 0 && mesh.indices.length > 0;
            if (i < geometry.targetMeshGridUuids.length) {
                plan.targetGridUuid = geometry.targetMeshGridUuids[i];
                auto target = viewSession.targetByGrid(plan.targetGridUuid);
                plan.hasTarget = target !is null && target.getTarget() !is null;
                if (plan.hasTarget) {
                    plan.drawableChildren = DepthTargetOffscreenTextureRenderer.drawableChildren(target.getTarget()).length;
                }
            }
            plan.willAttemptTexture = plan.hasMesh && plan.hasTarget && plan.drawableChildren > 0;
            plans ~= plan;
        }
        return plans;
    }

    override void drawOptions() {
        if (drawSession is null) return;
        auto display = drawSession.display;
        bool changed;
        changed = ngCheckbox(__("Composite"), &display.showComposite) || changed;
        changed = ngCheckbox(__("Layer Planes"), &display.showLayerPlanes) || changed;
        changed = ngCheckbox(__("Depth Ranges"), &display.showDepthRanges) || changed;
        changed = ngCheckbox(__("Missing Vertices"), &display.showMissingVertices) || changed;
        changed = ngCheckbox(__("Winning Layer"), &display.showWinningLayer) || changed;
        changed = ngCheckbox(__("Normal Image"), &display.showNormalImage) || changed;
        changed = ngCheckbox(__("Raw Depth"), &display.showRawDepth) || changed;
        changed = ngCheckbox(__("Coverage"), &display.showCoverage) || changed;
        changed = ngCheckbox(__("GPU Preview"), &display.useGpuPreview) || changed;
        if (changed) drawSession.updateDisplayOptions(display);
    }

    override void drawConfirmBar() {
        if (drawSession is null || !drawSession.display.showDepthRanges) return;
        auto geometry = collectRenderGeometry();
        foreach (i, label; geometry.textLabels) {
            auto viewportPoint = WorldToViewport(label.point.x, label.point.y);
            incBeginViewportToolArea(
                "DepthDrawLabel" ~ i.to!string,
                ImVec2(viewportPoint.x + 8.0f, viewportPoint.y - 8.0f),
                true
            );
            incTextLabel(label.text);
            incEndViewportToolArea();
        }
    }

    override void present() {
        syncTargets();
    }

    override void withdraw() {
        previewResults = null;
        pendingGpuPreviewJobs = null;
        gpuPreviewErrorMessage = null;
        viewSession.clear();
    }

    override void selectionChanged(Node[] selection) {
        this.selection = selection;
        syncTargets();
    }

    override void draw(Camera camera) {
        composeDirtyPreviews();
        auto geometry = collectRenderGeometry();
        drawRenderGeometry(geometry, camera);
    }

    override void update(ImGuiIO* io, Camera camera) {
        composeSelectedPreviewForUpdate();
        if (drawSession !is null && incInputIsMouseClicked(ImGuiMouseButton.Left)) {
            auto point = -incInputGetMousePosition();
            if (!selectTargetAtDepthViewPoint(point)) {
                selectLayerPlaneAtDepthViewPoint(point);
            }
        }
    }
}
