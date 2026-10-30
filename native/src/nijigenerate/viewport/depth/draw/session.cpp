#include "session.hpp"
#include "coordinate.hpp"
#include <nijigenerate/io/imageexport_case.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <common/object_key_policy.hpp>
#include <common/string_key_map.hpp>
#include <algorithm>
#include <array>
#include <bit>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::viewport::depth::draw {
using namespace io::depthimage;
namespace {
std::string_view text(const DepthDrawString& value) { return value.view(); }
bool equal(const DepthDrawString& a, const DepthDrawString& b) { return text(a) == text(b); }
template<class Value> using StringMap = nicxlive::core::common::StringKeyMap<Value>;

void prepareSessionClassInfo() {
    static const bool registered = [] {
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(DepthDrawSession),
            compat::RetainedString::fromStaticLiteral("nijigenerate.viewport.depth.draw.session.DepthDrawSession"));
        return true;
    }();
    (void)registered;
}

template<class Array> decltype(auto) sourceAt(Array& values, std::size_t index, std::size_t line) {
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/session.d"), line);
    return values[index];
}
template<class T> std::span<const T> view(const compat::Slice<T>& values) {
    return {values.data(), values.size()};
}
int narrowIndex(std::ptrdiff_t value) { return std::bit_cast<int>(static_cast<std::uint32_t>(value)); }
compat::Slice<DepthDrawLayerCleanupOperation> cloneCleanupOperations(
    const compat::Slice<DepthDrawLayerCleanupOperation> operations) {
    compat::Slice<DepthDrawLayerCleanupOperation> result;
    for (auto operation : operations) {
        DepthDrawLayerCleanupOperation copy;
        copy.kind = operation.kind;
        copy.contourThickness = operation.contourThickness;
        copy.focusedRules = operation.focusedRules.duplicate();
        result.append(copy);
    }
    return result;
}
bool writeCleanupDepthPixel(DepthDrawLayer& layer, std::size_t index, std::uint8_t value) {
    const auto offset = index * 4;
    bool changed = false;
    switch (layer.channel) {
        case DepthImageChannel::R:
            changed = sourceAt(layer.depthPixels, offset, 68) != value;
            sourceAt(layer.depthPixels, offset, 69) = value;
            break;
        case DepthImageChannel::G:
            changed = sourceAt(layer.depthPixels, offset + 1, 72) != value;
            sourceAt(layer.depthPixels, offset + 1, 73) = value;
            break;
        case DepthImageChannel::B:
            changed = sourceAt(layer.depthPixels, offset + 2, 76) != value;
            sourceAt(layer.depthPixels, offset + 2, 77) = value;
            break;
        case DepthImageChannel::AverageRGB:
        case DepthImageChannel::Luminance:
            changed = sourceAt(layer.depthPixels, offset, 81) != value ||
                sourceAt(layer.depthPixels, offset + 1, 82) != value ||
                sourceAt(layer.depthPixels, offset + 2, 83) != value;
            sourceAt(layer.depthPixels, offset, 84) = value;
            sourceAt(layer.depthPixels, offset + 1, 85) = value;
            sourceAt(layer.depthPixels, offset + 2, 86) = value;
            break;
        default: throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral(
            "No appropriate switch clause found"), compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/viewport/depth/draw/session.d"), 66);
    }
    return changed;
}
}
// Inherited Object defaults, object.d; Digital Mars 2000-2011, BSL-1.0.
// Retained notice: nicxlive/core/licenses/druntime-associative-array.txt.
compat::RetainedString DepthDrawSession::toString() {
    const auto receiver = weak_from_this().lock();
    prepareSessionClassInfo();
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
}
std::size_t DepthDrawSession::toHash() {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(this));
    return address ^ (address >> 4);
}
bool DepthDrawSession::opEquals(std::shared_ptr<DepthDrawSession> other) {
    return dynamic_cast<const void*>(this) == nicxlive::core::common::ngClassObjectIdentity(other);
}
int DepthDrawSession::opCmp(std::shared_ptr<DepthDrawSession>) {
    const auto receiver = weak_from_this().lock();
    prepareSessionClassInfo();
    const auto className = nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    const auto message = [&] {
        try {
            std::string result = "need opCmp for class ";
            result += className.view();
            return compat::RetainedString(std::move(result));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>();
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>();
        }
    }();
    throw compat::ThrowableException(message, compat::RetainedString::fromStaticLiteral("object.d"), 195);
}
bool ngDepthDrawSessionObjectEquals(std::shared_ptr<DepthDrawSession> left,
    std::shared_ptr<DepthDrawSession> right) {
    return nicxlive::core::common::ngClassObjectEquals(std::move(left), std::move(right), &prepareSessionClassInfo);
}
void DepthDrawSession::bumpDiagnosticsRevision() {
    ++diagnosticsRevisionValue;
    if (diagnosticsRevisionValue == 0) diagnosticsRevisionValue = 1;
}
std::ptrdiff_t DepthDrawSession::findLayerIndex(DepthDrawString id) const {
    const auto currentLayers = layers;
    for (std::size_t i = 0; i < currentLayers.size(); ++i)
        if (equal(currentLayers[i].id, id)) return static_cast<std::ptrdiff_t>(i);
    return -1;
}
std::shared_ptr<DepthDrawLayer> DepthDrawSession::layerById(DepthDrawString id) {
    const auto index = findLayerIndex(id);
    if (index < 0) return {};
    auto currentLayers = layers;
    auto* selected = &sourceAt(currentLayers, static_cast<std::size_t>(index), 123);
    return std::shared_ptr<DepthDrawLayer>(currentLayers.retentionOwner(), selected);
}
std::shared_ptr<const DepthDrawLayer> DepthDrawSession::layerById(DepthDrawString id) const {
    const auto index = findLayerIndex(id);
    if (index < 0) return {};
    const auto currentLayers = layers;
    const auto* selected = &sourceAt(currentLayers, static_cast<std::size_t>(index), 128);
    return std::shared_ptr<const DepthDrawLayer>(currentLayers.retentionOwner(), selected);
}
compat::Slice<DepthDrawBinding> DepthDrawSession::bindingsForGrid(std::uint64_t gridUuid) const {
    compat::Slice<DepthDrawBinding> result;
    const auto currentBindings = bindings;
    for (auto binding : currentBindings) if (binding.appliesTo(gridUuid)) result.append(binding);
    return result;
}
bool DepthDrawSession::hasTargetGrid(std::uint64_t gridUuid) const {
    if (gridUuid == 0) return false;
    const auto currentBindings = bindings;
    for (auto binding : currentBindings) if (binding.targetGridUuid == gridUuid) return true;
    return false;
}
void DepthDrawSession::markTargetPreviewDirty(std::uint64_t gridUuid) {
    if (gridUuid == 0) return;
    bumpDiagnosticsRevision();
    auto revision = previewTargetRevisions.find(gridUuid);
    auto nextRevision = revision == previewTargetRevisions.end() ? std::uint64_t{1} : revision->second + 1;
    if (nextRevision == 0) nextRevision = 1;
    previewTargetRevisions[gridUuid] = nextRevision;
    previewDirtyTargets[gridUuid] = true;
}
std::uint64_t DepthDrawSession::targetPreviewRevision(std::uint64_t gridUuid) const {
    auto revision = previewTargetRevisions.find(gridUuid);
    return revision == previewTargetRevisions.end() ? 0 : revision->second;
}
std::uint64_t DepthDrawSession::diagnosticsRevision() const { return diagnosticsRevisionValue; }
void DepthDrawSession::markLayerPreviewDirty(DepthDrawString layerId) {
    const auto receiver = weak_from_this().lock();
    if (text(layerId).empty()) return;
    bumpDiagnosticsRevision();
    const auto currentBindings = bindings;
    for (auto binding : currentBindings) {
        if (!binding.enabled || !equal(binding.layerId, layerId)) continue;
        markTargetPreviewDirty(binding.targetGridUuid);
    }
}
void DepthDrawSession::markBindingPreviewDirty(const DepthDrawBinding binding) {
    const auto receiver = weak_from_this().lock();
    bumpDiagnosticsRevision();
    if (!binding.enabled) return;
    markTargetPreviewDirty(binding.targetGridUuid);
}
void DepthDrawSession::markAllPreviewDirty() {
    const auto receiver = weak_from_this().lock();
    bumpDiagnosticsRevision();
    const auto currentBindings = bindings;
    for (auto binding : currentBindings) {
        if (!binding.enabled) continue;
        markTargetPreviewDirty(binding.targetGridUuid);
    }
}
bool DepthDrawSession::isTargetPreviewDirty(std::uint64_t gridUuid) const {
    return previewDirtyTargets.contains(gridUuid);
}
compat::Slice<std::uint64_t> DepthDrawSession::dirtyTargetGridIds() const {
    auto keys = previewDirtyTargets.keys();
    auto result = compat::Slice<std::uint64_t>::retainedView(keys.retentionOwner(), keys.data(), keys.size());
    if (result.size() > 1) std::sort(result.begin(), result.end());
    return result;
}
void DepthDrawSession::clearTargetPreviewDirty(std::uint64_t gridUuid) { previewDirtyTargets.erase(gridUuid); }
void DepthDrawSession::clearPreviewDirty() { previewDirtyTargets.reset(); }
bool DepthDrawSession::selectLayer(DepthDrawString id) {
    const auto receiver = weak_from_this().lock();
    if (!layerById(id)) return false;
    selectedLayerId = id;
    return true;
}
bool DepthDrawSession::selectTargetGrid(std::uint64_t gridUuid) {
    const auto receiver = weak_from_this().lock();
    if (!hasTargetGrid(gridUuid)) return false;
    selectedGridUuid = gridUuid;
    return true;
}
bool DepthDrawSession::selectLayerPlaneAtDocumentPoint(nicxlive::core::math::Vec2 documentPoint) {
    auto currentLayers = layers;
    for (std::size_t i = currentLayers.size(); i > 0; --i) {
        auto& layer = currentLayers[i - 1];
        if (!layer.enabled || !layer.visible) continue;
        nicxlive::core::math::Vec2 minPoint, maxPoint;
        coordinate::ngDepthDrawLayerDocumentBounds(layer, minPoint, maxPoint);
        if (documentPoint.x < minPoint.x || documentPoint.x > maxPoint.x ||
            documentPoint.y < minPoint.y || documentPoint.y > maxPoint.y) continue;
        selectedLayerId = layer.id;
        const auto currentBindings = bindings;
        for (auto binding : currentBindings) {
            if (!binding.enabled || !equal(binding.layerId, layer.id)) continue;
            selectedGridUuid = binding.targetGridUuid;
            break;
        }
        return true;
    }
    return false;
}
bool DepthDrawSession::updateLayerXYTransform(DepthDrawString layerId,
    nicxlive::core::math::Vec2 offset, nicxlive::core::math::Vec2 scale) {
    const auto receiver = weak_from_this().lock();
    auto layer = layerById(layerId);
    if (!layer) return false;
    layer->xyOffset = offset;
    layer->xyScale = scale;
    markLayerPreviewDirty(layerId);
    return true;
}
bool DepthDrawSession::updateLayerZTransform(DepthDrawString layerId, float backDepth, float frontDepth,
    bool invert, float zScale, float zOffset) {
    const auto receiver = weak_from_this().lock();
    auto layer = layerById(layerId);
    if (!layer) return false;
    layer->backDepth = backDepth;
    layer->frontDepth = frontDepth;
    layer->invert = invert;
    layer->zScale = zScale;
    layer->zOffset = zOffset;
    markLayerPreviewDirty(layerId);
    return true;
}
bool DepthDrawSession::updateLayerSampling(DepthDrawString layerId, DepthImageChannel channel,
    DepthImageConvolution convolution, int customRadius, float alphaThreshold) {
    const auto receiver = weak_from_this().lock();
    auto layer = layerById(layerId);
    if (!layer) return false;
    if (display.useGpuPreview && convolution == DepthImageConvolution::MedianCustom)
        convolution = DepthImageConvolution::Median3x3;
    layer->channel = channel;
    layer->convolution = convolution;
    layer->customRadius = ngNormalizeDepthImageCustomRadius(customRadius);
    layer->alphaThreshold = alphaThreshold;
    markLayerPreviewDirty(layerId);
    return true;
}
bool DepthDrawSession::updateLayerVisibility(DepthDrawString layerId, bool visible, bool enabled) {
    const auto receiver = weak_from_this().lock();
    auto layer = layerById(layerId);
    if (!layer) return false;
    layer->visible = visible;
    layer->enabled = enabled;
    markLayerPreviewDirty(layerId);
    return true;
}
DepthDrawLayerAlphaDepthGapFillSummary DepthDrawSession::applyLayerAlphaDepthGapFill(DepthDrawString layerId,
    compat::ConstSlice<DepthDrawAlphaDepthFocusedRule> focusedRules) {
    const auto receiver = weak_from_this().lock();
    DepthDrawLayerAlphaDepthGapFillSummary summary;
    summary.layerId = layerId;
    auto layer = layerById(layerId);
    if (!layer || !layer->hasDepthPixels()) return summary;
    compat::Slice<DepthDrawAlphaDepthFocusedRule> normalizedRules;
    for (auto rule : focusedRules)
        normalizedRules.append(ngNormalizeDepthDrawFocusedRule(rule, layer->width, layer->height));
    auto depth = ngDepthDrawDecodeDepthPixelsFromRgbaSource(layer->depthPixels, layer->channel);
    auto alphaMask = layer->alphaMask.size() == depth.size() ? layer->alphaMask.duplicate() :
        ngDepthDrawAlphaMaskFromRgbaSource(layer->depthPixels);
    summary.detected = ngDepthDrawDetectAlphaDepthGaps(view(depth), view(alphaMask), layer->width, layer->height,
        narrowIndex(findLayerIndex(layerId)), view(normalizedRules));
    summary.filled = ngDepthDrawMedianFillDepth(view(depth), view(alphaMask), view(summary.detected.mask),
        layer->width, layer->height);
    if (summary.detected.total == 0 && summary.filled.filled == 0 && summary.filled.remaining == 0) {
        summary.succeeded = true;
        return summary;
    }
    bool changed = false;
    for (std::size_t i = 0; i < summary.filled.depth.size(); ++i) {
        auto value = summary.filled.depth[i];
        if (!sourceAt(summary.detected.mask, i, 313) || sourceAt(depth, i, 313) == value) continue;
        changed = writeCleanupDepthPixel(*layer, i, value) || changed;
    }
    layer->alphaMask = alphaMask;
    if (changed) {
        DepthDrawLayerCleanupOperation operation;
        operation.kind = DepthDrawLayerCleanupKind::AlphaDepthGapFill;
        operation.focusedRules = normalizedRules;
        layer->cleanupOperations.append(operation);
        markLayerPreviewDirty(layerId);
    }
    summary.succeeded = true;
    return summary;
}
DepthDrawLayerContourRepairSummary DepthDrawSession::repairLayerContourDepth(DepthDrawString layerId, int thickness) {
    const auto receiver = weak_from_this().lock();
    DepthDrawLayerContourRepairSummary summary;
    summary.layerId = layerId;
    auto layer = layerById(layerId);
    if (!layer || !layer->hasDepthPixels()) return summary;
    thickness = ngNormalizeDepthDrawContourThickness(thickness);
    auto depth = ngDepthDrawDecodeDepthPixelsFromRgbaSource(layer->depthPixels, layer->channel);
    auto mask = layer->alphaMask.size() == depth.size() ? layer->alphaMask.duplicate() :
        ngDepthDrawAlphaMaskFromRgbaSource(layer->depthPixels);
    auto contourBand = ngDepthDrawBuildLayerContourBandMaskSource(mask, layer->width, layer->height, thickness);
    auto repairedSeed = depth.duplicate();
    for (std::size_t i = 0; i < contourBand.size(); ++i) {
        if (contourBand[i]) { sourceAt(repairedSeed, i, 343) = 0; ++summary.contourPixels; }
    }
    auto repaired = ngDepthDrawInpaintMaskedLayerDepth(view(repairedSeed), view(mask), layer->width, layer->height);
    for (auto value : repaired.filledMask) if (value) ++summary.filledPixels;
    bool changed = false;
    for (std::size_t i = 0; i < repaired.pixels.size(); ++i) {
        if (!sourceAt(repaired.filledMask, i, 353)) continue;
        changed = writeCleanupDepthPixel(*layer, i, repaired.pixels[i]) || changed;
    }
    layer->alphaMask = mask;
    if (changed) {
        DepthDrawLayerCleanupOperation operation;
        operation.kind = DepthDrawLayerCleanupKind::ContourRepair;
        operation.contourThickness = thickness;
        layer->cleanupOperations.append(operation);
        markLayerPreviewDirty(layerId);
    }
    summary.succeeded = true;
    return summary;
}
bool DepthDrawSession::replayLayerCleanupOperations(DepthDrawString layerId) {
    const auto receiver = weak_from_this().lock();
    auto layer = layerById(layerId);
    if (!layer || !layer->hasDepthPixels()) return false;
    auto operations = cloneCleanupOperations(layer->cleanupOperations);
    layer->cleanupOperations.clear();
    auto layerIndex = narrowIndex(findLayerIndex(layerId));
    for (auto& operation : operations) {
        switch (operation.kind) {
            case DepthDrawLayerCleanupKind::AlphaDepthGapFill: {
                auto rules = operation.focusedRules.duplicate();
                for (auto& rule : rules) rule.layerIndex = layerIndex;
                applyLayerAlphaDepthGapFill(layerId, rules);
                break;
            }
            case DepthDrawLayerCleanupKind::ContourRepair:
                operation.contourThickness = ngNormalizeDepthDrawContourThickness(operation.contourThickness);
                repairLayerContourDepth(layerId, operation.contourThickness);
                break;
            default: throw compat::ngStaticError<compat::SwitchError>(compat::RetainedString::fromStaticLiteral(
                "No appropriate switch clause found"), compat::RetainedString::fromStaticLiteral(
                    "source/nijigenerate/viewport/depth/draw/session.d"), 375);
        }
    }
    layer = layerById(layerId);
    if (!layer) return false;
    layer->cleanupOperations = operations;
    if (operations.size() > 0) markLayerPreviewDirty(layerId);
    return true;
}
bool DepthDrawSession::updateBindingSampling(DepthDrawString layerId, std::uint64_t targetGridUuid,
    bool useNormalLayerAlpha, float coverageThreshold) {
    const auto receiver = weak_from_this().lock();
    auto currentBindings = bindings;
    for (auto& binding : currentBindings) {
        if (!equal(binding.layerId, layerId) || binding.targetGridUuid != targetGridUuid) continue;
        binding.useNormalLayerAlpha = useNormalLayerAlpha;
        binding.coverageThreshold = coverageThreshold;
        markBindingPreviewDirty(binding);
        return true;
    }
    return false;
}
bool DepthDrawSession::updateBindingState(DepthDrawString layerId, std::uint64_t targetGridUuid,
    bool enabled, int order, DepthMergePolicy mergePolicy) {
    const auto receiver = weak_from_this().lock();
    auto currentBindings = bindings;
    for (auto& binding : currentBindings) {
        if (!equal(binding.layerId, layerId) || binding.targetGridUuid != targetGridUuid) continue;
        binding.enabled = enabled;
        binding.order = order;
        binding.mergePolicy = mergePolicy;
        markTargetPreviewDirty(targetGridUuid);
        return true;
    }
    return false;
}
bool DepthDrawSession::normalizeGpuPreviewSampling() {
    const auto receiver = weak_from_this().lock();
    if (!display.useGpuPreview) return false;
    bool changed = false;
    auto currentLayers = layers;
    for (auto& layer : currentLayers) {
        if (layer.convolution != DepthImageConvolution::MedianCustom) continue;
        layer.convolution = DepthImageConvolution::Median3x3;
        markLayerPreviewDirty(layer.id);
        changed = true;
    }
    return changed;
}
bool DepthDrawSession::updateDisplayOptions(DepthDrawDisplayOptions nextDisplay) {
    const auto receiver = weak_from_this().lock();
    auto displayChanged = display != nextDisplay;
    display = nextDisplay;
    auto samplingChanged = normalizeGpuPreviewSampling();
    if (!displayChanged && !samplingChanged) return false;
    markAllPreviewDirty();
    return true;
}
void DepthDrawSession::clearSelection() { selectedLayerId.reset(); selectedGridUuid = 0; }
void DepthDrawSession::clear() {
    const auto receiver = weak_from_this().lock();
    layers.clear(); bindings.clear(); sourceIdentity.reset(); documentWidth = 0; documentHeight = 0;
    clearSelection(); clearPreviewDirty(); previewTargetRevisions.reset(); bumpDiagnosticsRevision();
}
namespace {
std::string reloadLayerKey(const DepthDrawLayer layer) {
    const auto pattern = compat::RetainedString::fromStaticLiteral("%s\0%s\0%s\0%s");
    const std::array arguments{
        core::translation::ngFormatArgument(layer.layerPath),
        core::translation::ngFormatArgument(layer.displayName),
        core::translation::ngFormatArgument(layer.width),
        core::translation::ngFormatArgument(layer.height)};
    return core::translation::ngFormatTyped(pattern.view(), arguments);
}
bool isFlatDepthDrawLayerSource(const DepthDrawLayer layer) {
    auto path = text(layer.sourcePath);
    auto separator = [](char c) {
#ifdef _WIN32
        return c == '/' || c == '\\' || c == ':';
#else
        return c == '/';
#endif
    };
    auto dot = std::string_view::npos;
    for (std::size_t i = path.size(); i > 0;) {
        --i;
        if (separator(path[i])) break;
        if (path[i] == '.' && i > 0 && !separator(path[i - 1])) { dot = i; break; }
    }
    if (dot == std::string_view::npos) return false;
    const auto extension = compat::RetainedStringSlice(layer.sourcePath).slice(dot, path.size());
    const auto loweredExtension = io::imageexport::detail::lowerExtension(extension);
    const auto ext = loweredExtension.view();
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
}
std::shared_ptr<DepthDrawLayer> findReloadLayerByKey(std::shared_ptr<DepthDrawSession> session,
    const DepthDrawLayer previousLayer, StringMap<bool> usedReloadedLayerIds) {
    if (!session) return {};
    auto key = reloadLayerKey(previousLayer);
    auto currentLayers = session->layers;
    for (auto& layer : currentLayers) {
        if (!usedReloadedLayerIds.contains(text(layer.id)) && reloadLayerKey(layer) == key)
            return std::shared_ptr<DepthDrawLayer>(currentLayers.retentionOwner(), &layer);
    }
    if (session->layers.size() == 1 && isFlatDepthDrawLayerSource(previousLayer) &&
        isFlatDepthDrawLayerSource(session->layers[0]) &&
        !usedReloadedLayerIds.contains(text(session->layers[0].id))) {
        auto selectedLayers = session->layers;
        auto* selected = &sourceAt(selectedLayers, 0, 479);
        return std::shared_ptr<DepthDrawLayer>(selectedLayers.retentionOwner(), selected);
    }
    return {};
}
}
DepthDrawReloadStateResult ngDepthDrawCarryReloadState(std::shared_ptr<DepthDrawSession> reloaded,
    std::shared_ptr<DepthDrawSession> previous) {
    DepthDrawReloadStateResult result;
    if (!reloaded || !previous) return result;
    reloaded->display = previous->display;
    StringMap<DepthDrawString> layerIdMap;
    StringMap<bool> usedReloadedLayerIds;
    const auto previousLayers = previous->layers;
    for (auto previousLayer : previousLayers) {
        auto layer = findReloadLayerByKey(reloaded, previousLayer, usedReloadedLayerIds);
        if (!layer) continue;
        auto reloadedId = layer->id;
        usedReloadedLayerIds[reloadedId] = true;
        layer->visible = previousLayer.visible; layer->enabled = previousLayer.enabled;
        layer->xyOffset = previousLayer.xyOffset; layer->xyScale = previousLayer.xyScale;
        layer->zOffset = previousLayer.zOffset; layer->zScale = previousLayer.zScale;
        layer->backDepth = previousLayer.backDepth; layer->frontDepth = previousLayer.frontDepth;
        layer->invert = previousLayer.invert; layer->channel = previousLayer.channel;
        layer->sampleDepthScale = previousLayer.sampleDepthScale; layer->convolution = previousLayer.convolution;
        layer->customRadius = previousLayer.customRadius; layer->alphaThreshold = previousLayer.alphaThreshold;
        layer->cleanupOperations = cloneCleanupOperations(previousLayer.cleanupOperations);
        reloaded->replayLayerCleanupOperations(reloadedId);
        layerIdMap[previousLayer.id] = reloadedId;
        ++result.matchedLayers;
    }
    reloaded->bindings.clear();
    const auto previousBindings = previous->bindings;
    for (auto binding : previousBindings) {
        auto mappedId = layerIdMap.find(text(binding.layerId));
        if (!mappedId) continue;
        auto preserved = binding;
        preserved.layerId = *mappedId;
        reloaded->bindings.append(preserved);
        ++result.preservedBindings;
    }
    reloaded->selectedLayerId.reset();
    auto mappedSelection = layerIdMap.find(text(previous->selectedLayerId));
    if (mappedSelection && reloaded->layerById(*mappedSelection)) {
        reloaded->selectedLayerId = *mappedSelection;
        result.preservedSelectedLayer = true;
    }
    reloaded->selectedGridUuid = 0;
    if (previous->selectedGridUuid != 0 && reloaded->hasTargetGrid(previous->selectedGridUuid)) {
        reloaded->selectedGridUuid = previous->selectedGridUuid;
        result.preservedSelectedTarget = true;
    }
    reloaded->normalizeGpuPreviewSampling(); reloaded->markAllPreviewDirty();
    return result;
}
} // namespace nijigenerate::viewport::depth::draw
