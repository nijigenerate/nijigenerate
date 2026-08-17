module nijigenerate.viewport.depth.draw.composer;

import nijigenerate.io.depthimage : ngDepthImageSampleRgbaWithOpacity, ngDepthImageSampleRgbaWithOpacityAndCoverage;
import nijigenerate.viewport.depth.common.targetview;
import nijigenerate.viewport.depth.draw.binding;
import nijigenerate.viewport.depth.draw.coordinate;
import nijigenerate.viewport.depth.draw.layer;
import nijigenerate.viewport.depth.draw.session;
import nijilive;
import std.algorithm : max, min, sort;
import std.array : array;
import std.math : isFinite;

struct DepthDrawLayerComposeStats {
    string layerId;
    size_t sampledVertices;
    size_t missingVertices;
    size_t winningVertices;
    size_t contributedVertices;
    bool additiveMerge;
    bool hasDepthRange;
    float minDepth;
    float maxDepth;
}

struct DepthDrawComposeResult {
    ulong targetGridUuid;
    float[] depths;
    string[] winningLayerIds;
    DepthDrawLayerComposeStats[] layerStats;
    size_t sampledVertices;
    size_t missingVertices;
    bool hasDepthRange;
    float minDepth;
    float maxDepth;
}

private struct ComposeLayer {
    DepthDrawLayer layer;
    DepthDrawBinding binding;
    size_t bindingIndex;
}

private bool mergeDepth(
    DepthMergePolicy policy,
    ref float current,
    ref bool hasComposed,
    float baseDepth,
    float sampledDepth
) {
    final switch (policy) {
        case DepthMergePolicy.Replace:
            current = sampledDepth;
            hasComposed = true;
            return true;
        case DepthMergePolicy.Frontmost:
            if (!hasComposed || sampledDepth > current) {
                current = sampledDepth;
                hasComposed = true;
                return true;
            }
            return false;
        case DepthMergePolicy.Backmost:
            if (!hasComposed || sampledDepth < current) {
                current = sampledDepth;
                hasComposed = true;
                return true;
            }
            return false;
        case DepthMergePolicy.Add:
            current = (hasComposed ? current : baseDepth) + sampledDepth;
            hasComposed = true;
            return true;
        case DepthMergePolicy.KeepExistingWhereMissing:
            if (!hasComposed) {
                current = sampledDepth;
                hasComposed = true;
                return true;
            }
            return false;
    }
}

private void includeDepthRange(ref DepthDrawLayerComposeStats stats, float depth) {
    if (!depth.isFinite) return;
    if (!stats.hasDepthRange) {
        stats.hasDepthRange = true;
        stats.minDepth = depth;
        stats.maxDepth = depth;
        return;
    }
    stats.minDepth = min(stats.minDepth, depth);
    stats.maxDepth = max(stats.maxDepth, depth);
}

private void includeDepthRange(ref DepthDrawComposeResult result, float depth) {
    if (!depth.isFinite) return;
    if (!result.hasDepthRange) {
        result.hasDepthRange = true;
        result.minDepth = depth;
        result.maxDepth = depth;
        return;
    }
    result.minDepth = min(result.minDepth, depth);
    result.maxDepth = max(result.maxDepth, depth);
}

DepthDrawComposeResult ngComposeDepthDrawTarget(
    DepthDrawSession session,
    DepthTargetView target,
    int documentWidth,
    int documentHeight
) {
    DepthDrawComposeResult result;
    if (session is null || target is null) return result;

    auto grid = target.getTarget();
    result.targetGridUuid = grid.uuid;
    auto baseDepths = target.baseDepths.dup;
    auto vertices = target.getVertices();
    if (baseDepths.length != vertices.length) {
        auto oldLength = baseDepths.length;
        baseDepths.length = vertices.length;
        foreach (i; oldLength .. baseDepths.length) baseDepths[i] = 0.0f;
    }
    result.depths = baseDepths.dup;
    result.winningLayerIds.length = vertices.length;
    bool[] hasComposed;
    hasComposed.length = vertices.length;

    ComposeLayer[] composeLayers;
    foreach (bindingIndex, binding; session.bindingsForGrid(grid.uuid)) {
        auto layerPtr = session.layerById(binding.layerId);
        if (layerPtr is null || !layerPtr.enabled || !layerPtr.visible || !layerPtr.hasDepthPixels()) continue;
        ComposeLayer entry;
        entry.layer = *layerPtr;
        entry.binding = binding;
        entry.bindingIndex = bindingIndex;
        composeLayers ~= entry;
    }
    sort!((a, b) => a.binding.order == b.binding.order
        ? a.bindingIndex < b.bindingIndex
        : a.binding.order < b.binding.order)(composeLayers);

    foreach (composeLayer; composeLayers) {
        auto layer = composeLayer.layer;
        auto binding = composeLayer.binding;
        DepthDrawLayerComposeStats stats;
        stats.layerId = layer.id;
        stats.additiveMerge = binding.mergePolicy == DepthMergePolicy.Add;

        foreach (i, vertex; vertices) {
            auto layerPoint = ngDepthDrawLayerPixelFromVertex(target, layer, vertex, documentWidth, documentHeight);
            auto settings = layer.sampleSettings();
            settings.alphaThreshold = max(settings.alphaThreshold, binding.coverageThreshold);

            auto sample = binding.useNormalLayerAlpha && layer.hasNormalCoverage()
                ? ngDepthImageSampleRgbaWithOpacityAndCoverage(
                    layer.depthPixels,
                    layer.width,
                    layer.height,
                    layer.normalCoverage,
                    layer.width,
                    layer.height,
                    layer.opacity,
                    1.0f,
                    layerPoint.x,
                    layerPoint.y,
                    settings)
                : ngDepthImageSampleRgbaWithOpacity(
                    layer.depthPixels,
                    layer.width,
                    layer.height,
                    layerPoint.x,
                    layerPoint.y,
                    layer.opacity,
                    settings);
            if (!sample.valid) {
                stats.missingVertices++;
                result.missingVertices++;
                continue;
            }

            auto depth = layer.applyZTransform(sample.value);
            stats.sampledVertices++;
            result.sampledVertices++;
            includeDepthRange(stats, depth);

            auto won = mergeDepth(
                binding.mergePolicy,
                result.depths[i],
                hasComposed[i],
                baseDepths[i],
                depth
            );
            if (won) {
                result.winningLayerIds[i] = layer.id;
                if (stats.additiveMerge) stats.contributedVertices++;
            }
        }

        result.layerStats ~= stats;
    }

    foreach (winnerId; result.winningLayerIds) {
        if (winnerId.length == 0) continue;
        foreach (ref stats; result.layerStats) {
            if (stats.layerId == winnerId) {
                stats.winningVertices++;
                if (!stats.additiveMerge) stats.contributedVertices++;
                break;
            }
        }
    }

    foreach (depth; result.depths) includeDepthRange(result, depth);
    return result;
}

DepthDrawComposeResult ngPreviewDepthDrawTarget(
    DepthDrawSession session,
    DepthTargetView target,
    int documentWidth,
    int documentHeight
) {
    auto result = ngComposeDepthDrawTarget(session, target, documentWidth, documentHeight);
    if (target !is null && result.depths.length > 0) {
        target.replaceWorkingDepths(result.depths);
        if (session !is null) session.clearTargetPreviewDirty(result.targetGridUuid);
    }
    return result;
}
