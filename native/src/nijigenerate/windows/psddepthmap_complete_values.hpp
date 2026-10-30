#pragma once
#include "psddepthmap_values.hpp"
#include "detail/psddepthmap_value_declarations.hpp"
#include <nijigenerate/io/depthmap_psd_values.hpp>
#include <cmath>

namespace nijigenerate::windows::psddepthmap {
struct PsdDepth3DAdjustGeometryStats {
    std::size_t layerPlanes = 0, depthRangeLines = 0, targetWireLines = 0, sampledPoints = 0, missingPoints = 0;
};
struct PsdDepthDialogLayerState {
    compat::RetainedString layerPath;
    std::uint64_t targetGridUuid = 0;
    bool visible = false, enabled = false, depthEnabled = false, invert = false;
    float depthOffset = NAN, depthScale = NAN;
    bool outlierPruneEnabled = false, puppetFitEnabled = false;
};
struct PsdDepthDialogSettingsState {
    io::depthmap_psd::PsdDepthImportSettings settings;
    compat::Slice<PsdDepthDialogLayerState> layers;
    bool onlyProblemLayers = false;
};
struct PsdDepthDialogPartLayerData {
    compat::RetainedString layerPath, layerName, colorLayerPath, colorLayerName, sourcePath;
    std::int32_t left = 0, top = 0, width = 0, height = 0;
    bool visible = false, enabled = false, depthEnabled = false;
    std::uint64_t targetGridUuid = 0;
    io::depthmap_psd::PsdDepthLayerDepthStats depthStats;
    compat::Slice<std::uint8_t> colorRgba, depthRgba;
};
struct PsdDepthDialogPartData {
    std::uint64_t targetGridUuid = 0;
    compat::RetainedString targetGridName, targetType;
    bool skipped = false;
    std::int32_t documentWidth = 0, documentHeight = 0;
    std::size_t coverageSources = 0, sampledVertices = 0, missingVertices = 0;
    float minDepth = NAN, maxDepth = NAN;
    compat::Slice<float> vertexX, vertexY, depths, baseDepths;
    compat::Slice<compat::RetainedString> winnerLayerPaths;
    nicxlive::core::common::SharedArray<bool> missingVertexMask;
    std::int32_t previewLeft = 0, previewTop = 0, previewWidth = 0, previewHeight = 0;
    compat::Slice<std::uint8_t> previewRgba;
    compat::Slice<PsdDepthDialogPartLayerData> layers;
};
namespace detail {
struct PsdDepthLayerEdit {
    float zOffset = NAN, zScale = 1.0f;
    bool invert = false;
};
struct PsdDepthPendingLayerChange {
    compat::RetainedString layerPath;
    std::uint64_t targetGridUuid = 0;
};
class PsdDepth3DAdjustGpuRenderer;
}
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::windows::psddepthmap::PsdDepth3DAdjustGeometryStats> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.windows.psddepthmap.PsdDepth3DAdjustGeometryStats)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepth3DAdjustGeometryStats)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.windows.psddepthmap.PsdDepth3DAdjustGeometryStats";
    inline static constexpr char unqualifiedElement[] = "PsdDepth3DAdjustGeometryStats";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::psddepthmap::PsdDepthDialogLayerState> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.windows.psddepthmap.PsdDepthDialogLayerState)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepthDialogLayerState)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.windows.psddepthmap.PsdDepthDialogLayerState";
    inline static constexpr char unqualifiedElement[] = "PsdDepthDialogLayerState";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::psddepthmap::PsdDepthDialogSettingsState> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.windows.psddepthmap.PsdDepthDialogSettingsState)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepthDialogSettingsState)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.windows.psddepthmap.PsdDepthDialogSettingsState";
    inline static constexpr char unqualifiedElement[] = "PsdDepthDialogSettingsState";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::psddepthmap::PsdDepthDialogPartLayerData> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.windows.psddepthmap.PsdDepthDialogPartLayerData)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepthDialogPartLayerData)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.windows.psddepthmap.PsdDepthDialogPartLayerData";
    inline static constexpr char unqualifiedElement[] = "PsdDepthDialogPartLayerData";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::psddepthmap::PsdDepthDialogPartData> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.windows.psddepthmap.PsdDepthDialogPartData)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepthDialogPartData)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.windows.psddepthmap.PsdDepthDialogPartData";
    inline static constexpr char unqualifiedElement[] = "PsdDepthDialogPartData";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::psddepthmap::detail::PsdDepthLayerEdit> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.windows.psddepthmap.PsdDepthLayerEdit)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepthLayerEdit)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.windows.psddepthmap.PsdDepthLayerEdit";
    inline static constexpr char unqualifiedElement[] = "PsdDepthLayerEdit";
};
template<> struct SourceTypeSpellings<::nijigenerate::windows::psddepthmap::detail::PsdDepthPendingLayerChange> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.windows.psddepthmap.PsdDepthPendingLayerChange)";
    inline static constexpr char unqualifiedArgument[] = "(PsdDepthPendingLayerChange)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.windows.psddepthmap.PsdDepthPendingLayerChange";
    inline static constexpr char unqualifiedElement[] = "PsdDepthPendingLayerChange";
};
}
