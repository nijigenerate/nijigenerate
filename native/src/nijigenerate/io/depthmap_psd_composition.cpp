// Independent depthmap_psd.d composition state and pixel resolution.
#include "depthmap_psd_composition.hpp"
#include "detail/psd_pixel_copy.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>

namespace nijigenerate::io::depthmap_psd {
namespace detail {
PsdDepthCompositeSourceLayer cloneCompositeSourceLayer(PsdDepthCompositeSourceLayer layer) {
    layer.rgba = layer.rgba.duplicate();
    layer.maskRgba = layer.maskRgba.duplicate();
    return layer;
}
PsdDepthCompositeSource cloneCompositeSource(PsdDepthCompositeSource source) {
    const auto layers = source.layers;
    source.layers.resize(layers.size());
    // D assigns length without an unconditional dup; preserve the aliasing.
    std::size_t i = 0;
    for (auto sourceLayers = layers; auto layer : sourceLayers) {
        compat::ngArrayAt(source.layers, i) = cloneCompositeSourceLayer(layer);
        ++i;
    }
    return source;
}
PsdDepthComposedLayer cloneComposedLayer(PsdDepthComposedLayer layer) {
    layer.colorRgba = layer.colorRgba.duplicate();
    layer.depthRgba = layer.depthRgba.duplicate();
    layer.maskRgba = layer.maskRgba.duplicate();
    layer.coverageMaskRgba = layer.coverageMaskRgba.duplicate();
    return layer;
}
void copyComposedLayerState(PsdDepthComposedLayer& layer, PsdDepthComposedLayer previous) {
    layer.visible = previous.visible;
    if (layer.targetGridUuid != 0 && layer.targetGridUuid == previous.targetGridUuid)
        layer.enabled = previous.enabled;
    layer.depthEnabled = previous.depthEnabled;
    layer.invert = previous.invert;
    layer.depthOffset = previous.depthOffset;
    layer.depthScale = previous.depthScale;
    layer.outlierPruneEnabled = previous.outlierPruneEnabled;
    layer.puppetFitEnabled = previous.puppetFitEnabled;
}
void applySingleLayerReplacementFallback(PsdDepthImportResult& result, PsdDepthImportResult& previous) {
    if (result.composedLayers.size() != 1 || previous.composedLayers.size() != 1) return;
    copyComposedLayerState(compat::ngArrayAt(result.composedLayers, 0),
        compat::ngArrayAt(previous.composedLayers, 0));
    if (compat::ngArrayAt(result.composedLayers, 0).targetGridUuid == 0 &&
        compat::ngArrayAt(previous.composedLayers, 0).targetGridUuid != 0) {
        compat::ngArrayAt(result.composedLayers, 0).targetGridUuid =
            compat::ngArrayAt(previous.composedLayers, 0).targetGridUuid;
        compat::ngArrayAt(result.composedLayers, 0).targetGridName =
            compat::ngArrayAt(previous.composedLayers, 0).targetGridName;
        compat::ngArrayAt(result.composedLayers, 0).puppetBindingOverride =
            compat::ngArrayAt(previous.composedLayers, 0).puppetBindingOverride;
    }
    ngPsdDepthRefreshDerivedState(result);
}
}
namespace {
bool sameComposedLayerIdentity(PsdDepthComposedLayer layer, PsdDepthComposedLayer previous) {
    if (layer.layerPath.empty() || layer.layerPath != previous.layerPath) return false;
    if (layer.targetGridUuid != 0 || previous.targetGridUuid != 0)
        return layer.targetGridUuid != 0 && layer.targetGridUuid == previous.targetGridUuid;
    return true;
}
PsdDepthComposedLayer* findPreviousComposedLayerState(compat::Slice<PsdDepthComposedLayer> previous,
    PsdDepthComposedLayer layer) {
    for (auto& old : previous) if (sameComposedLayerIdentity(layer, old)) return &old;
    return nullptr;
}
float computeGlobalDepthCentroid(compat::Slice<PsdDepthComposedLayer> layers) {
    double sum = 0.0;
    std::size_t count = 0;
    for (auto layer : layers) {
        if (!layer.visible || !layer.depthEnabled || layer.depthRgba.size() < 4) continue;
        for (std::size_t i = 0; i < layer.depthRgba.size() / 4; ++i) {
            const auto index = i * 4;
            if (layer.maskRgba.size() >= index + 4 && layer.maskRgba[index + 3] == 0) continue;
            if (layer.depthRgba[index + 3] == 0) continue;
            const auto depth = layer.depthRgba[index];
            if (depth == 0) continue;
            const float scale = std::isfinite(layer.depthScale) ? layer.depthScale : 1.0f;
            const float offset = std::isfinite(layer.depthOffset) ? layer.depthOffset : 0.0f;
            const auto scaled = compat::ngFloatToInt32(std::round(float(depth) * scale + offset));
            sum += std::min(255, std::max(1, scaled));
            ++count;
        }
    }
    return count > 0 ? float(sum / double(count)) : 0.0f;
}
bool resolvedPixelAt(PsdDepthImportResult& imported, std::size_t layerIndex,
    std::int32_t documentX, std::int32_t documentY, std::ptrdiff_t& sourceLayerIndex,
    std::size_t& sourceRgbaIndex, std::array<std::uint8_t, 4>& pixel) {
    sourceLayerIndex = -1;
    sourceRgbaIndex = 0;
    pixel.fill(0);
    const auto copyPixel = [&](const auto& data, std::size_t first, std::size_t line) {
        const auto last = first + 4;
        if (first > last || last > data.size())
            throw compat::ngStaticError<compat::ArraySliceError>(first, last, data.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), line);
        detail::copyPsdPixel(pixel.data(), data.data() + first);
    };
    if (layerIndex >= imported.composedLayers.size()) return false;
    auto& layer = imported.composedLayers[layerIndex];
    if (!layer.enabled || !layer.visible) return false;
    if (layer.depthEnabled) {
        std::size_t index;
        if (!ngPsdDepthComposedLayerSurfaceCoversDocumentPixel(layer, documentX, documentY, index) ||
            index + 3 >= layer.depthRgba.size() || layer.depthRgba[index + 3] == 0) return false;
        sourceLayerIndex = std::bit_cast<std::ptrdiff_t>(layerIndex);
        sourceRgbaIndex = index;
        copyPixel(layer.depthRgba, index, 728);
        return true;
    }
    for (auto i = layerIndex; i-- > 0;) {
        auto& candidate = imported.composedLayers[i];
        if (!candidate.enabled || !candidate.visible || !candidate.depthEnabled) continue;
        std::size_t index;
        if (!ngPsdDepthComposedLayerSurfaceCoversDocumentPixel(candidate, documentX, documentY, index)) continue;
        sourceLayerIndex = std::bit_cast<std::ptrdiff_t>(i);
        if (index + 3 < candidate.depthRgba.size() && candidate.depthRgba[index + 3] != 0) {
            sourceRgbaIndex = index;
            copyPixel(candidate.depthRgba, index, 743);
            return true;
        }
        sourceLayerIndex = -1;
    }
    return false;
}
}
void ngPsdDepthApplyPreviousComposedLayerState(PsdDepthImportResult& result, PsdDepthImportResult& previous) {
    ngPsdDepthApplyPreviousComposedSourceState(result, ngPsdDepthComposedSourceFromImportResult(previous));
}
void ngPsdDepthApplyPreviousComposedSourceState(PsdDepthImportResult& result, PsdDepthComposedSource previous) {
    for (auto layers = result.composedLayers; auto& layer : layers)
        if (auto* old = findPreviousComposedLayerState(previous.layers, layer)) detail::copyComposedLayerState(layer, *old);
    ngPsdDepthRefreshDerivedState(result);
}
PsdDepthComposedSource ngPsdDepthComposedSourceFromImportResult(PsdDepthImportResult& result) {
    PsdDepthComposedSource source;
    source.mode = result.compositionMode;
    source.modeName = result.compositionModeName;
    source.width = result.compositionWidth;
    source.height = result.compositionHeight;
    source.globalDepthScale = result.globalDepthScale;
    source.globalDepthCentroid = result.globalDepthCentroid;
    source.colorSource = detail::cloneCompositeSource(result.colorSource);
    source.depthSource = detail::cloneCompositeSource(result.depthSource);
    source.layers.resize(result.composedLayers.size());
    const auto layers = result.composedLayers;
    const auto layerAt = [](auto& values, std::size_t index) -> decltype(auto) {
        if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/io/depthmap_psd.d"), 657);
        return values[index];
    };
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const auto layer = layerAt(layers, i);
        auto destinationLayers = source.layers;
        auto& destination = layerAt(destinationLayers, i);
        destination = detail::cloneComposedLayer(layer);
    }
    source.diagnostics = result.compositionDiagnostics.duplicate();
    return source;
}
bool ngPsdDepthMappingAccountsForColorLayer(PsdDepthLayerMapping mapping) {
    return mapping.matched || mapping.ignored;
}
bool ngPsdDepthComposedLayerSurfaceCoversDocumentPixel(PsdDepthComposedLayer& layer,
    std::int32_t documentX, std::int32_t documentY, std::size_t& rgbaIndex) {
    rgbaIndex = 0;
    const auto x = std::bit_cast<std::int32_t>(std::uint32_t(documentX) - std::uint32_t(layer.left));
    const auto y = std::bit_cast<std::int32_t>(std::uint32_t(documentY) - std::uint32_t(layer.top));
    if (x < 0 || y < 0 || x >= layer.width || y >= layer.height) return false;
    rgbaIndex = (std::size_t(y) * std::size_t(layer.width) + std::size_t(x)) * 4;
    const auto coverage = layer.coverageMaskRgba.size() >= rgbaIndex + 4 ? layer.coverageMaskRgba : layer.maskRgba;
    if (coverage.size() >= rgbaIndex + 4 && coverage[rgbaIndex + 3] == 0) return false;
    if (layer.colorRgba.size() >= rgbaIndex + 4 && layer.colorRgba[rgbaIndex + 3] < 3) return false;
    return true;
}
bool ngPsdDepthResolvedPixelAt(PsdDepthImportResult& imported, std::size_t layerIndex,
    std::int32_t documentX, std::int32_t documentY, std::ptrdiff_t& sourceLayerIndex,
    std::array<std::uint8_t, 4>& pixel) {
    std::size_t index;
    return resolvedPixelAt(imported, layerIndex, documentX, documentY, sourceLayerIndex, index, pixel);
}
std::ptrdiff_t ngPsdDepthResolvedLayerIndexAt(PsdDepthImportResult& imported, std::size_t layerIndex,
    std::int32_t documentX, std::int32_t documentY, std::size_t& rgbaIndex) {
    std::ptrdiff_t index;
    std::array<std::uint8_t, 4> pixel;
    return resolvedPixelAt(imported, layerIndex, documentX, documentY, index, rgbaIndex, pixel) ? index : -1;
}
void ngPsdDepthRefreshDerivedState(PsdDepthImportResult& result) {
    result.globalDepthCentroid = computeGlobalDepthCentroid(result.composedLayers);
}
}
