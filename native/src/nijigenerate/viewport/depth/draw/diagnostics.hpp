#pragma once
#include "session.hpp"
#include "composer.hpp"
#include <compat/object_metadata.hpp>
#include <cstdint>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <limits>

namespace nijigenerate::viewport::depth::draw {
enum class DepthDrawLayerStackSortMode : std::int32_t { SourceOrder, Name, Target, DepthMin, Sampled, Missing };
struct DepthDrawLayerStackRow {
    std::size_t sourceIndex = 0;
    DepthDrawString layerId, displayName, layerPath;
    bool enabled = false, visible = false, hasDepthPixels = false, hasNormalCoverage = false;
    bool hasBinding = false, hasAmbiguousBindings = false;
    std::uint64_t targetGridUuid = 0;
    DepthDrawString targetDisplayName;
    DepthMergePolicy mergePolicy = DepthMergePolicy::Replace;
    std::size_t sampledVertices = 0, missingVertices = 0, winningVertices = 0;
    bool hasDepthRange = false;
    float minDepth = std::numeric_limits<float>::quiet_NaN(), maxDepth = std::numeric_limits<float>::quiet_NaN();
    bool warningMissingDepthPixels = false, warningMissingTarget = false;
    bool warningMissingCoverage = false, warningMostlyMissingSamples = false;
};
struct DepthDrawCompositePreviewPoint {
    std::size_t vertexIndex = 0;
    float depth = std::numeric_limits<float>::quiet_NaN();
    DepthDrawString winningLayerId;
    nicxlive::core::math::Vec4 winningLayerColor{
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    bool sampled = false, missing = false;
};
struct DepthDrawCompositePreviewSummary {
    std::uint64_t targetGridUuid = 0;
    std::size_t vertexCount = 0, sampledVertices = 0, missingVertices = 0, winningVertices = 0;
    std::size_t finalSampledVertices = 0, finalMissingVertices = 0;
    bool hasDepthRange = false;
    float minDepth = std::numeric_limits<float>::quiet_NaN(), maxDepth = std::numeric_limits<float>::quiet_NaN();
    compat::Slice<DepthDrawCompositePreviewPoint> points;
};
struct DepthDrawDepthSpaceLayerPlane {
    DepthDrawString layerId, displayName;
    DepthDrawRect bounds;
    bool selected = false, visible = false, enabled = false, hasDepthRange = false;
    float minDepth = std::numeric_limits<float>::quiet_NaN(), maxDepth = std::numeric_limits<float>::quiet_NaN();
    float representativeDepth = std::numeric_limits<float>::quiet_NaN();
};
struct DepthDrawDepthSpaceGapMarker {
    DepthDrawString backLayerId, frontLayerId;
    bool valid = false, overlap = false;
    float backDepth = std::numeric_limits<float>::quiet_NaN(), frontDepth = std::numeric_limits<float>::quiet_NaN();
};
struct DepthDrawDepthSpaceSummary {
    std::uint64_t selectedGridUuid = 0;
    DepthDrawString selectedLayerId;
    compat::Slice<DepthDrawDepthSpaceLayerPlane> layers;
    compat::Slice<DepthDrawDepthSpaceGapMarker> gaps;
};
struct DepthDrawSourceDiagnostics {
    std::size_t totalLayers = 0, usableDepthLayers = 0, disabledLayers = 0, invisibleLayers = 0;
    std::size_t layersMissingDepthPixels = 0, layersMissingTarget = 0, layersMissingCoverage = 0;
    std::size_t layersWithAmbiguousBindings = 0, totalBindings = 0, enabledBindings = 0;
};
DepthDrawSourceDiagnostics ngDepthDrawSourceDiagnostics(std::shared_ptr<DepthDrawSession> session);
compat::Slice<DepthDrawLayerStackRow> ngDepthDrawLayerStackRows(std::shared_ptr<DepthDrawSession> session,
    const DepthDrawComposeResult* composeResult = nullptr,
    compat::SharedDelegate<DepthDrawString(std::uint64_t)> targetNameResolver = {});
compat::Slice<DepthDrawLayerStackRow> ngDepthDrawFilterAndSortLayerStackRows(
    compat::Slice<DepthDrawLayerStackRow> rows, DepthDrawString filter,
    DepthDrawLayerStackSortMode sortMode = DepthDrawLayerStackSortMode::SourceOrder, bool descending = false);
DepthDrawCompositePreviewSummary ngDepthDrawCompositePreviewSummary(const DepthDrawComposeResult result);
nicxlive::core::math::Vec4 ngDepthDrawWinningLayerColor(DepthDrawString layerId);
DepthDrawDepthSpaceSummary ngDepthDrawDepthSpaceSummary(std::shared_ptr<DepthDrawSession> session,
    const DepthDrawComposeResult* composeResult = nullptr);
}

namespace nijigenerate::viewport::depth::draw::diagnostics {
using ::nijigenerate::viewport::depth::draw::DepthDrawLayerStackSortMode;
using ::nijigenerate::viewport::depth::draw::DepthDrawLayerStackRow;
using ::nijigenerate::viewport::depth::draw::DepthDrawCompositePreviewPoint;
using ::nijigenerate::viewport::depth::draw::DepthDrawCompositePreviewSummary;
using ::nijigenerate::viewport::depth::draw::DepthDrawDepthSpaceLayerPlane;
using ::nijigenerate::viewport::depth::draw::DepthDrawDepthSpaceGapMarker;
using ::nijigenerate::viewport::depth::draw::DepthDrawDepthSpaceSummary;
using ::nijigenerate::viewport::depth::draw::DepthDrawSourceDiagnostics;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawSourceDiagnostics;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawLayerStackRows;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawFilterAndSortLayerStackRows;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawCompositePreviewSummary;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawWinningLayerColor;
using ::nijigenerate::viewport::depth::draw::ngDepthDrawDepthSpaceSummary;
} // namespace nijigenerate::viewport::depth::draw::diagnostics

namespace nicxlive::core::d_runtime {
#define NG_DEPTH_DIAGNOSTICS_VALUE_SOURCE_ORIGIN(Type) \
    template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::Type> { \
        inline static constexpr bool isNativeObject = false; \
        inline static constexpr char qualifiedArgument[] = \
            "(nijigenerate.viewport.depth.draw.diagnostics." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = \
            "nijigenerate.viewport.depth.draw.diagnostics." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }
NG_DEPTH_DIAGNOSTICS_VALUE_SOURCE_ORIGIN(DepthDrawLayerStackSortMode);
NG_DEPTH_DIAGNOSTICS_VALUE_SOURCE_ORIGIN(DepthDrawLayerStackRow);
NG_DEPTH_DIAGNOSTICS_VALUE_SOURCE_ORIGIN(DepthDrawCompositePreviewPoint);
NG_DEPTH_DIAGNOSTICS_VALUE_SOURCE_ORIGIN(DepthDrawCompositePreviewSummary);
NG_DEPTH_DIAGNOSTICS_VALUE_SOURCE_ORIGIN(DepthDrawDepthSpaceLayerPlane);
NG_DEPTH_DIAGNOSTICS_VALUE_SOURCE_ORIGIN(DepthDrawDepthSpaceGapMarker);
NG_DEPTH_DIAGNOSTICS_VALUE_SOURCE_ORIGIN(DepthDrawDepthSpaceSummary);
NG_DEPTH_DIAGNOSTICS_VALUE_SOURCE_ORIGIN(DepthDrawSourceDiagnostics);
#undef NG_DEPTH_DIAGNOSTICS_VALUE_SOURCE_ORIGIN
}
