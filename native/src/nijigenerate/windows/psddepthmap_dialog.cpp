#include "psddepthmap.hpp"
#include "detail/psddepthmap_helpers.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include "detail/psddepthmap_state.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/viewport/depth/draw/gpu.hpp>
#include <nodes/deformable.hpp>
#include <puppet.hpp>
#include <nijigenerate/compat/node_class_name.hpp>
#include <algorithm>

namespace nijigenerate::windows::psddepthmap {
using namespace commands::depth::map;
using viewport::depth::draw::ngDepthDrawGpuLayerSampleSupportsConvolution;
bool PSDDepthMapWindow::dialogCommandsAvailable() {
    const auto keepAlive = weak_from_this().lock();
    if (dialogDisplayed && detail::activePsdDepthMapWindow.get() == this) ensureDialogActionScope();
    return dialogDisplayed && dialogActionScope != nullptr && compat::ngRequireObject(dialogActionScope)->isActive();
}

bool PSDDepthMapWindow::applyDialogResult() {
    const auto keepAlive = weak_from_this().lock();
    return apply();
}

void PSDDepthMapWindow::cancelDialog() {
    const auto keepAlive = weak_from_this().lock();
    cancelPendingPreviewGpuComposition();
    deactivateDialogCommandContext();
    closeDialogActionScope();
    close();
}

compat::Slice<NodeRef> PSDDepthMapWindow::dialogContextNodesForLayer(compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
    const auto keepAlive = weak_from_this().lock();
    auto puppet = project::incActivePuppet();
    if (puppet == nullptr || puppet->root == nullptr) return {};

    std::uint64_t nodeUuid = targetGridUuid;
    if (auto mapping = findMapping(layerPath, targetGridUuid)) {
        if (mapping->matchedNodeUuid != 0) nodeUuid = mapping->matchedNodeUuid;
    }
    if (nodeUuid == 0) return {};
    auto node = puppet->findNodeById(static_cast<std::uint32_t>(nodeUuid));
    return node ? compat::Slice<NodeRef>{node} : compat::Slice<NodeRef>{};
}

compat::Slice<NodeRef> PSDDepthMapWindow::selectedDialogContextNodes() {
    const auto keepAlive = weak_from_this().lock();
    auto layer = selected3DAdjustLayerPreview();
    if (layer != nullptr) {
        auto nodes = dialogContextNodesForLayer(layer->layerPath, layer->targetGridUuid);
        if (nodes.size()) return nodes;
    }
    if (selectedGridIndex >= 0 && selectedGridIndex < preview.grids.size()) {
        auto grid = compat::ngArrayAt(preview.grids, static_cast<std::size_t>(selectedGridIndex)).grid;
        if (grid != nullptr) return {std::static_pointer_cast<nicxlive::core::nodes::Node>(grid)};
    }
    return {};
}

bool PSDDepthMapWindow::dialogContextMatchesLayer(ContextRef ctx, compat::RetainedString layerPath, std::uint64_t targetGridUuid) {
    const auto keepAlive = weak_from_this().lock();
    if (ctx == nullptr || !ctx->hasNodes() || ctx->nodes().size() == 0) return false;
    auto expected = dialogContextNodesForLayer(layerPath, targetGridUuid);
    if (expected.size() == 0) return false;
    for (auto selected : ctx->nodes()) {
        if (selected == compat::ngArrayAt(expected, 0)) return true;
    }
    return false;
}

bool PSDDepthMapWindow::captureDialogContextLayerState(
    ContextRef ctx,
    compat::RetainedString layerPath,
    PsdDepthDialogLayerState& state
) {
    state = {};
    const auto keepAlive = weak_from_this().lock();
    for (auto layers = preview.composedLayers; auto& layer : layers) {
        if (layer.layerPath != layerPath ||
            !dialogContextMatchesLayer(ctx, layer.layerPath, layer.targetGridUuid)) {
            continue;
        }
        state = dialogLayerState(layer);
        return true;
    }
    state = PsdDepthDialogLayerState{};
    return false;
}

bool PSDDepthMapWindow::dialogMappingTargetAvailable(NodeRef target) {
    const auto keepAlive = weak_from_this().lock();
    if (target == nullptr) return false;
    for (auto candidate : currentTargets()) {
        if (candidate == target) return true;
    }
    return false;
}

bool PSDDepthMapWindow::dialogContextTargetUuid(ContextRef ctx, std::uint64_t& uuid) {
    uuid = 0;
    const auto keepAlive = weak_from_this().lock();
    if (ctx == nullptr || !ctx->hasNodes() || ctx->nodes().size() != 1) return false;
    {
        auto nodes = ctx->nodes();
        if (!dialogMappingTargetAvailable(compat::ngArrayAt(nodes, 0))) return false;
    }
    auto nodes = ctx->nodes();
    uuid = compat::ngRequireObject(compat::ngArrayAt(nodes, 0))->getUUID();
    return true;
}

PsdDepthDialogSettingsState PSDDepthMapWindow::captureDialogSettingsState() {
    const auto keepAlive = weak_from_this().lock();
    PsdDepthDialogSettingsState result;
    result.settings = cloneDialogSettings(settings);
    result.onlyProblemLayers = onlyProblemLayers;
    for (auto layers = preview.composedLayers; auto& layer : layers) {
        result.layers.append(dialogLayerState(layer));
    }
    return result;
}

bool PSDDepthMapWindow::captureDialogOverallPreview(
    PsdDepthDialogOverallPreview& result,
    compat::RetainedString& error
) {
    const auto keepAlive = weak_from_this().lock();
    result = PsdDepthDialogOverallPreview{};
    error = nullptr;
    if (previewDirty) rebuildPreview();
    if (previewCompositionPending()) {
        error = detail::helpers::tr("GPU preview composition is still running.");
        return false;
    }
    if (errorMessage.size()) {
        error = errorMessage;
        return false;
    }

    if (!buildThreeDAdjustOverallPreview(640, 480, result)) {
        error = compat::RetainedString::fromStaticLiteral("Failed to render the PSD depth dialog overall preview");
        return false;
    }
    result.yaw = threeDAdjustCamera.yaw;
    result.pitch = threeDAdjustCamera.pitch;
    result.zoom = threeDAdjustCamera.zoom;
    result.panX = threeDAdjustCamera.pan.x;
    result.panY = threeDAdjustCamera.pan.y;
    return true;
}

bool PSDDepthMapWindow::captureDialogContextPartData(
    ContextRef ctx,
    compat::Slice<PsdDepthDialogPartData>& result,
    compat::RetainedString& error
) {
    result = {};
    error = nullptr;
    const auto keepAlive = weak_from_this().lock();
    if (ctx == nullptr || !ctx->hasNodes() || ctx->nodes().size() == 0) {
        error = compat::RetainedString::fromStaticLiteral(
            "Select one or more PSD depth dialog parts through Context.nodes");
        return false;
    }
    if (previewDirty) rebuildPreview();
    if (previewCompositionPending()) {
        error = detail::helpers::tr("GPU preview composition is still running.");
        return false;
    }
    if (errorMessage.size()) {
        error = errorMessage;
        return false;
    }

    for (auto grids = preview.grids; auto& gridResult : grids) {
        if (gridResult.grid == nullptr) continue;

        bool selected = false;
        for (auto node : ctx->nodes()) {
            if (node == std::static_pointer_cast<nicxlive::core::nodes::Node>(gridResult.grid)) {
                selected = true;
                break;
            }
        }
        if (!selected) {
            for (auto layers = preview.composedLayers; auto& layer : layers) {
                const auto targetGridUuid = layer.targetGridUuid;
                if (targetGridUuid != compat::ngRequireObject(gridResult.grid)->getUUID()) continue;
                if (dialogContextMatchesLayer(ctx, layer.layerPath, layer.targetGridUuid)) {
                    selected = true;
                    break;
                }
            }
        }
        if (!selected) continue;

        PsdDepthDialogPartData part;
        part.targetGridUuid = compat::ngRequireObject(gridResult.grid)->getUUID();
        part.targetGridName = compat::RetainedString(compat::ngRequireObject(gridResult.grid)->getName());
        part.targetType = compat::ngNodeDynamicClassName(*compat::ngRequireObject(gridResult.grid));
        part.skipped = gridResult.skipped;
        part.documentWidth = gridResult.documentWidth;
        part.documentHeight = gridResult.documentHeight;
        part.coverageSources = gridResult.coverageSources;
        part.sampledVertices = gridResult.sampledVertices;
        part.missingVertices = gridResult.missingVertices;
        part.minDepth = gridResult.minDepth;
        part.maxDepth = gridResult.maxDepth;
        {
            const auto vertexOwner = compat::ngRequireObject(gridResult.grid);
            const auto vertices = vertexOwner->getVertices();
            const auto vertexCount = vertices.size();
            for (std::size_t i = 0; i < vertexCount; ++i) {
                part.vertexX.append(compat::ngArrayAt(vertices, i).x);
                part.vertexY.append(compat::ngArrayAt(vertices, i).y);
            }
        }
        part.depths = gridResult.depths;
        part.baseDepths = gridResult.baseDepths;
        part.winnerLayerPaths = gridResult.winnerLayerPaths;
        part.missingVertexMask = gridResult.missingVertexMask;
        part.previewLeft = gridResult.previewLeft;
        part.previewTop = gridResult.previewTop;
        part.previewWidth = gridResult.previewWidth;
        part.previewHeight = gridResult.previewHeight;
        part.previewRgba = gridResult.rawCompositePreviewRgba;

        for (auto layers = preview.composedLayers; auto& layer : layers) {
            const auto targetGridUuid = layer.targetGridUuid;
            if (targetGridUuid != compat::ngRequireObject(gridResult.grid)->getUUID()) continue;
            PsdDepthDialogPartLayerData layerData;
            layerData.layerPath = layer.layerPath;
            layerData.layerName = layer.layerName;
            layerData.colorLayerPath = layer.colorLayerPath;
            layerData.colorLayerName = layer.colorLayerName;
            layerData.sourcePath = layer.sourcePath;
            layerData.left = layer.left;
            layerData.top = layer.top;
            layerData.width = layer.width;
            layerData.height = layer.height;
            layerData.visible = layer.visible;
            layerData.enabled = layer.enabled;
            layerData.depthEnabled = layer.depthEnabled;
            layerData.targetGridUuid = layer.targetGridUuid;
            layerData.depthStats = layer.depthStats;
            layerData.colorRgba = layer.colorRgba;
            layerData.depthRgba = layer.depthRgba;
            part.layers.append(layerData);
        }
        result.append(part);
    }

    if (result.size() == 0) {
        error = compat::RetainedString::fromStaticLiteral(
            "Context.nodes does not select a part loaded by the PSD depth dialog");
        return false;
    }
    return true;
}

bool PSDDepthMapWindow::applyDialogSettingsState(PsdDepthDialogSettingsState state) {
    const auto keepAlive = weak_from_this().lock();
    settings = cloneDialogSettings(state.settings);
    if (settings.useGpuComposition && !ngDepthDrawGpuLayerSampleSupportsConvolution(
        static_cast<int>(ngPsdDepthConvolutionToDepthImage(settings.convolution)))) {
        settings.convolution = PsdDepthConvolution::Median3x3;
    }
    onlyProblemLayers = state.onlyProblemLayers;
    pendingLayerStatesAfterRebuild = state.layers.duplicate();
    previewDirty = true;
    return true;
}

bool PSDDepthMapWindow::captureDialogLayerState(
    compat::RetainedString layerPath,
    std::uint64_t targetGridUuid,
    PsdDepthDialogLayerState& state
) {
    state = {};
    const auto keepAlive = weak_from_this().lock();
    if (auto layer = findComposedLayer(layerPath, targetGridUuid)) {
        state = dialogLayerState(*layer);
        return true;
    }
    state = PsdDepthDialogLayerState{};
    return false;
}

bool PSDDepthMapWindow::applyDialogLayerState(PsdDepthDialogLayerState state, bool stageForApply) {
    const auto keepAlive = weak_from_this().lock();
    const auto layerStorage = preview.composedLayers;
    auto layer = findComposedLayer(state.layerPath, state.targetGridUuid);
    if (layer == nullptr) return false;
    auto previous = dialogLayerState(*layer);
    if (sameDialogLayerState(previous, state)) return false;

    applyDialogLayerStateFields(*layer, state);
    if (stageForApply) {
        stage3DAdjustLayerChange(state.layerPath, state.targetGridUuid);
        if (state.invert != previous.invert) {
            invalidate3DAdjustLayerCaches(state.layerPath, state.targetGridUuid);
        }
        if (preview.gpuCompositionRequested) {
            startPreviewComposition();
            pending3DAdjustLayerChanges = {};
        }
    } else {
        refreshAfter3DAdjustLayerChange(state.layerPath, state.targetGridUuid);
    }
    return true;
}

compat::Slice<PsdDepthDialogLayerPixels> PSDDepthMapWindow::captureDialogLayerPixels() {
    compat::Slice<PsdDepthDialogLayerPixels> result;
    const auto keepAlive = weak_from_this().lock();
    auto layers = preview.composedLayers;
    for (auto& layer : layers) {
        PsdDepthDialogLayerPixels pixels;
        pixels.layerPath = layer.layerPath;
        pixels.targetGridUuid = layer.targetGridUuid;
        pixels.depthRgba = layer.depthRgba.duplicate();
        const auto key = cleanupLayerKey(layer.layerPath, layer.targetGridUuid);
        if (auto applied = detail::helpers::lookup(alphaDepthGapFillAppliedByLayer,
            compat::RetainedStringSlice(key))) {
            pixels.alphaDepthGapFillApplied = *applied;
        }
        result.append(pixels);
    }
    return result;
}

bool PSDDepthMapWindow::applyDialogLayerPixels(compat::Slice<PsdDepthDialogLayerPixels> state) {
    bool found = false;
    const auto keepAlive = weak_from_this().lock();
    for (auto pixels : state) {
        const auto layerStorage = preview.composedLayers;
        auto layer = findComposedLayer(pixels.layerPath.retainedString(), pixels.targetGridUuid);
        if (layer == nullptr) continue;
        layer->depthRgba = pixels.depthRgba.duplicate();
        alphaDepthGapFillAppliedByLayer[compat::RetainedStringSlice(
            cleanupLayerKey(pixels.layerPath.retainedString(), pixels.targetGridUuid))] =
            pixels.alphaDepthGapFillApplied;
        found = true;
    }
    if (!found && state.size() > 0) return false;

    disposePreviewTextures();
    if (!startPreviewComposition()) return false;
    previewDirty = false;
    return true;
}

bool PSDDepthMapWindow::applyDialogAlphaDepthGapFill() {
    const auto keepAlive = weak_from_this().lock();
    if (previewCompositionPending()) return false;
    auto oldPixels = captureDialogLayerPixels();
    auto oldApplied = detail::helpers::duplicateCache(alphaDepthGapFillAppliedByLayer);
    auto oldComposedPreview = composedPreview;
    applyAlphaDepthGapFill();
    if (errorMessage.size() == 0) return true;

    for (auto pixels : oldPixels) {
        const auto layerStorage = preview.composedLayers;
        auto layer = findComposedLayer(pixels.layerPath.retainedString(), pixels.targetGridUuid);
        if (layer != nullptr) layer->depthRgba = pixels.depthRgba.duplicate();
    }
    alphaDepthGapFillAppliedByLayer = oldApplied;
    composedPreview = oldComposedPreview;
    previewDirty = false;
    return false;
}

bool PSDDepthMapWindow::dialogLayerPixelsEqual(
    compat::Slice<PsdDepthDialogLayerPixels> a,
    compat::Slice<PsdDepthDialogLayerPixels> b
) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (compat::ngArrayAt(a, i).layerPath != compat::ngArrayAt(b, i).layerPath ||
            compat::ngArrayAt(a, i).targetGridUuid != compat::ngArrayAt(b, i).targetGridUuid ||
            compat::ngArrayAt(a, i).depthRgba.size() != compat::ngArrayAt(b, i).depthRgba.size() ||
            !std::equal(compat::ngArrayAt(a, i).depthRgba.begin(), compat::ngArrayAt(a, i).depthRgba.end(),
                compat::ngArrayAt(b, i).depthRgba.begin()) ||
            compat::ngArrayAt(a, i).alphaDepthGapFillApplied != compat::ngArrayAt(b, i).alphaDepthGapFillApplied) {
            return false;
        }
    }
    return true;
}
}
