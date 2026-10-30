#include "gpu_internal.hpp"
#include "gpu_compose_sort.hpp"
#include "coordinate.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nodes/deformable.hpp>
#include <algorithm>
#include <cmath>

namespace nijigenerate::viewport::depth::draw {
using namespace gpu_detail;
using nicxlive::core::math::Vec2;
namespace {
template<class T> compat::Slice<T> sourceSlice(const compat::Slice<T>& values,
    std::size_t first, std::size_t last, std::size_t line) {
    if (first > last || last > values.size())
        throw compat::ngStaticError<compat::ArraySliceError>(first, last, values.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/gpu.d"), line);
    return values.slice(first, last);
}
}
namespace gpu_detail {
DepthDrawGpuComposePacket cloneDepthDrawGpuPacket(const DepthDrawGpuComposePacket& p) {
    DepthDrawGpuComposePacket result;
    result.targetGridUuid = p.targetGridUuid;
    result.documentWidth = p.documentWidth;
    result.documentHeight = p.documentHeight;
    result.vertices = p.vertices.duplicate();
    result.documentPositions = p.documentPositions.duplicate();
    result.baseDepths = p.baseDepths.duplicate();
    result.layers = p.layers.duplicate();
    result.bindings = p.bindings.duplicate();
    result.depthPixels = p.depthPixels.duplicate();
    result.normalCoveragePixels = p.normalCoveragePixels.duplicate();
    return result;
}
}
DepthDrawGpuLayerSamplePacket ngBuildDepthDrawGpuLayerSamplePacket(const DepthDrawGpuComposePacket& p,
    std::size_t index) {
    validateDepthDrawGpuPacket(p);
    enforce(index < p.layers.size(), "DepthDraw GPU layer sample index is out of bounds", 149);
    const auto layer = p.layers[index];
    const auto binding = p.bindings[index];
    const auto pixelCount = std::size_t(layer.width) * std::size_t(layer.height) * 4;
    DepthDrawGpuLayerSamplePacket r;
    r.targetGridUuid = p.targetGridUuid; r.layerIndex = static_cast<std::uint32_t>(index);
    r.layer = layer; r.layer.depthPixelOffset = 0;
    r.layer.normalCoverageOffset = layer.normalCoverageOffset == UINT32_MAX ? UINT32_MAX : 0;
    r.binding = binding; r.binding.layerIndex = 0;
    r.documentPositions = p.documentPositions.duplicate();
    r.depthPixels = sourceSlice(p.depthPixels, layer.depthPixelOffset,
        std::size_t(layer.depthPixelOffset) + pixelCount, 163).duplicate();
    if (layer.normalCoverageOffset != UINT32_MAX)
        r.normalCoveragePixels = sourceSlice(p.normalCoveragePixels, layer.normalCoverageOffset,
            std::size_t(layer.normalCoverageOffset) + pixelCount, 167).duplicate();
    return r;
}
void ngValidateDepthDrawGpuLayerSamplePacket(const DepthDrawGpuLayerSamplePacket& p) {
    validateDepthDrawGpuLayerSamplePacket(p);
}
void ngValidateDepthDrawGpuComposePacket(const DepthDrawGpuComposePacket& p) { validateDepthDrawGpuPacket(p); }
void ngValidateDepthDrawGpuComposeReadback(const DepthDrawGpuComposePacket& p, const DepthDrawGpuComposeReadback& r) {
    validateDepthDrawGpuReadback(p, r);
}
DepthDrawGpuLayerSampleUpload ngBuildDepthDrawGpuLayerSampleUpload(const DepthDrawGpuLayerSamplePacket& p) {
    validateDepthDrawGpuLayerSamplePacket(p);
    DepthDrawGpuLayerSampleUpload result;
    result.documentPositions = ngFlattenDepthDrawGpuDocumentPositions(p.documentPositions);
    result.layer = ngFlattenDepthDrawGpuLayers(compat::Slice<DepthDrawGpuLayerPacket>{p.layer});
    result.binding = ngFlattenDepthDrawGpuBindings(compat::Slice<DepthDrawGpuBindingPacket>{p.binding});
    result.depthPixels = p.depthPixels;
    if (!p.normalCoveragePixels.empty()) result.normalCoveragePixels = p.normalCoveragePixels;
    return result;
}
compat::Slice<float> ngFlattenDepthDrawGpuDocumentPositions(compat::ConstSlice<Vec2> positions) {
    compat::Slice<float> r; r.resize(positions.size() * DepthDrawGpuDocumentPositionStride);
    for (std::size_t i = 0; i < positions.size(); ++i) {
        r[i * DepthDrawGpuDocumentPositionStride] = positions[i].x;
        r[i * DepthDrawGpuDocumentPositionStride + 1] = positions[i].y;
    }
    return r;
}
compat::Slice<float> ngFlattenDepthDrawGpuLayers(compat::ConstSlice<DepthDrawGpuLayerPacket> layers) {
    compat::Slice<float> result; result.resize(layers.size() * DepthDrawGpuLayerStride,
        std::numeric_limits<float>::quiet_NaN());
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const auto layer = layers[i]; const auto base = i * DepthDrawGpuLayerStride;
        const auto put = [&](DepthDrawGpuLayerField f, float v) { result[base + std::size_t(f)] = v; };
        put(DepthDrawGpuLayerField::Width, static_cast<float>(layer.width));
        put(DepthDrawGpuLayerField::Height, static_cast<float>(layer.height));
        put(DepthDrawGpuLayerField::DepthPixelOffset, static_cast<float>(layer.depthPixelOffset));
        put(DepthDrawGpuLayerField::NormalCoverageOffset, static_cast<float>(layer.normalCoverageOffset));
        put(DepthDrawGpuLayerField::Flags, static_cast<float>(layer.flags));
        put(DepthDrawGpuLayerField::BoundsLeft, layer.boundsLeft); put(DepthDrawGpuLayerField::BoundsTop, layer.boundsTop);
        put(DepthDrawGpuLayerField::BoundsWidth, layer.boundsWidth); put(DepthDrawGpuLayerField::BoundsHeight, layer.boundsHeight);
        put(DepthDrawGpuLayerField::Opacity, layer.opacity);
        put(DepthDrawGpuLayerField::XyOffsetX, layer.xyOffsetX); put(DepthDrawGpuLayerField::XyOffsetY, layer.xyOffsetY);
        put(DepthDrawGpuLayerField::XyScaleX, layer.xyScaleX); put(DepthDrawGpuLayerField::XyScaleY, layer.xyScaleY);
        put(DepthDrawGpuLayerField::ZOffset, layer.zOffset); put(DepthDrawGpuLayerField::ZScale, layer.zScale);
        put(DepthDrawGpuLayerField::BackDepth, layer.backDepth); put(DepthDrawGpuLayerField::FrontDepth, layer.frontDepth);
        put(DepthDrawGpuLayerField::AlphaThreshold, layer.alphaThreshold);
        put(DepthDrawGpuLayerField::Channel, static_cast<float>(layer.channel));
        put(DepthDrawGpuLayerField::Convolution, static_cast<float>(layer.convolution));
        put(DepthDrawGpuLayerField::CustomRadius, static_cast<float>(layer.customRadius));
        put(DepthDrawGpuLayerField::SampleDepthScale, layer.sampleDepthScale);
    }
    return result;
}
compat::Slice<float> ngFlattenDepthDrawGpuBindings(compat::ConstSlice<DepthDrawGpuBindingPacket> bindings) {
    compat::Slice<float> r; r.resize(bindings.size() * DepthDrawGpuBindingStride,
        std::numeric_limits<float>::quiet_NaN());
    for (std::size_t i = 0; i < bindings.size(); ++i) {
        const auto b = bindings[i]; const auto base = i * DepthDrawGpuBindingStride;
        r[base] = static_cast<float>(b.layerIndex); r[base + 1] = static_cast<float>(b.mergePolicy);
        r[base + 2] = static_cast<float>(b.flags); r[base + 3] = b.coverageThreshold;
    }
    return r;
}
namespace {
bool mergeDepthReadback(DepthMergePolicy policy, float& current, bool& composed, float base, float sample) {
    switch (policy) {
    case DepthMergePolicy::Replace: current = sample; composed = true; return true;
    case DepthMergePolicy::Frontmost:
        if (!composed || sample > current) { current = sample; composed = true; return true; } return false;
    case DepthMergePolicy::Backmost:
        if (!composed || sample < current) { current = sample; composed = true; return true; } return false;
    case DepthMergePolicy::Add: current = (composed ? current : base) + sample; composed = true; return true;
    case DepthMergePolicy::KeepExistingWhereMissing:
        if (!composed) { current = sample; composed = true; return true; } return false;
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/gpu.d"), 1339);
}
template<class Stats> void includeDepthRange(Stats& r, float depth) {
    if (!std::isfinite(depth)) return;
    if (!r.hasDepthRange) { r.hasDepthRange = true; r.minDepth = r.maxDepth = depth; return; }
    r.minDepth = std::min(r.minDepth, depth); r.maxDepth = std::max(r.maxDepth, depth);
}
}
DepthDrawGpuComposeReadback ngBuildDepthDrawGpuComposeReadback(const DepthDrawGpuComposePacket& p,
    compat::ConstSlice<DepthDrawGpuLayerReadback> layers) {
    validateDepthDrawGpuPacket(p);
    enforce(layers.size() == p.layers.size(), "DepthDraw GPU compose readback builder layer count mismatch", 1285);
    DepthDrawGpuComposeReadback r;
    r.targetGridUuid = p.targetGridUuid; r.depths = p.baseDepths.duplicate();
    r.winningLayerIndices.resize(p.vertices.size(), -1); r.layers.resize(layers.size());
    compat::Slice<bool> composed; composed.resize(p.vertices.size());
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const auto layer = layers[i];
        enforce(layer.layerIndex == i, "DepthDraw GPU compose readback builder layer index mismatch", 1298);
        enforce(layer.validSamples.size() == p.vertices.size(), "DepthDraw GPU compose readback builder valid-sample length mismatch", 1300);
        enforce(layer.sampleDepths.size() == p.vertices.size(), "DepthDraw GPU compose readback builder sample-depth length mismatch", 1302);
        r.layers[i] = {layer.layerIndex, layer.validSamples.duplicate(), layer.sampleDepths.duplicate()};
        const auto policy = static_cast<DepthMergePolicy>(p.bindings[i].mergePolicy);
        for (std::size_t v = 0; v < layer.validSamples.size(); ++v) {
            const auto valid = layer.validSamples[v];
            enforce(valid == 0 || valid == 1, "DepthDraw GPU compose readback builder valid-sample value must be 0 or 1", 1311);
            if (valid == 0) continue;
            const auto sample = layer.sampleDepths[v];
            enforce(std::isfinite(sample), "DepthDraw GPU compose readback builder contains non-finite sample depth", 1315);
            if (mergeDepthReadback(policy, r.depths[v], composed[v], p.baseDepths[v], sample))
                r.winningLayerIndices[v] = static_cast<std::int32_t>(i);
        }
    }
    validateDepthDrawGpuReadback(p, r); return r;
}
Vec2 ngDepthDrawGpuLayerPixelFromDocument(DepthDrawGpuLayerPacket layer, Vec2 p) {
    const auto sx = layer.xyScaleX == 0 ? 1 : layer.xyScaleX;
    const auto sy = layer.xyScaleY == 0 ? 1 : layer.xyScaleY;
    return {(p.x - layer.boundsLeft - layer.xyOffsetX) / sx, (p.y - layer.boundsTop - layer.xyOffsetY) / sy};
}
DepthDrawComposeResult ngDepthDrawComposeResultFromGpuReadback(DepthDrawGpuComposePacket p,
    DepthDrawGpuComposeReadback readback) {
    DepthDrawComposeResult r; r.targetGridUuid = readback.targetGridUuid; r.depths = readback.depths.duplicate();
    r.winningLayerIds.resize(readback.winningLayerIndices.size());
    for (std::size_t i = 0; i < readback.winningLayerIndices.size(); ++i) {
        const auto index = readback.winningLayerIndices[i];
        if (index >= 0 && std::size_t(index) < p.layers.size()) r.winningLayerIds[i] = p.layers[index].layerId;
    }
    for (auto layer : readback.layers) {
        DepthDrawLayerComposeStats stats;
        if (layer.layerIndex < p.layers.size()) {
            stats.layerId = p.layers[layer.layerIndex].layerId;
            if (layer.layerIndex < p.bindings.size())
                stats.additiveMerge = p.bindings[layer.layerIndex].mergePolicy == std::uint32_t(DepthMergePolicy::Add);
        }
        const auto count = std::min(layer.validSamples.size(), layer.sampleDepths.size());
        for (std::size_t i = 0; i < count; ++i) {
            if (!layer.validSamples[i]) { ++stats.missingVertices; ++r.missingVertices; continue; }
            ++stats.sampledVertices; ++r.sampledVertices; includeDepthRange(stats, layer.sampleDepths[i]);
            if (stats.additiveMerge) ++stats.contributedVertices;
        }
        if (layer.validSamples.size() > count) {
            stats.missingVertices += layer.validSamples.size() - count;
            r.missingVertices += layer.validSamples.size() - count;
        }
        r.layerStats.append(stats);
    }
    for (auto winner : r.winningLayerIds) {
        if (winner.empty()) continue;
        for (auto& stats : r.layerStats) if (stats.layerId == winner) {
            ++stats.winningVertices; if (!stats.additiveMerge) ++stats.contributedVertices; break;
        }
    }
    for (float depth : r.depths) includeDepthRange(r, depth);
    return r;
}
DepthDrawGpuComposePacket ngBuildDepthDrawGpuComposePacket(std::shared_ptr<DepthDrawSession> session,
    std::shared_ptr<common::targetview::DepthTargetView> target, std::int32_t width, std::int32_t height) {
    DepthDrawGpuComposePacket p;
    if (!session || !target) return p;
    const auto grid = target->getTarget();
    p.targetGridUuid = compat::ngRequireObject(grid)->getUUID(); p.documentWidth = width; p.documentHeight = height;
    p.vertices = target->getVertices(); p.documentPositions.resize(p.vertices.size());
    for (std::size_t i = 0; i < p.vertices.size(); ++i)
        p.documentPositions[i] = coordinate::ngDepthDrawVertexDocumentPosition(target, p.vertices[i], width, height);
    p.baseDepths = target->baseDepths.duplicate();
    if (p.baseDepths.size() != p.vertices.size()) {
        const auto old = p.baseDepths.size(); p.baseDepths.resize(p.vertices.size());
        for (std::size_t i = old; i < p.baseDepths.size(); ++i) p.baseDepths[i] = 0;
    }
    struct Entry { DepthDrawLayer layer; DepthDrawBinding binding; std::size_t index; };
    compat::Slice<Entry> layers;
    const auto bindings = session->bindingsForGrid(compat::ngRequireObject(grid)->getUUID());
    for (std::size_t i = 0; i < bindings.size(); ++i) {
        const auto binding = bindings[i];
        const std::shared_ptr<const DepthDrawLayer> layer = session->layerById(binding.layerId);
        if (!layer || !layer->enabled || !layer->visible || !layer->hasDepthPixels()) continue;
        layers.append({*layer, binding, i});
    }
    auto before = [](Entry a, Entry b) {
        return a.binding.order == b.binding.order ? a.index < b.index : a.binding.order < b.binding.order;
    };
    gpu_sort_detail::Sorter<Entry, decltype(before)>(before).sort({layers.data(), layers.size()});
    for (auto entry : layers) {
        const auto index = static_cast<std::uint32_t>(p.layers.size());
        const auto layer = entry.layer; const auto binding = entry.binding;
        const auto settings = layer.sampleSettings();
        DepthDrawGpuLayerPacket lp;
        lp.layerId = layer.id; lp.width = static_cast<std::uint32_t>(layer.width);
        lp.height = static_cast<std::uint32_t>(layer.height);
        lp.depthPixelOffset = static_cast<std::uint32_t>(p.depthPixels.size());
        lp.normalCoverageOffset = layer.hasNormalCoverage() ? static_cast<std::uint32_t>(p.normalCoveragePixels.size()) : UINT32_MAX;
        lp.flags = (layer.invert ? 1u : 0u) | (layer.hasNormalCoverage() ? 2u : 0u);
        lp.boundsLeft = static_cast<float>(layer.bounds.left); lp.boundsTop = static_cast<float>(layer.bounds.top);
        lp.boundsWidth = static_cast<float>(layer.bounds.width); lp.boundsHeight = static_cast<float>(layer.bounds.height);
        lp.opacity = layer.opacity; lp.xyOffsetX = layer.xyOffset.x; lp.xyOffsetY = layer.xyOffset.y;
        lp.xyScaleX = layer.xyScale.x; lp.xyScaleY = layer.xyScale.y;
        lp.zOffset = layer.zOffset; lp.zScale = layer.zScale;
        lp.backDepth = settings.backDepth; lp.frontDepth = settings.frontDepth;
        lp.sampleDepthScale = layer.sampleDepthScale; lp.alphaThreshold = settings.alphaThreshold;
        lp.channel = static_cast<std::int32_t>(settings.channel);
        lp.convolution = static_cast<std::int32_t>(settings.convolution); lp.customRadius = settings.customRadius;
        p.layers.append(lp); p.depthPixels.append(layer.depthPixels);
        if (layer.hasNormalCoverage()) p.normalCoveragePixels.append(layer.normalCoverage);
        DepthDrawGpuBindingPacket bp;
        bp.layerId = layer.id; bp.layerIndex = index; bp.mergePolicy = static_cast<std::uint32_t>(binding.mergePolicy);
        bp.flags = binding.useNormalLayerAlpha ? 1u : 0u; bp.coverageThreshold = binding.coverageThreshold;
        p.bindings.append(bp);
    }
    return p;
}
}
