#include <nijigenerate/viewport/depth/draw/composer.hpp>
#include <nijigenerate/viewport/depth/draw/binding.hpp>
#include <nijigenerate/viewport/depth/draw/session.hpp>
#include <nijigenerate/viewport/depth/draw/coordinate.hpp>
#include <nijigenerate/viewport/depth/common/targetview.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <nodes/deformable.hpp>
#include <common/shared_array.hpp>
#include <algorithm>
#include <cmath>

namespace nijigenerate::viewport::depth::draw {
namespace {
struct ComposeLayer {
    DepthDrawLayer layer;
    DepthDrawBinding binding;
    std::size_t bindingIndex{};
};

void includeDepthRange(DepthDrawLayerComposeStats& stats, float depth) {
    if (!std::isfinite(depth)) return;
    if (!stats.hasDepthRange) {
        stats.hasDepthRange = true;
        stats.minDepth = depth;
        stats.maxDepth = depth;
        return;
    }
    stats.minDepth = std::min(stats.minDepth, depth);
    stats.maxDepth = std::max(stats.maxDepth, depth);
}

void includeDepthRange(DepthDrawComposeResult& result, float depth) {
    if (!std::isfinite(depth)) return;
    if (!result.hasDepthRange) {
        result.hasDepthRange = true;
        result.minDepth = depth;
        result.maxDepth = depth;
        return;
    }
    result.minDepth = std::min(result.minDepth, depth);
    result.maxDepth = std::max(result.maxDepth, depth);
}

bool mergeDepth(DepthMergePolicy policy, float& current, bool& hasComposed,
                float baseDepth, float sampledDepth) {
    switch (policy) {
    case DepthMergePolicy::Replace:
        current = sampledDepth;
        hasComposed = true;
        return true;
    case DepthMergePolicy::Frontmost:
        if (!hasComposed || sampledDepth > current) {
            current = sampledDepth;
            hasComposed = true;
            return true;
        }
        return false;
    case DepthMergePolicy::Backmost:
        if (!hasComposed || sampledDepth < current) {
            current = sampledDepth;
            hasComposed = true;
            return true;
        }
        return false;
    case DepthMergePolicy::Add:
        current = (hasComposed ? current : baseDepth) + sampledDepth;
        hasComposed = true;
        return true;
    case DepthMergePolicy::KeepExistingWhereMissing:
        if (!hasComposed) {
            current = sampledDepth;
            hasComposed = true;
            return true;
        }
        return false;
    }
    throw compat::ngStaticError<compat::SwitchError>(
        compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/composer.d"), 51);
}

} // namespace
DepthDrawComposeResult ngComposeDepthDrawTarget(std::shared_ptr<DepthDrawSession> session,
    std::shared_ptr<common::targetview::DepthTargetView> target, std::int32_t width, std::int32_t height) {
    DepthDrawComposeResult result;
    if (!session || !target) return result;
    const auto grid = target->getTarget();
    result.targetGridUuid = compat::ngRequireObject(grid)->getUUID();
    auto baseDepths = target->baseDepths.duplicate();
    const auto vertices = target->getVertices();
    if (baseDepths.size() != vertices.size()) {
        const auto old = baseDepths.size();
        baseDepths.resize(vertices.size());
        for (std::size_t i = old; i < baseDepths.size(); ++i) baseDepths[i] = 0.0f;
    }
    result.depths = baseDepths.duplicate();
    result.winningLayerIds.resize(vertices.size());
    nicxlive::core::common::SharedArray<bool> hasComposed;
    hasComposed.resize(vertices.size());
    compat::Slice<ComposeLayer> layers;
    const auto bindings = session->bindingsForGrid(grid->getUUID());
    for (std::size_t i = 0; i < bindings.size(); ++i) {
        const auto binding = bindings[i];
        const std::shared_ptr<const DepthDrawLayer> layer = session->layerById(binding.layerId);
        if (!layer || !layer->enabled || !layer->visible || !layer->hasDepthPixels()) continue;
        layers.append({*layer, binding, i});
    }
    if (layers.size() > 1) {
        std::sort(layers.begin(), layers.end(), [](const ComposeLayer& a, const ComposeLayer& b) {
            return a.binding.order == b.binding.order
                ? a.bindingIndex < b.bindingIndex : a.binding.order < b.binding.order;
        });
    }
    for (auto entry : layers) {
        const auto layer = entry.layer;
        const auto binding = entry.binding;
        DepthDrawLayerComposeStats stats;
        stats.layerId = layer.id;
        stats.additiveMerge = binding.mergePolicy == DepthMergePolicy::Add;
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            const auto point = coordinate::ngDepthDrawLayerPixelFromVertex(target, layer, vertices[i], width, height);
            auto settings = layer.sampleSettings();
            settings.alphaThreshold = std::max(settings.alphaThreshold, binding.coverageThreshold);
            const auto sample = binding.useNormalLayerAlpha && layer.hasNormalCoverage()
                ? io::depthimage::ngDepthImageSampleRgbaWithOpacityAndCoverageSource(
                    layer.depthPixels, layer.width, layer.height,
                    layer.normalCoverage, layer.width, layer.height,
                    layer.opacity, 1.0f, point.x, point.y, settings)
                : io::depthimage::ngDepthImageSampleRgbaWithOpacitySource(
                    layer.depthPixels, layer.width, layer.height,
                    point.x, point.y, layer.opacity, settings);
            if (!sample.valid) {
                ++stats.missingVertices; ++result.missingVertices;
                continue;
            }
            const auto depth = layer.applyZTransform(sample.value);
            ++stats.sampledVertices; ++result.sampledVertices;
            includeDepthRange(stats, depth);
            if (mergeDepth(binding.mergePolicy, result.depths[i], hasComposed[i], baseDepths[i], depth)) {
                result.winningLayerIds[i] = layer.id;
                if (stats.additiveMerge) ++stats.contributedVertices;
            }
        }
        result.layerStats.append(stats);
    }
    for (auto winner : result.winningLayerIds) {
        if (winner.empty()) continue;
        for (auto& stats : result.layerStats) if (stats.layerId == winner) {
            ++stats.winningVertices;
            if (!stats.additiveMerge) ++stats.contributedVertices;
            break;
        }
    }
    for (auto depth : result.depths) includeDepthRange(result, depth);
    return result;
}
DepthDrawComposeResult ngPreviewDepthDrawTarget(std::shared_ptr<DepthDrawSession> session,
    std::shared_ptr<common::targetview::DepthTargetView> target, std::int32_t width, std::int32_t height) {
    auto result = ngComposeDepthDrawTarget(session, target, width, height);
    if (target && !result.depths.empty()) {
        target->replaceWorkingDepths(result.depths);
        if (session) session->clearTargetPreviewDirty(result.targetGridUuid);
    }
    return result;
}
} // namespace nijigenerate::viewport::depth::draw

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::ComposeLayer> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.draw.composer.ComposeLayer)";
    inline static constexpr char unqualifiedArgument[] = "(ComposeLayer)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.draw.composer.ComposeLayer";
    inline static constexpr char unqualifiedElement[] = "ComposeLayer";
};
} // namespace nicxlive::core::d_runtime
