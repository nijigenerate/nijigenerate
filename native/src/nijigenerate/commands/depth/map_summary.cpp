#include "map_json.hpp"
#include <bit>
namespace nijigenerate::commands::depth::map {
namespace {
template<std::size_t N> core::settings::SettingsString key(const char (&text)[N]) noexcept {
    return compat::RetainedString::fromStaticLiteral(text);
}
}
core::settings::SettingsJsonValue ngPsdDepthImportSummaryToJson(io::depthmap_psd::PsdDepthImportResult imported, std::size_t changedGrids) {
    using namespace detail;
    Object obj;
    obj[key("changedGrids")] = Json(std::bit_cast<std::int64_t>(changedGrids));
    obj[key("matchedLayers")] = Json(static_cast<std::int64_t>(imported.matchedLayers));
    obj[key("unmatchedLayers")] = Json(static_cast<std::int64_t>(imported.unmatchedLayers));
    obj[key("ambiguousLayers")] = Json(static_cast<std::int64_t>(imported.ambiguousLayers));
    obj[key("skippedGrids")] = Json(static_cast<std::int64_t>(imported.skippedGrids));
    obj[key("compositionMode")] = Json(core::settings::SettingsString(imported.compositionModeName));
    obj[key("compositionWidth")] = Json(static_cast<std::int64_t>(imported.compositionWidth));
    obj[key("compositionHeight")] = Json(static_cast<std::int64_t>(imported.compositionHeight));
    obj[key("colorSourceKind")] = Json(core::settings::SettingsString(imported.colorSource.kindName));
    obj[key("depthSourceKind")] = Json(core::settings::SettingsString(imported.depthSource.kindName));
    obj[key("colorLayerCount")] = Json(static_cast<std::int64_t>(imported.colorLayerCount));
    obj[key("sourceDepthLayerCount")] = Json(static_cast<std::int64_t>(imported.sourceDepthLayerCount));
    obj[key("composedLayerCount")] = Json(static_cast<std::int64_t>(imported.composedLayerCount));
    obj[key("globalDepthScale")] = Json(double(imported.globalDepthScale));
    obj[key("globalDepthCentroid")] = Json(double(imported.globalDepthCentroid));
    obj[key("composedSourceMode")] = Json(core::settings::SettingsString(imported.compositionModeName));
    obj[key("composedSourceWidth")] = Json(std::int64_t(imported.compositionWidth));
    obj[key("composedSourceHeight")] = Json(std::int64_t(imported.compositionHeight));
    obj[key("composedSourceLayerCount")] = Json(std::bit_cast<std::int64_t>(imported.composedLayers.size()));
    Array compositionDiagnostics;
    for (auto diagnostic : imported.compositionDiagnostics) {
        Object entry;
        entry[key("type")] = Json(core::settings::SettingsString(diagnostic.type));
        entry[key("message")] = Json(core::settings::SettingsString(diagnostic.message));
        entry[key("layerPath")] = Json(core::settings::SettingsString(diagnostic.layerPath));
        entry[key("layerName")] = Json(core::settings::SettingsString(diagnostic.layerName));
        compositionDiagnostics.append(Json(entry));
    }
    obj[key("compositionDiagnostics")] = Json(compositionDiagnostics);
    Array composedLayers;
    for (auto layer : imported.composedLayers) {
        Object entry;
        entry[key("id")] = Json(core::settings::SettingsString(layer.id));
        entry[key("layerPath")] = Json(core::settings::SettingsString(layer.layerPath));
        entry[key("layerName")] = Json(core::settings::SettingsString(layer.layerName));
        entry[key("colorLayerPath")] = Json(core::settings::SettingsString(layer.colorLayerPath));
        entry[key("colorLayerName")] = Json(core::settings::SettingsString(layer.colorLayerName));
        entry[key("left")] = Json(static_cast<std::int64_t>(layer.left));
        entry[key("top")] = Json(static_cast<std::int64_t>(layer.top));
        entry[key("width")] = Json(static_cast<std::int64_t>(layer.width));
        entry[key("height")] = Json(static_cast<std::int64_t>(layer.height));
        entry[key("visible")] = Json(layer.visible);
        entry[key("enabled")] = Json(layer.enabled);
        entry[key("depthEnabled")] = Json(layer.depthEnabled);
        entry[key("targetGridName")] = Json(core::settings::SettingsString(layer.targetGridName));
        entry[key("targetGridUuid")] = Json(layer.targetGridUuid);
        entry[key("depthMin01")] = Json(double(layer.depthStats.minDepth01));
        entry[key("depthMax01")] = Json(double(layer.depthStats.maxDepth01));
        entry[key("depthRange01")] = Json(double(layer.depthStats.rangeDepth01));
        entry[key("adjacentDelta01")] = Json(double(layer.depthStats.adjacentDelta01));
        entry[key("maskedPixels")] = Json(static_cast<std::int64_t>(layer.depthStats.maskedPixels));
        entry[key("zeroPixels")] = Json(static_cast<std::int64_t>(layer.depthStats.zeroPixels));
        entry[key("hasDepth")] = Json(layer.depthStats.hasDepth);
        composedLayers.append(Json(entry));
    }
    obj[key("composedLayers")] = Json(composedLayers);
    Array mappings;
    for (auto mapping : imported.mappings) {
        Object entry;
        entry[key("layerPath")] = Json(core::settings::SettingsString(mapping.layerPath));
        entry[key("layerName")] = Json(core::settings::SettingsString(mapping.layerName));
        entry[key("matchedNodeName")] = Json(core::settings::SettingsString(mapping.matchedNodeName));
        entry[key("targetGridName")] = Json(core::settings::SettingsString(mapping.targetGridName));
        entry[key("matched")] = Json(mapping.matched);
        entry[key("ambiguous")] = Json(mapping.ambiguous);
        entry[key("ignored")] = Json(mapping.ignored);
        entry[key("manual")] = Json(mapping.manual);
        entry[key("status")] = Json(core::settings::SettingsString(mapping.status));
        mappings.append(Json(entry));
    }
    obj[key("mappings")] = Json(mappings);
    return Json(obj);
}
}

