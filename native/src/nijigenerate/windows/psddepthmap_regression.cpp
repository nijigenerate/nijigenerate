#include "psddepthmap.hpp"
#include <nijigenerate/ext/nodes/exdepthmapped.hpp>
#include <nijigenerate/io/depthmap_psd_names.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <algorithm>
#include <cmath>
#include <limits>
namespace nijigenerate::windows::psddepthmap {
using namespace commands::depth::map;
namespace {
template<class... T> std::string format(std::string_view pattern, T&&... values) {
    const std::array<core::translation::TypedFormatArgument, sizeof...(T)> args{
        core::translation::ngFormatArgument(std::forward<T>(values))...};
    return core::translation::ngFormatTyped(pattern, args);
}
}
#if defined(NG_COMMAND_BROWSER_DIFFERENTIAL)
void PSDDepthMapWindow::beginDialogCommandSessionForRegression() {
    dialogDisplayed = true;
    activateDialogCommandContext();
    ensureDialogActionScope();
}

bool PSDDepthMapWindow::dialogCommandScopeActiveForRegression() {
    return dialogCommandScope != nullptr && dialogCommandScope->isActive();
}

bool PSDDepthMapWindow::dialogActionScopeActiveForRegression() {
    return dialogActionScope != nullptr && dialogActionScope->isActive();
}

void PSDDepthMapWindow::setDialogLayerStateForRegression(PsdDepthDialogLayerState state) {
    PsdDepthComposedLayer layer;
    layer.id = state.layerPath;
    layer.layerPath = state.layerPath;
    layer.targetGridUuid = state.targetGridUuid;
    applyDialogLayerStateFields(layer, state);
    preview.composedLayers = {layer};
    previewDirty = false;
}

bool PSDDepthMapWindow::setDialogPartDataForRegression(
    DeformableRef target,
    int width,
    int height,
    compat::Slice<std::uint8_t> colorRgba,
    compat::Slice<std::uint8_t> depthRgba,
    compat::Slice<std::uint8_t> previewRgba,
    compat::Slice<float> depths
) {
    if (target == nullptr || preview.composedLayers.size() != 1 ||
        width <= 0 || height <= 0) return false;
    auto expectedLength = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    if (colorRgba.size() != expectedLength ||
        depthRgba.size() != expectedLength ||
        previewRgba.size() != expectedLength) return false;

    const auto layerStorage = preview.composedLayers;
    auto layer = &preview.composedLayers[0];
    layer->targetGridUuid = compat::ngRequireObject(target)->getUUID();
    layer->targetGridName = compat::RetainedString(compat::ngRequireObject(target)->getName());
    layer->layerName = "Regression Layer";
    layer->colorLayerPath = layer->layerPath;
    layer->colorLayerName = layer->layerName;
    layer->sourcePath = path;
    layer->width = width;
    layer->height = height;
    layer->colorRgba = colorRgba.duplicate();
    layer->depthRgba = depthRgba.duplicate();
    layer->maskRgba.resize(expectedLength);
    for (std::size_t i = 0; i < static_cast<std::size_t>(width) * static_cast<std::size_t>(height); ++i) {
        auto offset = i * 4;
        layer->maskRgba[offset] = 255;
        layer->maskRgba[offset + 1] = 0;
        layer->maskRgba[offset + 2] = 0;
        layer->maskRgba[offset + 3] = 255;
    }
    layer->depthStats.hasDepth = depths.size() > 0;
    layer->depthStats.maskedPixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);

    PsdDepthGridResult gridResult;
    gridResult.grid = target;
    gridResult.depths = depths.duplicate();
    gridResult.baseDepths.resize(depths.size(), std::numeric_limits<float>::quiet_NaN());
    gridResult.winnerLayerPaths.resize(depths.size());
    std::fill(gridResult.winnerLayerPaths.begin(), gridResult.winnerLayerPaths.end(), layer->layerPath);
    gridResult.missingVertexMask.resize(depths.size());
    gridResult.documentWidth = width;
    gridResult.documentHeight = height;
    gridResult.coverageSources = 1;
    gridResult.sampledVertices = depths.size();
    gridResult.minDepth = depths.size() ? depths[0] : 0;
    gridResult.maxDepth = depths.size() ? depths[0] : 0;
    for (auto depth : depths) {
        gridResult.minDepth = std::min(gridResult.minDepth, depth);
        gridResult.maxDepth = std::max(gridResult.maxDepth, depth);
    }
    gridResult.previewWidth = width;
    gridResult.previewHeight = height;
    gridResult.rawCompositePreviewRgba = previewRgba.duplicate();
    gridResult.compositePreviewRgba = previewRgba.duplicate();
    PsdDepthGridLayerMask layerMask;
    layerMask.layerPath = layer->layerPath;
    layerMask.layerName = layer->layerName;
    layerMask.sampledVertices = depths.size();
    layerMask.selectedVertices = depths.size();
    gridResult.layerMasks = {layerMask};
    preview.grids = {gridResult};
    previewDirty = false;
    return true;
}

void PSDDepthMapWindow::clearDialogPartDataForRegression() {
    preview.grids = {};
    previewDirty = false;
}

bool PSDDepthMapWindow::setDialogLayerPixelsForRegression(
    int width,
    int height,
    compat::Slice<std::uint8_t> depthRgba,
    compat::Slice<std::uint8_t> maskRgba
) {
    if (preview.composedLayers.size() != 1) return false;
    preview.composedLayers[0].width = width;
    preview.composedLayers[0].height = height;
    preview.composedLayers[0].depthRgba = depthRgba.duplicate();
    preview.composedLayers[0].maskRgba = maskRgba.duplicate();
    if (!startPreviewComposition()) return false;
    previewDirty = false;
    return true;
}

bool PSDDepthMapWindow::replayDialogCleanupAfterRebuildForRegression(compat::Slice<std::uint8_t> rebuiltDepthRgba) {
    if (preview.composedLayers.size() != 1) return false;
    preview.composedLayers[0].depthRgba = rebuiltDepthRgba.duplicate();
    replayAlphaDepthGapFills();
    return true;
}

void PSDDepthMapWindow::failNextPreviewCompositionForRegressionTest() {
    failNextPreviewCompositionForRegression = true;
}

void PSDDepthMapWindow::useCurrentPreviewForDialogApplyRegression() {
    composedPreview = ngPsdDepthComposedViewForRegression(preview);
}

bool PSDDepthMapWindow::prepareDialogApplyForRegression() {
    preview = PsdDepthImportResult{};
    pending3DAdjustLayerChanges = {};
    if (!startPreviewComposition()) return false;
    previewDirty = false;
    return true;
}

void PSDDepthMapWindow::endDialogCommandSessionForRegression() {
    dialogDisplayed = false;
    deactivateDialogCommandContext();
    closeDialogActionScope();
}
#endif

#if defined(NG_REGRESSION_SMOKE)
void PSDDepthMapWindow::rebuildPreviewForRegressionSmoke() {
    rebuildPreview();
}

compat::RetainedString PSDDepthMapWindow::loadErrorForRegressionSmoke() const {
    return errorMessage;
}

std::size_t PSDDepthMapWindow::previewGridCountForRegressionSmoke() const {
    return preview.grids.size();
}

bool PSDDepthMapWindow::remapLayerNameToGridForRegressionSmoke(compat::RetainedString layerName, std::uint64_t gridUuid) {
    for (auto mapping : preview.mappings) {
        if (ngPsdDepthLowerString(mapping.layerName) != ngPsdDepthLowerString(layerName)) continue;
        settings.layerTargetGridUuidOverrides.set(mapping.layerPath.view(), std::to_string(gridUuid));
        rebuildPreview();
        return true;
    }
    return false;
}

bool PSDDepthMapWindow::remapAnyLayerToSampledGridForRegressionSmoke(compat::RetainedString gridName, std::uint64_t gridUuid) {
    compat::Slice<compat::RetainedString> layerPaths;
    for (auto mapping : preview.mappings) {
        if (mapping.layerPath.size() == 0) continue;
        layerPaths.append(mapping.layerPath);
    }
    for (auto layerPath : layerPaths) {
        settings.layerTargetGridUuidOverrides.set(layerPath.view(), std::to_string(gridUuid));
        rebuildPreview();
        if (hasSampledPreviewGridForRegressionSmoke(gridName)) return true;
    }
    return false;
}

bool PSDDepthMapWindow::hasSampledPreviewGridForRegressionSmoke(compat::RetainedString gridName) {
    for (auto gridResult : preview.grids) {
        if (gridResult.grid == nullptr ||
            compat::RetainedString(compat::ngRequireObject(gridResult.grid)->getName()) != gridName) continue;
        return !gridResult.skipped &&
            gridResult.depths.size() > 0 &&
            gridResult.sampledVertices > 0 &&
            gridResult.previewWidth > 0 &&
            gridResult.previewHeight > 0 &&
            gridResult.rawCompositePreviewRgba.size() > 0;
    }
    return false;
}

bool PSDDepthMapWindow::has3DAdjustGeometryForRegressionSmoke(compat::RetainedString gridName) {
    for (auto grids = preview.grids; auto& gridResult : grids) {
        if (gridResult.grid == nullptr ||
            compat::RetainedString(compat::ngRequireObject(gridResult.grid)->getName()) != gridName) continue;
        auto stats = threeDAdjustGeometryStats(gridResult);
        return stats.layerPlanes > 0 &&
            stats.depthRangeLines > 0 &&
            stats.targetWireLines > 0 &&
            stats.sampledPoints + stats.missingPoints ==
                compat::ngRequireObject(gridResult.grid)->getVertices().size() &&
            stats.sampledPoints > 0;
    }
    return false;
}

compat::Slice<compat::RetainedString> PSDDepthMapWindow::diagnosticsForRegressionSmoke() {
    return diagnostics();
}

compat::RetainedString PSDDepthMapWindow::previewSummaryForRegressionSmoke() {
    compat::Slice<compat::RetainedString> lines;
    for (auto layer : preview.composedLayers) {
        lines.append(format("layer=%s bounds=%s,%s %sx%s enabled=%s depth=%s zero=%s", 
            layer.layerPath,
            layer.left,
            layer.top,
            layer.width,
            layer.height,
            layer.enabled,
            layer.depthStats.maskedPixels,
            layer.depthStats.zeroPixels));
    }
    for (auto mapping : preview.mappings) {
        lines.append(format("mapping layer=%s target=%s status=%s matched=%s manual=%s", 
            mapping.layerPath,
            mapping.targetGridName,
            mapping.status,
            mapping.matched,
            mapping.manual));
    }
    for (auto gridResult : preview.grids) {
        lines.append(format("grid=%s depths=%s sampled=%s missing=%s preview=%sx%s raw=%s skipped=%s coverage=%s", 
            gridResult.grid != nullptr
                ? compat::RetainedString(compat::ngRequireObject(gridResult.grid)->getName())
                : compat::RetainedString::fromStaticLiteral("<null>"),
            gridResult.depths.size(),
            gridResult.sampledVertices,
            gridResult.missingVertices,
            gridResult.previewWidth,
            gridResult.previewHeight,
            gridResult.rawCompositePreviewRgba.size(),
            gridResult.skipped,
            gridResult.coverageSources));
    }
    std::string joined;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i) joined += " | ";
        joined += lines[i].view();
    }
    return joined;
}

bool PSDDepthMapWindow::applyForRegressionSmoke(compat::RetainedString& message) {
    message = {};
    if (previewDirty) rebuildPreview();
    if (!compose3DAdjustChangesForApply()) {
        message = errorMessage;
        return false;
    }
    if (errorMessage.size()) {
        message = errorMessage;
        return false;
    }
    auto result = ngApplyPsdDepthImportResult(composedPreview);
    message = result->message;
    return result->succeeded;
}

void PSDDepthMapWindow::setGpuCompositionForRegressionSmoke(bool enabled) {
    settings.useGpuComposition = enabled;
    previewDirty = true;
    rebuildPreview();
}

bool PSDDepthMapWindow::setFirstMappedLayerZTransformForRegressionSmoke(compat::RetainedString gridName, float zScale, float zOffset) {
    if (previewDirty) rebuildPreview();
    for (auto grids = preview.grids; auto& gridResult : grids) {
        if (gridResult.grid == nullptr ||
            compat::RetainedString(compat::ngRequireObject(gridResult.grid)->getName()) != gridName) continue;
        for (auto layerMask : gridResult.layerMasks) {
            if (layerMask.layerPath.size() == 0) continue;
            const auto firstPath = layerMask.layerPath;
            const auto firstUuid = compat::ngRequireObject(gridResult.grid)->getUUID();
            if (auto layer = findComposedLayer(firstPath, firstUuid)) {
                threeDAdjustLayerMesh(*layer);
            }
            const auto transformPath = layerMask.layerPath;
            const auto transformUuid = compat::ngRequireObject(gridResult.grid)->getUUID();
            auto transform = layerTransform(transformPath, transformUuid);
            transform.zScale = zScale;
            transform.zOffset = zOffset;
            const auto secondPath = layerMask.layerPath;
            const auto secondUuid = compat::ngRequireObject(gridResult.grid)->getUUID();
            if (auto layer = findComposedLayer(secondPath, secondUuid)) {
                layer->depthScale = transform.zScale;
                layer->depthOffset = transform.zOffset;
            }
            const auto changePath = layerMask.layerPath;
            const auto changeUuid = compat::ngRequireObject(gridResult.grid)->getUUID();
            stage3DAdjustLayerChange(changePath, changeUuid);
            return true;
        }
    }
    return false;
}

bool PSDDepthMapWindow::hasPending3DAdjustLayerChangesForRegressionSmoke() const {
    return pending3DAdjustLayerChanges.size() > 0;
}

std::size_t PSDDepthMapWindow::last3DAdjustApplyRecomposedGridCountForRegressionSmoke() const {
    return last3DAdjustApplyRecomposedGridCount;
}

bool PSDDepthMapWindow::hasRealtime3DAdjustTransformForRegressionSmoke(compat::RetainedString gridName) {
    for (auto grids = preview.grids; auto& gridResult : grids) {
        if (gridResult.grid == nullptr ||
            compat::RetainedString(compat::ngRequireObject(gridResult.grid)->getName()) != gridName) continue;
        for (auto layerMask : gridResult.layerMasks) {
            const auto layerPath = layerMask.layerPath;
            const auto gridUuid = compat::ngRequireObject(gridResult.grid)->getUUID();
            auto layer = findComposedLayer(layerPath, gridUuid);
            if (layer == nullptr) continue;
            auto mesh = threeDAdjustLayerMesh(*layer);
            if (mesh == nullptr || mesh->vertexData.size() < 3) continue;
            auto previousDepth = mesh->vertexData[2];
            updateThreeDAdjustMeshDisplayDepths(*layer, *mesh);
            return std::abs(mesh->vertexData[2] - previousDepth) > 0.000001f;
        }
    }
    return false;
}

bool PSDDepthMapWindow::hasFrontOnlyIntersectionConstraintForRegressionSmoke() {
    return frontOnlyIntersectionConstraintDiagnosticsForRegressionSmoke() == "ok";
}

compat::RetainedString PSDDepthMapWindow::frontOnlyIntersectionConstraintDiagnosticsForRegressionSmoke() {
    const auto solidPixels = [](std::uint8_t depth) {
        compat::Slice<std::uint8_t> result;
        for (int i = 0; i < 4; ++i) result.append(compat::Slice<std::uint8_t>{depth, depth, depth, 255});
        return result;
    };
    const auto depthPixels = [](compat::Slice<std::uint8_t> values) {
        compat::Slice<std::uint8_t> result;
        for (auto value : values) result.append(compat::Slice<std::uint8_t>{value, value, value, 255});
        return result;
    };

    PsdDepthComposedLayer backLayer;
    backLayer.id = "/constraint-back";
    backLayer.layerPath = backLayer.id;
    backLayer.layerName = "Constraint Back";
    backLayer.width = 2;
    backLayer.height = 2;
    backLayer.colorRgba = solidPixels(255);
    backLayer.depthRgba = depthPixels({0, 64, 128, 192});
    backLayer.maskRgba = solidPixels(255);
    backLayer.coverageMaskRgba = solidPixels(255);

    auto frontLayer = backLayer;
    frontLayer.id = "/constraint-front";
    frontLayer.layerPath = frontLayer.id;
    frontLayer.layerName = "Constraint Front";
    frontLayer.depthRgba = solidPixels(220);

    preview = PsdDepthImportResult{};
    preview.compositionMode = PsdDepthCompositionMode::NToN;
    preview.depthSource.kind = PsdDepthCompositeSourceKind::PsdLayers;
    preview.compositionWidth = 2;
    preview.compositionHeight = 2;
    preview.composedLayers = {backLayer, frontLayer};
    threeDAdjustSamples = {};
    threeDAdjustMeshes = {};
    threeDAdjustPreparedTransforms = {};
    threeDAdjustFrontIntersections = {};
    threeDAdjustDisplayDepthDirty = {};
    threeDAdjustSamplesPrepared = false;
    prepareThreeDAdjustSamples();

    auto backFrontIndices = threeDAdjustFrontIntersectingLayerIndices(preview.composedLayers[0]);
    auto frontFrontIndices = threeDAdjustFrontIntersectingLayerIndices(preview.composedLayers[1]);
    if ((backFrontIndices.size() != 1 || backFrontIndices[0] != 1) || frontFrontIndices.size() != 0) {
        return format("front index mismatch: back=%s front=%s", 
            backFrontIndices.size(), frontFrontIndices.size());
    }

    auto samples = threeDAdjustLayerSamples(preview.composedLayers[0]);
    if (samples.size() == 0) return "back layer has no samples";
    auto sample = samples[samples.size() - 1];
    auto initialDepth = sample.depth;
    preview.composedLayers[0].depthScale = 200.0f;
    auto unconstrained = threeDAdjustDisplayDepth(preview.composedLayers[0], sample.sourceDepth);
    auto constrained = threeDAdjustConstrainedDisplayDepth(preview.composedLayers[0], sample);
    if (!(constrained < unconstrained && std::abs(constrained - initialDepth) > 0.000001f)) {
        return format("depth mismatch: initial=%s unconstrained=%s constrained=%s", 
            initialDepth, unconstrained, constrained);
    }
    return "ok";
}

compat::Slice<float> PSDDepthMapWindow::previewDepthsForRegressionSmoke(compat::RetainedString gridName) {
    for (auto gridResult : preview.grids) {
        if (gridResult.grid == nullptr ||
            compat::RetainedString(compat::ngRequireObject(gridResult.grid)->getName()) != gridName) continue;
        return gridResult.depths.duplicate();
    }
    return {};
}

PsdDepthImportResult PSDDepthMapWindow::previewForRegressionSmoke() {
    if (previewDirty) rebuildPreview();
    return preview;
}

bool PSDDepthMapWindow::hasAppliedDepthsForRegressionSmoke(compat::RetainedString gridName) {
    for (auto gridResult : preview.grids) {
        if (gridResult.grid == nullptr ||
            compat::RetainedString(compat::ngRequireObject(gridResult.grid)->getName()) != gridName) continue;
        auto mapped = std::dynamic_pointer_cast<ext::nodes::DepthMappedNode>(gridResult.grid);
        if (mapped == nullptr) return false;
        auto depths = mapped->copyDepths();
        return !depths.isNull() && depths.size() == gridResult.depths.size() && std::equal(depths.begin(), depths.end(), gridResult.depths.begin());
    }
    return false;
}
#endif
}
