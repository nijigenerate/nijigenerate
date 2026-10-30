#include "viewport.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/source_allocator.hpp>
#include <nodes/deformable.hpp>
#include <cmath>

namespace nijigenerate::viewport::depth::draw {
namespace {
#if defined(_WIN32)
using ViewportReal = double;
#else
using ViewportReal = long double;
#endif
template<class T> std::shared_ptr<T> allocateViewportObject() {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{});
}
}
DepthDrawViewportRenderGeometryStats DepthDrawViewportRenderGeometry::stats() const {
    DepthDrawViewportRenderGeometryStats result;
    result.normalImageLines = normalImageLines.size();
    result.rawDepthLines = rawDepthLines.size();
    result.coverageLines = coverageLines.size();
    result.coveragePoints = coveragePoints.size();
    result.targetMeshes = targetMeshes.size();
    for (auto mesh : targetMeshes) result.targetSurfaceTriangles += mesh.indices.size() / 3;
    result.targetMeshGridUuids = targetMeshGridUuids.size();
    result.targetLines = targetLines.size();
    result.layerPlaneLines = layerPlaneLines.size();
    result.selectedLayerLines = selectedLayerLines.size();
    result.depthRangeLines = depthRangeLines.size();
    result.gapLines = gapLines.size();
    result.overlapGapLines = overlapGapLines.size();
    result.gapHandles = gapHandles.size();
    result.textLabels = textLabels.size();
    result.missingPoints = missingPoints.size();
    result.winningPoints = winningPoints.size();
    result.winningPointGroups = winningPointGroups.size();
    return result;
}
std::size_t ngDepthDrawCoverageOverlayStride(std::size_t pixelCount) {
    if (pixelCount <= DepthDrawMaxCoverageOverlayPoints) return 1;
    return 1 + (pixelCount - 1) / DepthDrawMaxCoverageOverlayPoints;
}
void DepthDrawViewport::cancelPendingGpuPreviews() {
    const auto receiver = weak_from_this().lock();
    const auto keys = pendingGpuPreviewJobs.keys();
    for (auto gridUuid : keys) {
        auto pending = pendingGpuPreviewJobs.find(gridUuid);
        if (pending != pendingGpuPreviewJobs.end()) ngCancelDepthDrawGpuTargetCompose(pending->second.job);
    }
    pendingGpuPreviewJobs = {};
}
DepthDrawViewport::DepthDrawViewport(std::shared_ptr<DepthDrawSession> session) {
    prepareSourceClassInfo();
    viewSession = allocateViewportObject<common::session::DepthViewSession>();
    drawSession = session ? session : allocateViewportObject<DepthDrawSession>();
    renderer = allocateViewportObject<DepthTargetRenderer>();
    meshRenderer = allocateViewportObject<DepthTextureMeshRenderer>();
    offscreenTextureRenderer = allocateViewportObject<DepthTargetOffscreenTextureRenderer>();
}
std::shared_ptr<common::session::DepthViewSession> DepthDrawViewport::depthViewSession() { return viewSession; }
std::shared_ptr<DepthDrawSession> DepthDrawViewport::depthDrawSession() { return drawSession; }
std::shared_ptr<DepthTargetRenderer> DepthDrawViewport::targetRenderer() { return renderer; }
void DepthDrawViewport::setDepthDrawSession(std::shared_ptr<DepthDrawSession> session) {
    const auto receiver = weak_from_this().lock();
    cancelPendingGpuPreviews();
    previewResults = {};
    drawSession = session ? session : allocateViewportObject<DepthDrawSession>();
    syncTargets();
}
void DepthDrawViewport::setDocumentSize(std::int32_t width, std::int32_t height) {
    const auto receiver = weak_from_this().lock();
    documentWidth = width > 0 ? width : 1;
    documentHeight = height > 0 ? height : 1;
    if (drawSession) compat::ngRequireObject(drawSession)->markAllPreviewDirty();
}
DepthDrawComposeResult DepthDrawViewport::previewResult(std::uint64_t gridUuid) {
    const auto result = previewResults.find(gridUuid);
    return result == previewResults.end() ? DepthDrawComposeResult{} : result->second;
}
DepthDrawComposeResult DepthDrawViewport::composePreview(std::uint64_t gridUuid) {
    const auto receiver = weak_from_this().lock();
    if (!drawSession) return {};
    auto target = compat::ngRequireObject(viewSession)->targetByGrid(gridUuid);
    if (!target) return {};
    auto result = ngPreviewDepthDrawTarget(drawSession, target, documentWidth, documentHeight);
    previewResults[gridUuid] = result;
    return result;
}
bool DepthDrawViewport::composeSelectedPreviewForUpdate() {
    const auto receiver = weak_from_this().lock();
    if (!drawSession || compat::ngRequireObject(drawSession)->display.useGpuPreview ||
        !compat::ngRequireObject(drawSession)->isTargetPreviewDirty(
            compat::ngRequireObject(viewSession)->selectedGridUuid)) return false;
    composePreview(compat::ngRequireObject(viewSession)->selectedGridUuid);
    return true;
}
compat::RetainedString DepthDrawViewport::gpuPreviewError() const { return gpuPreviewErrorMessage; }
std::size_t DepthDrawViewport::pendingGpuPreviewJobCount() const { return pendingGpuPreviewJobs.size(); }
bool DepthDrawViewport::submitGpuPreview(std::uint64_t gridUuid) {
    const auto receiver = weak_from_this().lock();
    if (!drawSession || pendingGpuPreviewJobs.contains(gridUuid)) return false;
    auto target = compat::ngRequireObject(viewSession)->targetByGrid(gridUuid);
    if (!target) return false;
    DepthDrawGpuPreviewPendingJob pending;
    pending.session = drawSession;
    pending.revision = compat::ngRequireObject(drawSession)->targetPreviewRevision(gridUuid);
    compat::RetainedString error;
    if (!ngSubmitDepthDrawGpuTargetCompose(drawSession, target, documentWidth, documentHeight, pending.job, error)) {
        gpuPreviewErrorMessage = error.empty() ?
            compat::RetainedString::fromStaticLiteral("DepthDraw GPU preview submit failed") : error;
        return false;
    }
    pendingGpuPreviewJobs[gridUuid] = pending;
    gpuPreviewErrorMessage = {};
    return true;
}
std::size_t DepthDrawViewport::pollGpuPreviews() {
    const auto receiver = weak_from_this().lock();
    std::size_t completed = 0;
    const auto keys = pendingGpuPreviewJobs.keys();
    for (auto gridUuid : keys) {
        auto found = pendingGpuPreviewJobs.find(gridUuid);
        if (found == pendingGpuPreviewJobs.end()) continue;
        auto& pending = found->second;
        DepthDrawGpuTargetComposePollResult pollResult;
        compat::RetainedString error;
        if (!ngPollDepthDrawGpuTargetCompose(pending.job, pollResult, error)) {
            gpuPreviewErrorMessage = error.empty() ?
                compat::RetainedString::fromStaticLiteral("DepthDraw GPU preview poll failed") : error;
            ngCancelDepthDrawGpuTargetCompose(pending.job);
            pendingGpuPreviewJobs.erase(gridUuid);
            continue;
        }
        if (!pollResult.ready) continue;
        bool current = false;
        if (pending.session == drawSession && drawSession) {
            const auto revision = pending.revision;
            const auto currentRevision = compat::ngRequireObject(drawSession)->targetPreviewRevision(gridUuid);
            current = revision == currentRevision;
        }
        pendingGpuPreviewJobs.erase(gridUuid);
        if (!current) continue;
        previewResults[gridUuid] = pollResult.result;
        auto target = compat::ngRequireObject(viewSession)->targetByGrid(gridUuid);
        if (target && !pollResult.result.depths.empty()) target->replaceWorkingDepths(pollResult.result.depths);
        if (drawSession) compat::ngRequireObject(drawSession)->clearTargetPreviewDirty(gridUuid);
        ++completed;
    }
    return completed;
}
std::size_t DepthDrawViewport::composeDirtyGpuPreviews() {
    const auto receiver = weak_from_this().lock();
    if (!drawSession) return 0;
    auto completed = pollGpuPreviews();
    for (auto gridUuid : compat::ngRequireObject(drawSession)->dirtyTargetGridIds()) {
        if (!compat::ngRequireObject(viewSession)->targetByGrid(gridUuid)) continue;
        if (submitGpuPreview(gridUuid)) ++completed;
    }
    return completed;
}
bool DepthDrawViewport::selectLayerPlaneAtDocumentPoint(Vec2 point) {
    const auto receiver = weak_from_this().lock();
    if (!drawSession) return false;
    const auto selected = compat::ngRequireObject(drawSession)->selectLayerPlaneAtDocumentPoint(point);
    if (selected && compat::ngRequireObject(viewSession)->targetByGrid(
            compat::ngRequireObject(drawSession)->selectedGridUuid))
        compat::ngRequireObject(viewSession)->selectTarget(compat::ngRequireObject(drawSession)->selectedGridUuid);
    return selected;
}
DepthDrawViewport::Vec2 DepthDrawViewport::depthViewPointToDocumentPoint(Vec2 point, float representativeDepth) {
    const auto receiver = weak_from_this().lock();
    const auto displayDepth = -representativeDepth * selectedDepthDisplayScale();
    const auto camera = compat::ngRequireObject(viewSession)->camera;
    const auto modelPoint = unprojectDepthPoint(point, displayDepth, camera);
    return {modelPoint.x + static_cast<float>(documentWidth) * .5f,
        modelPoint.y + static_cast<float>(documentHeight) * .5f};
}
bool DepthDrawViewport::selectLayerPlaneAtDepthViewPoint(Vec2 point, float representativeDepth) {
    const auto receiver = weak_from_this().lock();
    return selectLayerPlaneAtDocumentPoint(depthViewPointToDocumentPoint(point, representativeDepth));
}
bool DepthDrawViewport::selectTargetAtDepthViewPoint(Vec2 point, float radius) {
    const auto receiver = weak_from_this().lock();
    if (!drawSession) return false;
    std::shared_ptr<common::targetview::DepthTargetView> bestTarget;
    float bestDistance = radius;
    const auto targets = compat::ngRequireObject(viewSession)->targets;
    for (auto target : targets) {
        if (!target || !target->getTarget()) continue;
        const auto mesh = compat::ngRequireObject(renderer)->buildMesh(target,
            compat::ngRequireObject(viewSession)->camera);
        for (auto projected : mesh.positions) {
            const float dx = projected.x - point.x, dy = projected.y - point.y;
            ViewportReal squared = 0;
            squared += dx * dx;
            squared += dy * dy;
            const ViewportReal distance = std::sqrt(squared);
            if (distance > bestDistance) continue;
            bestDistance = static_cast<float>(distance);
            bestTarget = target;
        }
    }
    if (!bestTarget || !bestTarget->getTarget()) return false;
    const auto gridUuid = compat::ngRequireObject(bestTarget->getTarget())->getUUID();
    if (!compat::ngRequireObject(drawSession)->selectTargetGrid(gridUuid)) return false;
    return compat::ngRequireObject(viewSession)->selectTarget(gridUuid);
}
std::size_t DepthDrawViewport::composeDirtyPreviews() {
    const auto receiver = weak_from_this().lock();
    if (!drawSession) return 0;
    if (compat::ngRequireObject(drawSession)->display.useGpuPreview) return composeDirtyGpuPreviews();
    cancelPendingGpuPreviews();
    std::size_t composed = 0;
    for (auto gridUuid : compat::ngRequireObject(drawSession)->dirtyTargetGridIds()) {
        if (!compat::ngRequireObject(viewSession)->targetByGrid(gridUuid)) continue;
        composePreview(gridUuid);
        ++composed;
    }
    return composed;
}
}
