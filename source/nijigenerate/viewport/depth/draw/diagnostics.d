module nijigenerate.viewport.depth.draw.diagnostics;

import nijigenerate.viewport.depth.draw.binding;
import nijigenerate.viewport.depth.draw.composer;
import nijigenerate.viewport.depth.draw.layer;
import nijigenerate.viewport.depth.draw.session;
import nijilive.math : vec4;
import std.algorithm : canFind, sort;
import std.conv : to;
import std.string : toLower;

enum DepthDrawLayerStackSortMode {
    SourceOrder,
    Name,
    Target,
    DepthMin,
    Sampled,
    Missing,
}

struct DepthDrawLayerStackRow {
    size_t sourceIndex;
    string layerId;
    string displayName;
    string layerPath;
    bool enabled;
    bool visible;
    bool hasDepthPixels;
    bool hasNormalCoverage;
    bool hasBinding;
    bool hasAmbiguousBindings;
    ulong targetGridUuid;
    string targetDisplayName;
    DepthMergePolicy mergePolicy;
    size_t sampledVertices;
    size_t missingVertices;
    size_t winningVertices;
    bool hasDepthRange;
    float minDepth;
    float maxDepth;
    bool warningMissingDepthPixels;
    bool warningMissingTarget;
    bool warningMissingCoverage;
    bool warningMostlyMissingSamples;
}

struct DepthDrawCompositePreviewPoint {
    size_t vertexIndex;
    float depth;
    string winningLayerId;
    vec4 winningLayerColor;
    bool sampled;
    bool missing;
}

struct DepthDrawCompositePreviewSummary {
    ulong targetGridUuid;
    size_t vertexCount;
    size_t sampledVertices;
    size_t missingVertices;
    size_t winningVertices;
    size_t finalSampledVertices;
    size_t finalMissingVertices;
    bool hasDepthRange;
    float minDepth;
    float maxDepth;
    DepthDrawCompositePreviewPoint[] points;
}

struct DepthDrawDepthSpaceLayerPlane {
    string layerId;
    string displayName;
    DepthDrawRect bounds;
    bool selected;
    bool visible;
    bool enabled;
    bool hasDepthRange;
    float minDepth;
    float maxDepth;
    float representativeDepth;
}

struct DepthDrawDepthSpaceGapMarker {
    string backLayerId;
    string frontLayerId;
    bool valid;
    bool overlap;
    float backDepth;
    float frontDepth;
}

struct DepthDrawDepthSpaceSummary {
    ulong selectedGridUuid;
    string selectedLayerId;
    DepthDrawDepthSpaceLayerPlane[] layers;
    DepthDrawDepthSpaceGapMarker[] gaps;
}

struct DepthDrawSourceDiagnostics {
    size_t totalLayers;
    size_t usableDepthLayers;
    size_t disabledLayers;
    size_t invisibleLayers;
    size_t layersMissingDepthPixels;
    size_t layersMissingTarget;
    size_t layersMissingCoverage;
    size_t layersWithAmbiguousBindings;
    size_t totalBindings;
    size_t enabledBindings;
}

DepthDrawSourceDiagnostics ngDepthDrawSourceDiagnostics(DepthDrawSession session) {
    DepthDrawSourceDiagnostics result;
    if (session is null) return result;

    result.totalLayers = session.layers.length;
    result.totalBindings = session.bindings.length;

    foreach (binding; session.bindings) {
        if (binding.enabled) result.enabledBindings++;
    }

    foreach (layer; session.layers) {
        if (!layer.enabled) result.disabledLayers++;
        if (!layer.visible) result.invisibleLayers++;
        if (!layer.hasDepthPixels()) {
            result.layersMissingDepthPixels++;
            continue;
        }

        result.usableDepthLayers++;
        if (!layer.hasNormalCoverage()) result.layersMissingCoverage++;

        size_t enabledBindingCount;
        foreach (binding; session.bindings) {
            if (!binding.enabled || binding.layerId != layer.id) continue;
            enabledBindingCount++;
        }

        if (enabledBindingCount == 0) {
            result.layersMissingTarget++;
        } else if (enabledBindingCount > 1) {
            result.layersWithAmbiguousBindings++;
        }
    }

    return result;
}

DepthDrawLayerStackRow[] ngDepthDrawLayerStackRows(
    DepthDrawSession session,
    const(DepthDrawComposeResult)* composeResult = null,
    string delegate(ulong) targetNameResolver = null
) {
    DepthDrawLayerStackRow[] rows;
    if (session is null) return rows;

    foreach (sourceIndex, layer; session.layers) {
        DepthDrawLayerStackRow row;
        row.sourceIndex = sourceIndex;
        row.layerId = layer.id;
        row.displayName = layer.displayName.length > 0 ? layer.displayName : layer.id;
        row.layerPath = layer.layerPath;
        row.enabled = layer.enabled;
        row.visible = layer.visible;
        row.hasDepthPixels = layer.hasDepthPixels();
        row.hasNormalCoverage = layer.hasNormalCoverage();
        row.warningMissingDepthPixels = !row.hasDepthPixels;
        row.warningMissingCoverage = row.hasDepthPixels && !row.hasNormalCoverage;

        size_t enabledBindingCount;
        foreach (binding; session.bindings) {
            if (!binding.enabled || binding.layerId != layer.id) continue;
            enabledBindingCount++;
            if (enabledBindingCount == 1) {
                row.targetGridUuid = binding.targetGridUuid;
                if (targetNameResolver !is null) {
                    row.targetDisplayName = targetNameResolver(binding.targetGridUuid);
                }
                row.mergePolicy = binding.mergePolicy;
            }
        }
        row.hasBinding = enabledBindingCount > 0;
        row.hasAmbiguousBindings = enabledBindingCount > 1;
        row.warningMissingTarget = row.hasDepthPixels && !row.hasBinding;

        if (composeResult !is null) {
            foreach (stats; composeResult.layerStats) {
                if (stats.layerId != layer.id) continue;
                row.sampledVertices += stats.sampledVertices;
                row.missingVertices += stats.missingVertices;
                row.winningVertices += stats.winningVertices;
                if (stats.hasDepthRange) {
                    if (!row.hasDepthRange) {
                        row.hasDepthRange = true;
                        row.minDepth = stats.minDepth;
                        row.maxDepth = stats.maxDepth;
                    } else {
                        if (stats.minDepth < row.minDepth) row.minDepth = stats.minDepth;
                        if (stats.maxDepth > row.maxDepth) row.maxDepth = stats.maxDepth;
                    }
                }
            }
        }
        row.warningMostlyMissingSamples = row.missingVertices > 0 && row.missingVertices > row.sampledVertices;
        rows ~= row;
    }

    return rows;
}

DepthDrawLayerStackRow[] ngDepthDrawFilterAndSortLayerStackRows(
    DepthDrawLayerStackRow[] rows,
    string filter,
    DepthDrawLayerStackSortMode sortMode = DepthDrawLayerStackSortMode.SourceOrder,
    bool descending = false
) {
    DepthDrawLayerStackRow[] result;
    auto needle = filter.toLower;
    foreach (row; rows) {
        if (needle.length) {
            auto haystack = (
                row.layerId ~ "\n" ~
                row.displayName ~ "\n" ~
                row.layerPath ~ "\n" ~
                row.targetDisplayName ~ "\n" ~
                (row.hasBinding ? row.targetGridUuid.to!string : "")
            ).toLower;
            if (!haystack.canFind(needle)) continue;
        }
        result ~= row;
    }

    final switch (sortMode) {
        case DepthDrawLayerStackSortMode.SourceOrder:
            break;
        case DepthDrawLayerStackSortMode.Name:
            sort!((a, b) {
                auto av = (a.displayName.length ? a.displayName : a.layerId).toLower;
                auto bv = (b.displayName.length ? b.displayName : b.layerId).toLower;
                if (av == bv) return a.sourceIndex < b.sourceIndex;
                return descending ? av > bv : av < bv;
            })(result);
            break;
        case DepthDrawLayerStackSortMode.Target:
            sort!((a, b) {
                auto av = (a.targetDisplayName.length ? a.targetDisplayName : a.targetGridUuid.to!string).toLower;
                auto bv = (b.targetDisplayName.length ? b.targetDisplayName : b.targetGridUuid.to!string).toLower;
                if (av == bv) return a.sourceIndex < b.sourceIndex;
                return descending ? av > bv : av < bv;
            })(result);
            break;
        case DepthDrawLayerStackSortMode.DepthMin:
            sort!((a, b) {
                auto av = a.hasDepthRange ? a.minDepth : float.max;
                auto bv = b.hasDepthRange ? b.minDepth : float.max;
                if (av == bv) return a.sourceIndex < b.sourceIndex;
                return descending ? av > bv : av < bv;
            })(result);
            break;
        case DepthDrawLayerStackSortMode.Sampled:
            sort!((a, b) {
                if (a.sampledVertices == b.sampledVertices) return a.sourceIndex < b.sourceIndex;
                return descending ? a.sampledVertices > b.sampledVertices : a.sampledVertices < b.sampledVertices;
            })(result);
            break;
        case DepthDrawLayerStackSortMode.Missing:
            sort!((a, b) {
                if (a.missingVertices == b.missingVertices) return a.sourceIndex < b.sourceIndex;
                return descending ? a.missingVertices > b.missingVertices : a.missingVertices < b.missingVertices;
            })(result);
            break;
    }
    return result;
}

DepthDrawCompositePreviewSummary ngDepthDrawCompositePreviewSummary(const(DepthDrawComposeResult) result) {
    DepthDrawCompositePreviewSummary summary;
    summary.targetGridUuid = result.targetGridUuid;
    summary.vertexCount = result.depths.length;
    summary.sampledVertices = result.sampledVertices;
    summary.missingVertices = result.missingVertices;
    summary.hasDepthRange = result.hasDepthRange;
    summary.minDepth = result.minDepth;
    summary.maxDepth = result.maxDepth;

    foreach (i, depth; result.depths) {
        DepthDrawCompositePreviewPoint point;
        point.vertexIndex = i;
        point.depth = depth;
        point.winningLayerId = i < result.winningLayerIds.length ? result.winningLayerIds[i] : null;
        point.sampled = point.winningLayerId.length > 0;
        point.missing = !point.sampled;
        if (point.sampled) point.winningLayerColor = ngDepthDrawWinningLayerColor(point.winningLayerId);
        if (point.sampled) {
            summary.winningVertices++;
            summary.finalSampledVertices++;
        } else {
            summary.finalMissingVertices++;
        }
        summary.points ~= point;
    }

    return summary;
}

vec4 ngDepthDrawWinningLayerColor(string layerId) {
    uint hash = 2166136261U;
    foreach (ubyte c; cast(const(ubyte)[])layerId) {
        hash ^= c;
        hash *= 16777619U;
    }
    auto r = 0.25f + cast(float)(hash & 0xFF) / 255.0f * 0.7f;
    auto g = 0.25f + cast(float)((hash >> 8) & 0xFF) / 255.0f * 0.7f;
    auto b = 0.25f + cast(float)((hash >> 16) & 0xFF) / 255.0f * 0.7f;
    return vec4(r, g, b, 1.0f);
}

DepthDrawDepthSpaceSummary ngDepthDrawDepthSpaceSummary(
    DepthDrawSession session,
    const(DepthDrawComposeResult)* composeResult = null
) {
    DepthDrawDepthSpaceSummary summary;
    if (session is null) return summary;
    summary.selectedGridUuid = session.selectedGridUuid;
    summary.selectedLayerId = session.selectedLayerId;

    auto rows = ngDepthDrawLayerStackRows(session, composeResult);
    foreach (i, row; rows) {
        auto layer = session.layerById(row.layerId);
        if (layer is null) continue;

        DepthDrawDepthSpaceLayerPlane plane;
        plane.layerId = row.layerId;
        plane.displayName = row.displayName;
        plane.bounds = layer.bounds;
        plane.selected = row.layerId == session.selectedLayerId;
        plane.visible = row.visible;
        plane.enabled = row.enabled;
        plane.hasDepthRange = row.hasDepthRange;
        plane.minDepth = row.minDepth;
        plane.maxDepth = row.maxDepth;
        plane.representativeDepth = row.hasDepthRange ? (row.minDepth + row.maxDepth) * 0.5f : 0.0f;
        summary.layers ~= plane;

        if (i == 0) continue;
        auto previous = rows[i - 1];
        DepthDrawDepthSpaceGapMarker gap;
        gap.backLayerId = previous.layerId;
        gap.frontLayerId = row.layerId;
        if (previous.hasDepthRange && row.hasDepthRange) {
            gap.backDepth = previous.maxDepth;
            gap.frontDepth = row.minDepth;
            gap.overlap = gap.frontDepth < gap.backDepth;
            gap.valid = true;
        }
        summary.gaps ~= gap;
    }

    return summary;
}
