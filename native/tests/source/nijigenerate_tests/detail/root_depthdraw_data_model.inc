// Existing regression.d7865-8549; include once outside all namespaces.
#include "root_depthdraw_data_model.hpp"
#include "regression_internal.hpp"
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/ext/nodes/exgriddeformer_node.hpp>
#include <nijigenerate/ext/nodes/expathdeformer_node.hpp>
#include <nijigenerate/io/depthmap_psd.hpp>
#include <nijigenerate/platform/detail/path_allocation.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/viewport/depth/draw/autobind.hpp>
#include <nijigenerate/viewport/depth/draw/diagnostics.hpp>
#include <nijigenerate/viewport/depth/draw/persistence.hpp>
#include <nijigenerate/viewport/depth/draw/source.hpp>
#include <nijigenerate/viewport/depth/draw/viewport.hpp>
#include <compat/source_allocator.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>
namespace nijigenerate_tests::regression::detail {
void testDepthDrawDataModelContracts() {
    using nijigenerate::compat::RetainedString;
    using nijigenerate::compat::Slice;
    using nijigenerate::compat::ngRequireObject;
    using nicxlive::core::d_runtime::SourceAllocator;
    using nicxlive::core::math::Vec2;
    using nijigenerate::project::incActivePuppet;
    using nijigenerate::ext::nodes::ExGridDeformer;
    using nijigenerate::ext::nodes::ExPathDeformer;
    using nijigenerate::io::depthmap_psd::PsdDepthImportSettings;
    using namespace nijigenerate::viewport::depth::draw;
    using namespace nijigenerate::io::depthimage;
    namespace format = nijigenerate::core::translation;
    const auto text = []<std::size_t N>(const char (&value)[N]) {
        return RetainedString::fromStaticLiteral(value);
    };
    const auto equalElements = [](const auto& left, const auto& right) {
        return left.size() == right.size() &&
            (left.empty() || std::equal(left.begin(), left.end(), right.begin()));
    };
    // Evaluate the original binary operands in order, including RHS allocation.
    const auto equalLiteral = [&](auto&& left, auto&& right) {
        auto leftValue = left();
        auto rightValue = right();
        return equalElements(leftValue, rightValue);
    };
    // Selected inmath 1.0.6 linalg.d696: vector == vec.vector.
    const auto equalSourceVec2 = [](Vec2 left, Vec2 right) {
        return left.x == right.x && left.y == right.y;
    };
    resetCase();

    require(ngDepthDrawCoverageOverlayStride(DepthDrawMaxCoverageOverlayPoints) == 1 &&
        ngDepthDrawCoverageOverlayStride(DepthDrawMaxCoverageOverlayPoints + 1) == 2,
        text("DepthDraw coverage overlays must start sampling once the bounded point budget is exceeded"));
    auto largeCoveragePixelCount = DepthDrawMaxCoverageOverlayPoints * 100 + 37;
    auto largeCoverageStride = ngDepthDrawCoverageOverlayStride(largeCoveragePixelCount);
    require(1 + (largeCoveragePixelCount - 1) / largeCoverageStride <= DepthDrawMaxCoverageOverlayPoints,
        text("DepthDraw coverage overlay sampling must keep arbitrarily large layers within the point budget"));

    require(ngDepthDrawHistogramIntegralDimensionsSupported(2048, 2048) &&
        !ngDepthDrawHistogramIntegralDimensionsSupported(4096, 4096) &&
        !ngDepthDrawHistogramIntegralDimensionsSupported(-1, 1),
        text("DepthDraw alpha-gap cleanup must reject unsafe integral-histogram working sets before allocation"));

    DepthDrawLayer layer;
    layer.id = text("layer-a");
    layer.sourcePath = text("source.psd");
    layer.layerPath = text("/Face/depth");
    layer.displayName = text("Face Depth");
    layer.width = 2;
    layer.height = 1;
    layer.bounds.left = 10;
    layer.bounds.top = 20;
    layer.bounds.width = 2;
    layer.bounds.height = 1;
    layer.depthPixels = {0, 0, 0, 255, 255, 255, 255, 255};
    layer.normalCoverage = {255, 255, 255, 255, 255, 255, 255, 128};
    layer.xyOffset = Vec2{3, -4};
    layer.xyScale = Vec2{1.5f, 0.5f};
    layer.zScale = 2.0f;
    layer.zOffset = -0.25f;
    layer.backDepth = -0.5f;
    layer.frontDepth = 0.75f;
    layer.invert = true;
    layer.channel = DepthImageChannel::R;
    layer.convolution = DepthImageConvolution::BoxCustom;
    layer.customRadius = 4;
    layer.alphaThreshold = 0.2f;

    require(layer.hasDepthPixels(), text("DepthDrawLayer should report complete RGBA depth pixels"));
    require(layer.hasNormalCoverage(), text("DepthDrawLayer should report complete normal coverage pixels"));
    require(near(layer.applyZTransform(0.5f), 0.75f), text("DepthDrawLayer should apply z scale before z offset"));

    auto settings = layer.sampleSettings();
    require(settings.invert && near(settings.backDepth, -0.5f) && near(settings.frontDepth, 0.75f),
        text("DepthDrawLayer sample settings should preserve depth conversion parameters"));
    require(settings.channel == DepthImageChannel::R && settings.convolution == DepthImageConvolution::BoxCustom &&
        settings.customRadius == 4 && near(settings.alphaThreshold, 0.2f),
        text("DepthDrawLayer sample settings should preserve channel, convolution, radius, and alpha threshold"));

    DepthDrawLayer clippingBase;
    clippingBase.width = 2;
    clippingBase.height = 1;
    clippingBase.bounds = DepthDrawRect{10, 20, 2, 1};
    clippingBase.rgba = {std::uint8_t{255}, 255, 255, 128,
        std::uint8_t{255}, 255, 255, 0,};
    clippingBase.alphaMask = {std::uint8_t{1}, 0};
    clippingBase.visible = false;
    clippingBase.enabled = false;
    clippingBase.opacity = 0.25f;
    DepthDrawLayer clippedLayer;
    clippedLayer.width = 2;
    clippedLayer.height = 1;
    clippedLayer.bounds = DepthDrawRect{10, 20, 2, 1};
    clippedLayer.rgba = {std::uint8_t{255}, 255, 255, 255,
        std::uint8_t{255}, 255, 255, 255,};
    clippedLayer.depthPixels = clippedLayer.rgba.duplicate();
    ngDepthDrawApplyClippingBaseCoverage(clippedLayer, clippingBase);
    require(clippedLayer.rgba[3] == 128 && clippedLayer.rgba[7] == 0 &&
        clippedLayer.depthPixels[3] == 128 && clippedLayer.depthPixels[7] == 0,
        text("DepthDraw PSD extraction should preserve byte alpha when intersecting clipping-base coverage"));
    require(!clippedLayer.visible && !clippedLayer.enabled && near(clippedLayer.opacity, 0.25f),
        text("DepthDraw PSD extraction should propagate clipping-base visibility, enabled state, and opacity"));

    DepthDrawLayer offsetClippedLayer;
    offsetClippedLayer.width = 4;
    offsetClippedLayer.height = 1;
    offsetClippedLayer.bounds = DepthDrawRect{9, 20, 4, 1};
    offsetClippedLayer.rgba = {std::uint8_t{255}, 255, 255, 255,
        std::uint8_t{255}, 255, 255, 255,
        std::uint8_t{255}, 255, 255, 255,
        std::uint8_t{255}, 255, 255, 255,};
    offsetClippedLayer.depthPixels = offsetClippedLayer.rgba.duplicate();
    ngDepthDrawApplyClippingBaseCoverage(offsetClippedLayer, clippingBase);
    require(offsetClippedLayer.rgba[3] == 0 && offsetClippedLayer.rgba[7] == 128 &&
        offsetClippedLayer.rgba[11] == 0 && offsetClippedLayer.rgba[15] == 0,
        text("DepthDraw PSD clipping must make pixels outside the clipping base bounds transparent"));

    DepthDrawBinding binding;
    binding.layerId = layer.id;
    binding.targetGridUuid = 42;
    binding.targetNodeUuid = 24;
    binding.order = 7;
    require(binding.mergePolicy == DepthMergePolicy::Frontmost,
        text("DepthDrawBinding should default to depth-draw frontmost-style composition"));
    require(binding.appliesTo(42) && !binding.appliesTo(43),
        text("DepthDrawBinding should match only its enabled target grid"));

    auto session = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    session->layers.append(layer);
    session->bindings.append(binding);
    require(session->layerById(text("layer-a")) != nullptr && session->layerById(text("missing")) == nullptr,
        text("DepthDrawSession should look up layers by id"));
    require(session->selectLayer(text("layer-a")) && session->selectedLayerId == text("layer-a"),
        text("DepthDrawSession should select existing layers"));
    require(!session->selectLayer(text("missing")) && session->selectedLayerId == text("layer-a"),
        text("DepthDrawSession should reject missing layer selection without changing selection"));
    require(session->selectTargetGrid(42) && session->selectedGridUuid == 42,
        text("DepthDrawSession should select target grids referenced by bindings"));
    require(!session->selectTargetGrid(43) && session->selectedGridUuid == 42,
        text("DepthDrawSession should reject unbound target grid selection without changing selection"));
    auto selectionSession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    selectionSession->layers.append(layer);
    selectionSession->bindings.append(binding);
    DepthDrawLayer topLayer = layer;
    topLayer.id = text("layer-top");
    topLayer.bounds.left = 10;
    topLayer.bounds.top = 20;
    topLayer.bounds.width = 2;
    topLayer.bounds.height = 1;
    selectionSession->layers.append(topLayer);
    DepthDrawBinding topBinding = binding;
    topBinding.layerId = topLayer.id;
    topBinding.targetGridUuid = 84;
    selectionSession->bindings.append(topBinding);
    require(selectionSession->selectLayerPlaneAtDocumentPoint(Vec2{13.5f, 16.25f}) &&
        selectionSession->selectedLayerId == text("layer-top") && selectionSession->selectedGridUuid == 84,
        text("DepthDrawSession should select the topmost transformed visible layer plane and synchronize its target"));
    selectionSession->layers[selectionSession->layers.size() - 1].visible = false;
    require(selectionSession->selectLayerPlaneAtDocumentPoint(Vec2{13.5f, 16.25f}) &&
        selectionSession->selectedLayerId == text("layer-a") && selectionSession->selectedGridUuid == 42,
        text("DepthDrawSession layer-plane selection should ignore invisible layers"));
    require(!selectionSession->selectLayerPlaneAtDocumentPoint(Vec2{1000, 1000}) &&
        selectionSession->selectedLayerId == text("layer-a") && selectionSession->selectedGridUuid == 42,
        text("DepthDrawSession layer-plane selection should reject misses without changing selection"));
    session->clearSelection();
    require(session->selectedLayerId.size() == 0 && session->selectedGridUuid == 0,
        text("DepthDrawSession should clear layer and target selection together"));
    require(session->bindingsForGrid(42).size() == 1 && session->bindingsForGrid(43).size() == 0,
        text("DepthDrawSession should list enabled bindings for one target grid"));

    auto dirtySession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    DepthDrawLayer dirtyLayerB;
    dirtyLayerB.id = text("layer-b");
    dirtySession->layers.append(layer);
    dirtySession->layers.append(dirtyLayerB);
    dirtySession->bindings.append(binding);
    DepthDrawBinding dirtyBindingB = binding;
    dirtyBindingB.layerId = text("layer-b");
    dirtyBindingB.targetGridUuid = 43;
    dirtySession->bindings.append(dirtyBindingB);
    dirtySession->markLayerPreviewDirty(text("layer-a"));
    require(dirtySession->isTargetPreviewDirty(42) && !dirtySession->isTargetPreviewDirty(43),
        text("DepthDrawSession should mark only targets bound to the changed layer as preview-dirty"));
    dirtySession->markBindingPreviewDirty(dirtyBindingB);
    require(equalLiteral(
            [&] { return dirtySession->dirtyTargetGridIds(); },
            [&] { return Slice<std::uint64_t>{42ULL, 43ULL}; }),
        text("DepthDrawSession should expose sorted preview-dirty target grid ids"));
    dirtySession->clearTargetPreviewDirty(42);
    require(!dirtySession->isTargetPreviewDirty(42) && dirtySession->isTargetPreviewDirty(43),
        text("DepthDrawSession should clear preview dirty state per target"));
    dirtySession->markAllPreviewDirty();
    require(equalLiteral(
            [&] { return dirtySession->dirtyTargetGridIds(); },
            [&] { return Slice<std::uint64_t>{42ULL, 43ULL}; }),
        text("DepthDrawSession should mark every enabled bound target preview-dirty"));
    dirtySession->clearPreviewDirty();
    dirtySession->bindings[1].enabled = false;
    auto disabledBindingDiagnosticsRevision = dirtySession->diagnosticsRevision();
    require(dirtySession->updateBindingSampling(text("layer-b"), 43, false, 0.6f) &&
        dirtySession->diagnosticsRevision() != disabledBindingDiagnosticsRevision &&
        dirtySession->dirtyTargetGridIds().size() == 0,
        text("disabled binding sampling edits must invalidate diagnostics without dispatching target preview work"));
    dirtySession->bindings[1].enabled = true;
    require(dirtySession->dirtyTargetGridIds().size() == 0,
        text("DepthDrawSession should clear all preview dirty state"));
    require(dirtySession->updateLayerXYTransform(text("layer-a"), Vec2{-2, 5}, Vec2{0.5f, 2.0f}),
        text("DepthDrawSession should update XY transform for existing layers"));
    require(equalSourceVec2(dirtySession->layers[0].xyOffset, Vec2{-2, 5}) &&
        equalSourceVec2(dirtySession->layers[0].xyScale, Vec2{0.5f, 2.0f}),
        text("DepthDrawSession should store updated XY transform values"));
    require(equalLiteral(
            [&] { return dirtySession->dirtyTargetGridIds(); },
            [&] { return Slice<std::uint64_t>{42ULL}; }),
        text("DepthDrawSession XY transform updates should dirty only targets bound to the changed layer"));
    dirtySession->clearPreviewDirty();
    require(dirtySession->updateLayerZTransform(text("layer-a"), -0.75f, 0.8f, false, 1.25f, -0.1f),
        text("DepthDrawSession should update Z transform for existing layers"));
    require(near(dirtySession->layers[0].backDepth, -0.75f) && near(dirtySession->layers[0].frontDepth, 0.8f) &&
        !dirtySession->layers[0].invert && near(dirtySession->layers[0].zScale, 1.25f) &&
        near(dirtySession->layers[0].zOffset, -0.1f),
        text("DepthDrawSession should store updated Z transform values"));
    require(equalLiteral(
            [&] { return dirtySession->dirtyTargetGridIds(); },
            [&] { return Slice<std::uint64_t>{42ULL}; }),
        text("DepthDrawSession Z transform updates should dirty only targets bound to the changed layer"));
    dirtySession->clearPreviewDirty();
    require(dirtySession->updateLayerSampling(text("layer-b"), DepthImageChannel::B, DepthImageConvolution::MedianCustom, 6, 0.35f),
        text("DepthDrawSession should update layer sampling settings"));
    require(dirtySession->layers[1].channel == DepthImageChannel::B &&
        dirtySession->layers[1].convolution == DepthImageConvolution::MedianCustom &&
        dirtySession->layers[1].customRadius == 6 && near(dirtySession->layers[1].alphaThreshold, 0.35f),
        text("DepthDrawSession should store updated layer sampling settings"));
    require(equalLiteral(
            [&] { return dirtySession->dirtyTargetGridIds(); },
            [&] { return Slice<std::uint64_t>{43ULL}; }),
        text("DepthDrawSession sampling updates should dirty only targets bound to the changed layer"));
    dirtySession->clearPreviewDirty();
    require(dirtySession->updateLayerSampling(
            text("layer-b"), DepthImageChannel::B, DepthImageConvolution::BoxCustom, 100, 0.35f) &&
        dirtySession->layers[1].customRadius == 64,
        text("DepthDrawSession must clamp oversized custom sampling radii at the shared state boundary"));
    dirtySession->clearPreviewDirty();
    require(dirtySession->updateLayerSampling(
            text("layer-b"), DepthImageChannel::B, DepthImageConvolution::MedianCustom, 0, 0.35f) &&
        dirtySession->layers[1].customRadius == 1,
        text("DepthDrawSession must clamp non-positive custom sampling radii at the shared state boundary"));
    dirtySession->clearPreviewDirty();
    auto gpuDisplay = dirtySession->display;
    gpuDisplay.useGpuPreview = true;
    require(dirtySession->updateDisplayOptions(gpuDisplay) &&
        dirtySession->layers[1].convolution == DepthImageConvolution::Median3x3 &&
        equalLiteral(
            [&] { return dirtySession->dirtyTargetGridIds(); },
            [&] { return Slice<std::uint64_t>{42ULL, 43ULL}; }),
        text("enabling GPU preview should normalize unsupported custom median sampling and dirty every target"));
    dirtySession->clearPreviewDirty();
    require(dirtySession->updateLayerSampling(
            text("layer-b"), DepthImageChannel::B, DepthImageConvolution::MedianCustom, 6, 0.35f) &&
        dirtySession->layers[1].convolution == DepthImageConvolution::Median3x3 &&
        equalLiteral(
            [&] { return dirtySession->dirtyTargetGridIds(); },
            [&] { return Slice<std::uint64_t>{43ULL}; }),
        text("GPU preview should normalize unsupported sampling requested after the mode is already active"));
    dirtySession->clearPreviewDirty();
    require(dirtySession->updateLayerVisibility(text("layer-b"), false, false) &&
        !dirtySession->layers[1].visible && !dirtySession->layers[1].enabled,
        text("DepthDrawSession should update layer visibility and enabled state"));
    require(equalLiteral(
            [&] { return dirtySession->dirtyTargetGridIds(); },
            [&] { return Slice<std::uint64_t>{43ULL}; }),
        text("DepthDrawSession visibility updates should dirty only targets bound to the changed layer"));
    dirtySession->clearPreviewDirty();
    DepthDrawLayer gapLayer;
    gapLayer.id = text("gap-layer");
    gapLayer.width = 5;
    gapLayer.height = 5;
    gapLayer.bounds.width = 5;
    gapLayer.bounds.height = 5;
    Slice<std::uint8_t> gapDepth = {80, 80, 80, 80, 80,
        80, 10, 0, 30, 80,
        80, 80, 20, 80, 80,
        80, 80, 0, 80, 80,
        80, 80, 80, 80, 80,};
    for (auto depth : gapDepth) {
        gapLayer.depthPixels.append(depth);
        gapLayer.depthPixels.append(depth);
        gapLayer.depthPixels.append(depth);
        gapLayer.depthPixels.append(std::uint8_t{255});
    }
    gapLayer.alphaMask.resize(gapDepth.size());
    std::fill(gapLayer.alphaMask.begin(), gapLayer.alphaMask.end(), 1);
    auto gapSourcePixels = gapLayer.depthPixels.duplicate();
    DepthDrawBinding gapBinding = binding;
    gapBinding.layerId = gapLayer.id;
    gapBinding.targetGridUuid = 44;
    dirtySession->layers.append(gapLayer);
    dirtySession->bindings.append(gapBinding);
    DepthDrawAlphaDepthFocusedRule extremeFocusedRule;
    extremeFocusedRule.layerIndex = std::numeric_limits<std::int32_t>::max();
    extremeFocusedRule.x = std::numeric_limits<std::int32_t>::max();
    extremeFocusedRule.y = std::numeric_limits<std::int32_t>::min();
    extremeFocusedRule.w = std::numeric_limits<std::int32_t>::max();
    extremeFocusedRule.h = std::numeric_limits<std::int32_t>::max();
    extremeFocusedRule.lift = std::numeric_limits<std::int32_t>::max();
    extremeFocusedRule.radius = std::numeric_limits<std::int32_t>::max();
    auto gapFill = dirtySession->applyLayerAlphaDepthGapFill(
        gapLayer.id, Slice<DepthDrawAlphaDepthFocusedRule>{extremeFocusedRule});
    require(gapFill.succeeded && gapFill.detected.total == 5 && gapFill.filled.filled == 5 &&
        gapFill.filled.remaining == 0,
        text("DepthDrawSession alpha-depth gap fill should report depth-draw-compatible detection/fill counts"));
    require(equalLiteral(
            [&] { return ngDepthDrawDecodeGrayscaleDepthPixelsFromRgbaSource(dirtySession->layers[dirtySession->layers.size() - 1].depthPixels); },
            [&] { return Slice<std::uint8_t>{80, 80, 80, 80, 80,
            80, 80, 80, 80, 80,
            80, 80, 80, 80, 80,
            80, 80, 80, 80, 80,
            80, 80, 80, 80, 80,}; }),
        text("DepthDrawSession alpha-depth gap fill should update layer depth pixels from the depth-draw median fill result"));
    require(equalLiteral(
            [&] { return dirtySession->dirtyTargetGridIds(); },
            [&] { return Slice<std::uint64_t>{44ULL}; }),
        text("DepthDrawSession alpha-depth gap fill should dirty only targets bound to the changed layer"));
    require(dirtySession->layers[dirtySession->layers.size() - 1].cleanupOperations.size() == 1 &&
        dirtySession->layers[dirtySession->layers.size() - 1].cleanupOperations[0].kind == DepthDrawLayerCleanupKind::AlphaDepthGapFill,
        text("DepthDrawSession should record destructive alpha-depth cleanup as a replayable operation"));
    auto storedFocusedRule = dirtySession->layers[dirtySession->layers.size() - 1].cleanupOperations[0].focusedRules[0];
    require(storedFocusedRule.x == gapLayer.width && storedFocusedRule.y == 0 &&
        storedFocusedRule.w == 0 && storedFocusedRule.h == gapLayer.height &&
        storedFocusedRule.lift == 255 && storedFocusedRule.radius == DepthDrawMaxFocusedRuleRadius,
        text("generic alpha-depth cleanup must normalize focused rule bounds before execution and persistence"));
    auto cleanedGapPixels = dirtySession->layers[dirtySession->layers.size() - 1].depthPixels.duplicate();
    auto cleanupState = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    cleanupState->layers = {dirtySession->layers[dirtySession->layers.size() - 1]};
    cleanupState->bindings = {gapBinding};
    auto persistedCleanup = ngDepthDrawSessionFromPersistentJson(ngDepthDrawSessionToPersistentJson(cleanupState));
    auto reloadedCleanup = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    auto freshGapLayer = gapLayer;
    freshGapLayer.depthPixels = gapSourcePixels.duplicate();
    freshGapLayer.cleanupOperations = {};
    reloadedCleanup->layers = {freshGapLayer};
    auto cleanupReload = ngDepthDrawCarryReloadState(reloadedCleanup, persistedCleanup);
    require(cleanupReload.matchedLayers == 1 &&
        equalLiteral(
            [&] { return reloadedCleanup->layers[0].depthPixels; },
            [&] { return cleanedGapPixels; }) &&
        reloadedCleanup->layers[0].cleanupOperations.size() == 1,
        text("DepthDraw persistent state should replay destructive cleanup after source pixels are reloaded"));
    auto duplicatePrevious = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    DepthDrawLayer duplicatePreviousA = layer;
    duplicatePreviousA.id = text("previous-duplicate-a");
    duplicatePreviousA.layerPath = text("/duplicate");
    duplicatePreviousA.displayName = text("duplicate");
    duplicatePreviousA.xyOffset = Vec2{1, 2};
    duplicatePreviousA.sampleDepthScale = 0.25f;
    DepthDrawLayer duplicatePreviousB = duplicatePreviousA;
    duplicatePreviousB.id = text("previous-duplicate-b");
    duplicatePreviousB.xyOffset = Vec2{3, 4};
    duplicatePreviousB.sampleDepthScale = 0.75f;
    duplicatePrevious->layers = {duplicatePreviousA, duplicatePreviousB};
    DepthDrawBinding duplicateBindingA = binding;
    duplicateBindingA.layerId = duplicatePreviousA.id;
    duplicateBindingA.targetGridUuid = 501;
    DepthDrawBinding duplicateBindingB = binding;
    duplicateBindingB.layerId = duplicatePreviousB.id;
    duplicateBindingB.targetGridUuid = 502;
    duplicatePrevious->bindings = {duplicateBindingA, duplicateBindingB};
    duplicatePrevious->selectedLayerId = duplicatePreviousB.id;
    duplicatePrevious->selectedGridUuid = duplicateBindingB.targetGridUuid;
    auto duplicateReloaded = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    DepthDrawLayer duplicateReloadedA = duplicatePreviousA;
    duplicateReloadedA.id = text("reloaded-duplicate-a");
    duplicateReloadedA.xyOffset = Vec2{0, 0};
    duplicateReloadedA.sampleDepthScale = 1.0f;
    DepthDrawLayer duplicateReloadedB = duplicateReloadedA;
    duplicateReloadedB.id = text("reloaded-duplicate-b");
    duplicateReloaded->layers = {duplicateReloadedA, duplicateReloadedB};
    auto duplicateReload = ngDepthDrawCarryReloadState(duplicateReloaded, duplicatePrevious);
    require(duplicateReload.matchedLayers == 2 && duplicateReload.preservedBindings == 2 &&
        equalSourceVec2(duplicateReloaded->layers[0].xyOffset, Vec2{1, 2}) &&
        equalSourceVec2(duplicateReloaded->layers[1].xyOffset, Vec2{3, 4}) &&
        near(duplicateReloaded->layers[0].sampleDepthScale, 0.25f) &&
        near(duplicateReloaded->layers[1].sampleDepthScale, 0.75f) &&
        duplicateReloaded->bindings[0].layerId == text("reloaded-duplicate-a") &&
        duplicateReloaded->bindings[1].layerId == text("reloaded-duplicate-b") &&
        duplicateReloaded->selectedLayerId == text("reloaded-duplicate-b"),
        text("DepthDraw reload should match duplicate layer keys one-to-one and preserve sample depth scale"));
    auto unrelatedPrevious = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    auto unrelatedOldLayer = duplicatePreviousA;
    unrelatedOldLayer.id = text("psd:0");
    unrelatedOldLayer.layerPath = text("/old-layer");
    unrelatedOldLayer.displayName = text("old-layer");
    unrelatedOldLayer.sourcePath = text("same.psd");
    unrelatedPrevious->layers = {unrelatedOldLayer};
    auto unrelatedBinding = duplicateBindingA;
    unrelatedBinding.layerId = unrelatedOldLayer.id;
    unrelatedPrevious->bindings = {unrelatedBinding};
    auto unrelatedReloaded = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    auto unrelatedNewLayer = unrelatedOldLayer;
    unrelatedNewLayer.layerPath = text("/new-layer");
    unrelatedNewLayer.displayName = text("new-layer");
    unrelatedReloaded->layers = {unrelatedNewLayer};
    auto unrelatedReload = ngDepthDrawCarryReloadState(unrelatedReloaded, unrelatedPrevious);
    require(unrelatedReload.matchedLayers == 0 && unrelatedReload.preservedBindings == 0 &&
        unrelatedReloaded->bindings.size() == 0,
        text("DepthDraw reload must not transfer state to an unrelated same-size PSD layer at the same positional id"));
    auto repeatedGapFill = dirtySession->applyLayerAlphaDepthGapFill(gapLayer.id);
    require(repeatedGapFill.succeeded && repeatedGapFill.detected.total == 0 &&
        repeatedGapFill.filled.filled == 0,
        text("DepthDrawSession alpha-depth gap fill should be repeatable from the previously filled depth"));
    require(!PsdDepthImportSettings{}.repairContourBand &&
        !PsdDepthImportSettings{}.smoothWavySurface,
        text("PSD depth cleanup toggles should match depth-draw's default-off state"));
    Slice<std::uint8_t> smoothValid = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    Slice<int> smoothGroups = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    Slice<float> flatDepths = {2, 2, 2, 2, 2, 2, 2, 2, 2};
    auto smoothedFlatDepths = ngDepthDrawSmoothGridDepthValuesSource(flatDepths, smoothValid, smoothGroups, 3, 3);
    for (std::size_t i = 0, count = smoothedFlatDepths.size(); i < count; ++i) {
        const auto value = smoothedFlatDepths[i];
        const bool sameDepth = std::abs(value - flatDepths[i]) < 0.0001f;
        const std::array<format::TypedFormatArgument, 3> arguments{
            format::ngFormatArgument(i), format::ngFormatArgument(value), format::ngFormatArgument(flatDepths[i])};
        auto rendered = format::ngFormatTyped(
            "Smooth wavy surface should preserve a constant Z surface: index=%s actual=%s expected=%s", arguments);
        auto message = nijigenerate::platform::detail::ngPathAllocation([&] {
            return RetainedString(std::move(rendered));
        });
        require(sameDepth, message);
    }
    Slice<float> wavyDepths = {0, 0, 0, 0, 10, 0, 0, 0, 0};
    auto smoothedWavyDepths = ngDepthDrawSmoothGridDepthValuesSource(
        wavyDepths, smoothValid, smoothGroups, 3, 3, 18.0f, 10.0f);
    require(smoothedWavyDepths[4] < wavyDepths[4] && smoothedWavyDepths[4] > 0.0f,
        text("Smooth wavy surface should smooth Z discontinuities with depth-draw's Taubin passes"));
    dirtySession->clearPreviewDirty();
    require(dirtySession->updateBindingSampling(text("layer-b"), 43, false, 0.45f),
        text("DepthDrawSession should update binding sampling settings"));
    require(!dirtySession->bindings[1].useNormalLayerAlpha && near(dirtySession->bindings[1].coverageThreshold, 0.45f),
        text("DepthDrawSession should store updated binding sampling settings"));
    require(equalLiteral(
            [&] { return dirtySession->dirtyTargetGridIds(); },
            [&] { return Slice<std::uint64_t>{43ULL}; }),
        text("DepthDrawSession binding sampling updates should dirty only the binding target"));
    dirtySession->clearPreviewDirty();
    auto nextDisplay = dirtySession->display;
    require(!dirtySession->updateDisplayOptions(nextDisplay) && dirtySession->dirtyTargetGridIds().size() == 0,
        text("DepthDrawSession display option updates should ignore unchanged display state"));
    nextDisplay.showRawDepth = !nextDisplay.showRawDepth;
    nextDisplay.showCoverage = !nextDisplay.showCoverage;
    require(dirtySession->updateDisplayOptions(nextDisplay) &&
        dirtySession->display.showRawDepth == nextDisplay.showRawDepth &&
        dirtySession->display.showCoverage == nextDisplay.showCoverage,
        text("DepthDrawSession should update display options through a shared preview-dirty path"));
    require(equalLiteral(
            [&] { return dirtySession->dirtyTargetGridIds(); },
            [&] { return Slice<std::uint64_t>{42ULL, 43ULL, 44ULL}; }),
        text("DepthDrawSession display option updates should dirty every enabled bound target"));
    dirtySession->clearPreviewDirty();
    require(!dirtySession->updateLayerXYTransform(text("missing"), Vec2{0, 0}, Vec2{1, 1}) &&
        !dirtySession->updateLayerZTransform(text("missing"), -1, 1, false, 1, 0) &&
        !dirtySession->updateLayerSampling(text("missing"), DepthImageChannel::R, DepthImageConvolution::Nearest, 1, 0.01f) &&
        !dirtySession->updateLayerVisibility(text("missing"), true, true) &&
        !dirtySession->updateBindingSampling(text("missing"), 42, true, 0.01f),
        text("DepthDrawSession inspector updates should reject missing layers and bindings"));
    require(dirtySession->dirtyTargetGridIds().size() == 0,
        text("DepthDrawSession failed inspector updates should not dirty any target"));

    auto directGrid = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    directGrid->setName(text("AutoBindGrid").toNullableString());
    DepthDrawLayer autoLayer;
    autoLayer.id = text("auto-layer");
    autoLayer.layerPath = text("/AutoBindGrid");
    autoLayer.displayName = text("AutoBindGrid");
    auto autoBind = ngDepthDrawAutoBindLayer(incActivePuppet(), autoLayer);
    require(autoBind.matched && !autoBind.ambiguous && autoBind.targetGridUuid == directGrid->getUUID(),
        text("DepthDraw auto-binding should reuse PSD depth direct GridDeformer name matching"));
    auto autoBindSession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    autoBindSession->layers.append(autoLayer);
    auto directBinding = ngDepthDrawAutoBindSession(autoBindSession, incActivePuppet());
    require(directBinding.size() == 1 && autoBindSession->bindings.size() == 1 &&
        autoBindSession->bindings[0].targetGridUuid == directGrid->getUUID(),
        text("DepthDraw auto-bind session should add bindings for matched layers"));

    auto staleAutoBindSession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    staleAutoBindSession->layers.append(autoLayer);
    auto staleBinding = ngDepthDrawBindingFromAutoBind(autoBind, 0);
    staleBinding.targetNodeUuid = std::numeric_limits<std::uint64_t>::max();
    staleBinding.targetGridUuid = std::numeric_limits<std::uint64_t>::max();
    staleBinding.enabled = false;
    staleAutoBindSession->bindings.append(staleBinding);
    ngDepthDrawAutoBindSession(staleAutoBindSession, incActivePuppet());
    require(staleAutoBindSession->bindings.size() == 1 &&
        staleAutoBindSession->bindings[0].targetGridUuid == directGrid->getUUID() &&
        staleAutoBindSession->bindings[0].targetNodeUuid == directGrid->getUUID() &&
        staleAutoBindSession->bindings[0].enabled,
        text("DepthDraw auto-bind should replace a disabled or missing stale binding with the discovered target"));

    auto disabledExactSession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    disabledExactSession->layers.append(autoLayer);
    auto disabledExactBinding = ngDepthDrawBindingFromAutoBind(autoBind, 0);
    disabledExactBinding.enabled = false;
    disabledExactSession->bindings.append(disabledExactBinding);
    ngDepthDrawAutoBindSession(disabledExactSession, incActivePuppet());
    require(disabledExactSession->bindings.size() == 1 && disabledExactSession->bindings[0].enabled,
        text("DepthDraw auto-bind should re-enable an existing binding to the discovered target"));

    auto directPath = ExPathDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    directPath->setName(text("AutoBindPath").toNullableString());
    DepthDrawLayer autoPathLayer;
    autoPathLayer.id = text("auto-path-layer");
    autoPathLayer.layerPath = text("/AutoBindPath");
    autoPathLayer.displayName = text("AutoBindPath");
    auto autoPathBind = ngDepthDrawAutoBindLayer(incActivePuppet(), autoPathLayer);
    require(autoPathBind.matched && !autoPathBind.ambiguous && autoPathBind.targetGridUuid == directPath->getUUID(),
        text("DepthDraw auto-binding should support direct PathDeformer name matching"));

    auto duplicateGrid = ExGridDeformer::createSourceDeferred([] { return ngRequireObject(incActivePuppet())->root; });
    duplicateGrid->setName(text("AutoBindGrid").toNullableString());
    auto ambiguousBind = ngDepthDrawAutoBindLayer(incActivePuppet(), autoLayer);
    require(!ambiguousBind.matched && ambiguousBind.ambiguous &&
        ambiguousBind.targetGridUuid == 0 && ambiguousBind.status == text("Ambiguous"),
        text("DepthDraw auto-binding should report ambiguous matches without selecting an arbitrary target"));

    DepthDrawLayer unboundLayer;
    unboundLayer.id = text("layer-b");
    unboundLayer.width = 1;
    unboundLayer.height = 1;
    unboundLayer.depthPixels = {128, 128, 128, 255};
    session->layers.append(unboundLayer);

    DepthDrawLayer missingPixelsLayer;
    missingPixelsLayer.id = text("layer-c");
    missingPixelsLayer.width = 1;
    missingPixelsLayer.height = 1;
    session->layers.append(missingPixelsLayer);

    auto ambiguousBinding = binding;
    ambiguousBinding.targetGridUuid = 44;
    session->bindings.append(ambiguousBinding);

    auto diagnostics = ngDepthDrawSourceDiagnostics(session);
    require(diagnostics.totalLayers == 3 && diagnostics.usableDepthLayers == 2,
        text("DepthDraw source diagnostics should count total and usable depth layers"));
    require(diagnostics.layersMissingDepthPixels == 1 && diagnostics.layersMissingTarget == 1,
        text("DepthDraw source diagnostics should report missing pixels and missing targets"));
    require(diagnostics.layersMissingCoverage == 1 && diagnostics.layersWithAmbiguousBindings == 1,
        text("DepthDraw source diagnostics should report missing coverage and ambiguous bindings"));
    auto stackRows = ngDepthDrawLayerStackRows(session);
    require(stackRows.size() == 3 && stackRows[0].layerId == text("layer-a") && stackRows[0].hasBinding &&
        stackRows[0].targetGridUuid == binding.targetGridUuid && stackRows[0].mergePolicy == binding.mergePolicy,
        text("DepthDraw layer stack rows should expose binding target and merge policy"));
    auto namedStackRows = ngDepthDrawLayerStackRows(session, nullptr, [&](std::uint64_t uuid) -> RetainedString {
        return uuid == binding.targetGridUuid ? text("Resolved Target") : RetainedString{};
    });
    require(namedStackRows[0].targetDisplayName == text("Resolved Target"),
        text("DepthDraw layer stack rows should resolve target display names when a resolver is provided"));
    auto filteredStackRows = ngDepthDrawFilterAndSortLayerStackRows(
        namedStackRows,
        text("resolved"),
        DepthDrawLayerStackSortMode::Name,
        false
    );
    require(filteredStackRows.size() == 1 && filteredStackRows[0].layerId == text("layer-a"),
        text("DepthDraw layer stack rows should filter against resolved target names"));
    auto sortedStackRows = ngDepthDrawFilterAndSortLayerStackRows(
        namedStackRows,
        text(""),
        DepthDrawLayerStackSortMode::Name,
        true
    );
    require(sortedStackRows.size() == 3 && sortedStackRows[0].layerId == text("layer-c") &&
        sortedStackRows[sortedStackRows.size() - 1].layerId == text("layer-a"),
        text("DepthDraw layer stack rows should sort by display name in descending order"));
    auto tieRows = namedStackRows.duplicate();
    for (auto& row : tieRows) {
        row.displayName = text("same");
        row.targetDisplayName = text("same-target");
        row.sampledVertices = 1;
        row.missingVertices = 1;
        row.hasDepthRange = true;
        row.minDepth = 0.0f;
    }
    auto sortedTieRows = ngDepthDrawFilterAndSortLayerStackRows(
        tieRows,
        text(""),
        DepthDrawLayerStackSortMode::Name,
        true
    );
    require(sortedTieRows.size() == 3 && sortedTieRows[0].layerId == text("layer-a") &&
        sortedTieRows[1].layerId == text("layer-b") && sortedTieRows[2].layerId == text("layer-c"),
        text("DepthDraw layer stack row sorting should preserve source order for equal sort keys"));
    sortedTieRows = ngDepthDrawFilterAndSortLayerStackRows(
        tieRows,
        text(""),
        DepthDrawLayerStackSortMode::Sampled,
        false
    );
    require(sortedTieRows.size() == 3 && sortedTieRows[0].layerId == text("layer-a") &&
        sortedTieRows[1].layerId == text("layer-b") && sortedTieRows[2].layerId == text("layer-c"),
        text("DepthDraw layer stack numeric sorting should preserve source order for equal sort keys"));
    require(stackRows[0].hasAmbiguousBindings && !stackRows[0].warningMissingCoverage &&
        !stackRows[0].warningMissingTarget,
        text("DepthDraw layer stack rows should expose ambiguous binding without false missing-coverage warnings"));
    require(stackRows[1].warningMissingTarget && stackRows[1].warningMissingCoverage &&
        !stackRows[1].warningMissingDepthPixels,
        text("DepthDraw layer stack rows should warn when a usable layer has no binding or coverage"));
    require(stackRows[2].warningMissingDepthPixels,
        text("DepthDraw layer stack rows should warn when a layer has no depth pixels"));

    DepthDrawLayer maskedLayer;
    maskedLayer.id = text("masked");
    maskedLayer.width = 2;
    maskedLayer.height = 2;
    maskedLayer.bounds.left = 10;
    maskedLayer.bounds.top = 20;
    maskedLayer.bounds.width = 2;
    maskedLayer.bounds.height = 2;
    maskedLayer.rgba = {10, 10, 10, 255, 20, 20, 20, 128,
        30, 30, 30, 64,  40, 40, 40, 255,};
    maskedLayer.depthPixels = maskedLayer.rgba.duplicate();
    ngDepthDrawApplyMaskToLayerAlpha(maskedLayer, Slice<std::uint8_t>{255, 128, 64, 0}, 2, 2, 10, 20, false, 255);
    require(maskedLayer.rgba[3] == 255 && maskedLayer.rgba[7] == 64 &&
        maskedLayer.rgba[11] == 16 && maskedLayer.rgba[15] == 0,
        text("DepthDraw PSD mask application should multiply layer alpha using document-space mask coordinates"));
    require(maskedLayer.depthPixels[3] == maskedLayer.rgba[3] &&
        maskedLayer.depthPixels[7] == maskedLayer.rgba[7] &&
        maskedLayer.depthPixels[11] == maskedLayer.rgba[11] &&
        maskedLayer.depthPixels[15] == maskedLayer.rgba[15],
        text("DepthDraw PSD mask application should keep depth pixel alpha in sync with RGBA alpha"));
    require(equalLiteral(
            [&] { return maskedLayer.alphaMask; },
            [&] { return Slice<std::uint8_t>{1, 1, 1, 0}; }),
        text("DepthDraw PSD mask application should rebuild the binary alpha mask after masking"));

    Slice<std::uint8_t> relativeMaskedAlpha = [&] {
        auto alpha = Slice<std::uint8_t>{255, 128, 64, 32};
        auto mask = Slice<std::uint8_t>{0};
        return ngDepthDrawApplyPsdMaskToAlphaSource(alpha, 2, 2, 100, 200,
            mask, 1, 1, 1, 1, true, 128);
    }();
    require(equalLiteral(
            [&] { return relativeMaskedAlpha; },
            [&] { return Slice<std::uint8_t>{128, 64, 32, 0}; }),
        text("DepthDraw PSD mask application should honor relative mask coordinates and default mask color"));
    auto scaledMask = ngDepthDrawScaleAlphaMaskSource(Slice<std::uint8_t>{1, 0, 0, 1}, 2, 2, 4, 4);
    require(equalLiteral(
            [&] { return scaledMask; },
            [&] { return Slice<std::uint8_t>{1, 1, 0, 0,
        1, 1, 0, 0,
        0, 0, 1, 1,
        0, 0, 1, 1,}; }), text("DepthDraw PSD depth alpha mask scaling should match depth-draw center/floor nearest mapping"));

    auto pairSession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    DepthDrawLayer depthA;
    depthA.id = text("depth-a");
    depthA.displayName = text(" Face ");
    depthA.layerPath = text("/Depth/Face");
    depthA.width = 2;
    depthA.height = 2;
    depthA.bounds.left = 10;
    depthA.bounds.top = 20;
    depthA.bounds.width = 2;
    depthA.bounds.height = 2;
    depthA.depthPixels = {0, 0, 0, 255, 0, 0, 0, 255,
        0, 0, 0, 255, 0, 0, 0, 255,};
    DepthDrawLayer depthB = depthA;
    depthB.id = text("depth-b");
    depthB.displayName = text("Missing");
    depthB.layerPath = text("/Depth/Missing");
    depthB.bounds.left = 100;
    pairSession->layers.append(depthA);
    pairSession->layers.append(depthB);

    DepthDrawLayer normalA;
    normalA.id = text("normal-a");
    normalA.displayName = text("face");
    normalA.layerPath = text("/Color/Face");
    normalA.width = 3;
    normalA.height = 3;
    normalA.bounds.left = 9;
    normalA.bounds.top = 19;
    normalA.bounds.width = 3;
    normalA.bounds.height = 3;
    normalA.opacity = 0.5f;
    normalA.rgba = {255, 255, 255, 0,   255, 255, 255, 20,  255, 255, 255, 40,
        255, 255, 255, 60,  255, 255, 255, 128, 255, 255, 255, 200,
        255, 255, 255, 220, 255, 255, 255, 240, 255, 255, 255, 255,};
    ngDepthDrawApplyMaskToLayerAlpha(normalA, Slice<std::uint8_t>{255, 128, 255, 255}, 2, 2, 10, 20, false, 255);
    DepthDrawLayer normalUnused = normalA;
    normalUnused.id = text("normal-unused");
    normalUnused.displayName = text("Other");
    normalUnused.bounds.left = 200;

    auto pairing = ngDepthDrawAttachNormalCoverage(pairSession, Slice<DepthDrawLayer>{normalUnused, normalA});
    require(pairing.matchedDepthLayers == 1 && pairing.missingNormalCoverage == 1,
        text("DepthDraw normal/depth pairing should report matched and missing depth layers"));
    require(equalLiteral(
            [&] { return pairing.matchedDepthLayerIds; },
            [&] { return Slice<RetainedString>{text("depth-a")}; }) &&
        equalLiteral(
            [&] { return pairing.missingNormalLayerIds; },
            [&] { return Slice<RetainedString>{text("depth-b")}; }),
        text("DepthDraw normal/depth pairing should report affected depth layer ids"));
    require(pairSession->layers[0].hasNormalCoverage() && pairSession->layers[1].normalCoverage.size() == 0,
        text("DepthDraw normal/depth pairing should attach coverage only to matched depth layers"));
    require(pairSession->layers[0].normalCoverage[3] == 64 &&
        pairSession->layers[0].normalCoverage[7] == 50 &&
        pairSession->layers[0].normalCoverage[11] == 120 &&
        pairSession->layers[0].normalCoverage[15] == 128,
        text("DepthDraw normal coverage should project masked normal alpha through document bounds and layer opacity"));

    auto hiddenPairSession = std::allocate_shared<DepthDrawSession>(SourceAllocator<DepthDrawSession>{});
    DepthDrawLayer hiddenDepth = depthA;
    hiddenDepth.id = text("hidden-depth");
    hiddenDepth.visible = false;
    hiddenDepth.enabled = false;
    hiddenPairSession->layers.append(hiddenDepth);
    hiddenPairSession->layers.append(depthA);
    DepthDrawLayer hiddenNormal = normalA;
    hiddenNormal.id = text("hidden-normal");
    hiddenNormal.visible = false;
    hiddenNormal.enabled = false;
    hiddenNormal.opacity = 1.0f;
    DepthDrawLayer visibleNormal = normalA;
    visibleNormal.id = text("visible-normal");
    visibleNormal.opacity = 0.25f;
    auto visibleOnlyPairing = ngDepthDrawAttachNormalCoverage(
        hiddenPairSession, Slice<DepthDrawLayer>{hiddenNormal, visibleNormal});
    require(visibleOnlyPairing.matchedDepthLayers == 1 && visibleOnlyPairing.missingNormalCoverage == 0,
        text("DepthDraw normal/depth pairing should exclude hidden depth layers from missing-coverage reporting"));
    require(equalLiteral(
            [&] { return visibleOnlyPairing.matchedDepthLayerIds; },
            [&] { return Slice<RetainedString>{text("depth-a")}; }),
        text("DepthDraw normal/depth pairing should match only visible depth layers"));
    require(hiddenPairSession->layers[0].normalCoverage.size() == 0,
        text("DepthDraw normal/depth pairing should not attach coverage to hidden depth layers"));
    require(hiddenPairSession->layers[1].normalCoverage[3] == 32 &&
        hiddenPairSession->layers[1].normalCoverage[7] == 25 &&
        hiddenPairSession->layers[1].normalCoverage[11] == 60 &&
        hiddenPairSession->layers[1].normalCoverage[15] == 64,
        text("DepthDraw normal/depth pairing should skip hidden normal layers before exact-name/bounds matching"));

    session->clear();
    require(session->layers.size() == 0 && session->bindings.size() == 0 && session->selectedLayerId.size() == 0,
        text("DepthDrawSession clear should remove layers, bindings, and selection"));
}
}
