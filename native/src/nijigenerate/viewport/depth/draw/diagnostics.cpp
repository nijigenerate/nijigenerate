#include "diagnostics.hpp"
#include "diagnostics_sort.hpp"
#include "nijigenerate/io/imageexport_case.hpp"
#include "nijigenerate/compat/switch_error.hpp"
#include "nijigenerate/compat/out_of_memory_error.hpp"
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::viewport::depth::draw {
namespace {
template<class Operation> decltype(auto) allocateDiagnosticsMemory(Operation&& operation) {
    try { return std::forward<Operation>(operation)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
std::string_view view(const DepthDrawString& text) { return text.view(); }
DepthDrawString lower(DepthDrawString text) {
    return io::imageexport::detail::lowerExtension(compat::RetainedStringSlice(text));
}
}
DepthDrawSourceDiagnostics ngDepthDrawSourceDiagnostics(std::shared_ptr<DepthDrawSession> session) {
    DepthDrawSourceDiagnostics result;
    if (!session) return result;
    result.totalLayers = session->layers.size(); result.totalBindings = session->bindings.size();
    const auto enabledBindings = session->bindings;
    for (const auto binding : enabledBindings) if (binding.enabled) ++result.enabledBindings;
    const auto layers = session->layers;
    for (const auto layer : layers) {
        if (!layer.enabled) ++result.disabledLayers;
        if (!layer.visible) ++result.invisibleLayers;
        if (!layer.hasDepthPixels()) { ++result.layersMissingDepthPixels; continue; }
        ++result.usableDepthLayers;
        if (!layer.hasNormalCoverage()) ++result.layersMissingCoverage;
        std::size_t enabledBindingCount = 0;
        const auto bindings = session->bindings;
        for (const auto binding : bindings)
            if (binding.enabled && view(binding.layerId) == view(layer.id)) ++enabledBindingCount;
        if (enabledBindingCount == 0) ++result.layersMissingTarget;
        else if (enabledBindingCount > 1) ++result.layersWithAmbiguousBindings;
    }
    return result;
}
compat::Slice<DepthDrawLayerStackRow> ngDepthDrawLayerStackRows(std::shared_ptr<DepthDrawSession> session,
    const DepthDrawComposeResult* composeResult,
    compat::SharedDelegate<DepthDrawString(std::uint64_t)> targetNameResolver) {
    compat::Slice<DepthDrawLayerStackRow> rows;
    if (!session) return rows;
    const auto layers = session->layers;
    for (std::size_t index = 0; index < layers.size(); ++index) {
        const auto layer = layers[index];
        DepthDrawLayerStackRow row;
        row.sourceIndex = index; row.layerId = layer.id;
        row.displayName = view(layer.displayName).empty() ? layer.id : layer.displayName;
        row.layerPath = layer.layerPath; row.enabled = layer.enabled; row.visible = layer.visible;
        row.hasDepthPixels = layer.hasDepthPixels(); row.hasNormalCoverage = layer.hasNormalCoverage();
        row.warningMissingDepthPixels = !row.hasDepthPixels;
        row.warningMissingCoverage = row.hasDepthPixels && !row.hasNormalCoverage;
        std::size_t enabledBindingCount = 0;
        const auto bindings = session->bindings;
        for (const auto binding : bindings) {
            if (!binding.enabled || view(binding.layerId) != view(layer.id)) continue;
            ++enabledBindingCount;
            if (enabledBindingCount == 1) {
                row.targetGridUuid = binding.targetGridUuid;
                if (targetNameResolver) row.targetDisplayName = targetNameResolver(binding.targetGridUuid);
                row.mergePolicy = binding.mergePolicy;
            }
        }
        row.hasBinding = enabledBindingCount > 0; row.hasAmbiguousBindings = enabledBindingCount > 1;
        row.warningMissingTarget = row.hasDepthPixels && !row.hasBinding;
        if (composeResult) {
            const auto layerStats = composeResult->layerStats;
            for (const auto stats : layerStats) {
                if (view(stats.layerId) != view(layer.id)) continue;
                row.sampledVertices += stats.sampledVertices;
                row.missingVertices += stats.missingVertices;
                row.winningVertices += stats.winningVertices;
                if (stats.hasDepthRange) {
                    if (!row.hasDepthRange) {
                        row.hasDepthRange = true; row.minDepth = stats.minDepth; row.maxDepth = stats.maxDepth;
                    } else {
                        if (stats.minDepth < row.minDepth) row.minDepth = stats.minDepth;
                        if (stats.maxDepth > row.maxDepth) row.maxDepth = stats.maxDepth;
                    }
                }
            }
        }
        row.warningMostlyMissingSamples = row.missingVertices > 0 && row.missingVertices > row.sampledVertices;
        rows.append(row);
    }
    return rows;
}
compat::Slice<DepthDrawLayerStackRow> ngDepthDrawFilterAndSortLayerStackRows(
    compat::Slice<DepthDrawLayerStackRow> rows, DepthDrawString filter,
    DepthDrawLayerStackSortMode sortMode, bool descending) {
    compat::Slice<DepthDrawLayerStackRow> result;
    const auto needle = lower(filter);
    for (const auto row : rows) {
        if (!needle.empty()) {
            const auto joined = allocateDiagnosticsMemory([&] {
                return DepthDrawString(std::string(view(row.layerId)) + "\n" + std::string(view(row.displayName)) +
                    "\n" + std::string(view(row.layerPath)) + "\n" + std::string(view(row.targetDisplayName)) + "\n" +
                    (row.hasBinding ? std::to_string(row.targetGridUuid) : ""));
            });
            const auto haystack = lower(joined);
            if (haystack.view().find(needle.view()) == std::string_view::npos) continue;
        }
        result.append(row);
    }
    if (sortMode == DepthDrawLayerStackSortMode::SourceOrder) return result;
    if (sortMode < DepthDrawLayerStackSortMode::Name || sortMode > DepthDrawLayerStackSortMode::Missing)
        throw compat::ngStaticError<compat::SwitchError>(
            compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/diagnostics.d"), 234);
    auto compare = [&](DepthDrawLayerStackRow a, DepthDrawLayerStackRow b) {
        auto values = [&](const auto& av, const auto& bv) {
            if (av == bv) return a.sourceIndex < b.sourceIndex;
            return descending ? av > bv : av < bv;
        };
        switch (sortMode) {
        case DepthDrawLayerStackSortMode::Name: {
            const auto av = lower(a.displayName.empty() ? a.layerId : a.displayName);
            const auto bv = lower(b.displayName.empty() ? b.layerId : b.displayName);
            return values(av.view(), bv.view());
        }
        case DepthDrawLayerStackSortMode::Target: {
            const auto aText = a.targetDisplayName.empty() ? allocateDiagnosticsMemory([&] {
                return DepthDrawString(std::to_string(a.targetGridUuid));
            }) : a.targetDisplayName;
            const auto av = lower(aText);
            const auto bText = b.targetDisplayName.empty() ? allocateDiagnosticsMemory([&] {
                return DepthDrawString(std::to_string(b.targetGridUuid));
            }) : b.targetDisplayName;
            const auto bv = lower(bText);
            return values(av.view(), bv.view());
        }
        case DepthDrawLayerStackSortMode::DepthMin:
            return values(a.hasDepthRange ? a.minDepth : std::numeric_limits<float>::max(),
                b.hasDepthRange ? b.minDepth : std::numeric_limits<float>::max());
        case DepthDrawLayerStackSortMode::Sampled: return values(a.sampledVertices, b.sampledVertices);
        case DepthDrawLayerStackSortMode::Missing: return values(a.missingVertices, b.missingVertices);
        default: throw compat::ngStaticError<compat::SwitchError>(
            compat::RetainedString::fromStaticLiteral("No appropriate switch clause found"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/viewport/depth/draw/diagnostics.d"), 234);
        }
    };
    const bool scalarPredicate = sortMode == DepthDrawLayerStackSortMode::DepthMin ||
        sortMode == DepthDrawLayerStackSortMode::Sampled || sortMode == DepthDrawLayerStackSortMode::Missing;
    diagnostics_sort_detail::Sorter(compare, scalarPredicate).sort({result.data(), result.size()});
    return result;
}
DepthDrawCompositePreviewSummary ngDepthDrawCompositePreviewSummary(const DepthDrawComposeResult result) {
    DepthDrawCompositePreviewSummary summary;
    summary.targetGridUuid = result.targetGridUuid; summary.vertexCount = result.depths.size();
    summary.sampledVertices = result.sampledVertices; summary.missingVertices = result.missingVertices;
    summary.hasDepthRange = result.hasDepthRange; summary.minDepth = result.minDepth; summary.maxDepth = result.maxDepth;
    for (std::size_t i = 0; i < result.depths.size(); ++i) {
        DepthDrawCompositePreviewPoint point;
        point.vertexIndex = i; point.depth = result.depths[i];
        point.winningLayerId = i < result.winningLayerIds.size() ? result.winningLayerIds[i] : nullptr;
        point.sampled = !view(point.winningLayerId).empty(); point.missing = !point.sampled;
        if (point.sampled) point.winningLayerColor = ngDepthDrawWinningLayerColor(point.winningLayerId);
        if (point.sampled) { ++summary.winningVertices; ++summary.finalSampledVertices; }
        else ++summary.finalMissingVertices;
        summary.points.append(point);
    }
    return summary;
}
nicxlive::core::math::Vec4 ngDepthDrawWinningLayerColor(DepthDrawString layerId) {
    std::uint32_t hash = 2166136261U;
    for (unsigned char byte : view(layerId)) { hash ^= byte; hash *= 16777619U; }
    return {0.25f + static_cast<float>(hash & 0xff)/255.0f*0.7f,
        0.25f + static_cast<float>((hash >> 8) & 0xff)/255.0f*0.7f,
        0.25f + static_cast<float>((hash >> 16) & 0xff)/255.0f*0.7f, 1.0f};
}
DepthDrawDepthSpaceSummary ngDepthDrawDepthSpaceSummary(std::shared_ptr<DepthDrawSession> session,
    const DepthDrawComposeResult* composeResult) {
    DepthDrawDepthSpaceSummary summary;
    if (!session) return summary;
    summary.selectedGridUuid = session->selectedGridUuid; summary.selectedLayerId = session->selectedLayerId;
    const auto rows = ngDepthDrawLayerStackRows(session, composeResult);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto row = rows[i];
        const auto layer = session->layerById(row.layerId);
        if (!layer) continue;
        DepthDrawDepthSpaceLayerPlane plane;
        plane.layerId = row.layerId; plane.displayName = row.displayName; plane.bounds = layer->bounds;
        plane.selected = view(row.layerId) == view(session->selectedLayerId);
        plane.visible = row.visible; plane.enabled = row.enabled; plane.hasDepthRange = row.hasDepthRange;
        plane.minDepth = row.minDepth; plane.maxDepth = row.maxDepth;
        plane.representativeDepth = row.hasDepthRange ? (row.minDepth + row.maxDepth)*0.5f : 0.0f;
        summary.layers.append(plane);
        if (i == 0) continue;
        const auto previous = rows[i-1];
        DepthDrawDepthSpaceGapMarker gap;
        gap.backLayerId = previous.layerId; gap.frontLayerId = row.layerId;
        if (previous.hasDepthRange && row.hasDepthRange) {
            gap.backDepth = previous.maxDepth; gap.frontDepth = row.minDepth;
            gap.overlap = gap.frontDepth < gap.backDepth; gap.valid = true;
        }
        summary.gaps.append(gap);
    }
    return summary;
}
}
