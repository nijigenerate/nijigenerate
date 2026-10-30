#include "depthmap_psd_loading.hpp"
namespace nijigenerate::io::depthmap_psd {
using namespace detail;
PsdDepthImportResult ngBuildPsdDepthsFromSource(PuppetRef puppet, compat::RetainedString path,
    PsdDepthImportSettings settings) {
    if (sourceExtensionLower(path).view() == ".png") return ngBuildPsdDepthsFromImage(puppet, path, settings);
    return ngBuildPsdDepthsFromPSD(puppet, path, settings);
}
PsdDepthImportResult ngPsdDepthReplaceDepthSource(PuppetRef puppet, PsdDepthImportResult& previous,
    compat::RetainedString path, PsdDepthImportSettings settings) {
    enforce(!path.empty(), "Replacement depth source path is required", 4105);
    const auto previousColor = previous.colorSource.sourcePath;
    if (settings.colorSourcePath.empty() && !previousColor.empty()) settings.colorSourcePath = previousColor;
    if (previous.composedLayers.size() == 1 && previous.composedLayers[0].targetGridUuid != 0) {
        const auto stem = sourceStem(path);
        const auto replacement = allocateImportBytes([&] {
            return compat::RetainedString("/" + std::string(stem.view()));
        });
        if (!settings.layerTargetGridUuidOverrides.contains(replacement.view()))
            settings.layerTargetGridUuidOverrides.setRetained(replacement,
                retainImportUnsignedDecimal(previous.composedLayers[0].targetGridUuid));
    }
    auto result = ngBuildPsdDepthsFromSource(puppet, path, settings);
    ngPsdDepthApplyPreviousComposedLayerState(result, previous);
    applySingleLayerReplacementFallback(result, previous);
    return result;
}
PsdDepthImportResult ngPsdDepthReplaceColorSource(PuppetRef puppet, PsdDepthImportResult& previous,
    compat::RetainedString path, PsdDepthImportSettings settings) {
    enforce(!path.empty(), "Replacement color source path is required", 4129);
    const auto previousDepth = previous.depthSource.sourcePath;
    enforce(!previousDepth.empty(), "Previous depth source path is required", 4131);
    settings.colorSourcePath = path;
    auto result = ngBuildPsdDepthsFromSource(puppet, previousDepth, settings);
    ngPsdDepthApplyPreviousComposedLayerState(result, previous);
    applySingleLayerReplacementFallback(result, previous);
    return result;
}
}
