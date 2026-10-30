#pragma once
#include "depthmap_psd_import.hpp"
#include "depthmap_psd_targets.hpp"
#include "depthmap_psd_sampling.hpp"
#include "depthmap_psd_helpers.hpp"
#include "depthmap_psd_enum_functions.hpp"
#include "detail/unique_psd_path.hpp"
#include "detail/psd_depth_allocation.hpp"
#include <nijigenerate/compat/exception_message.hpp>
#include <nijigenerate/io/psd/package.hpp>
#include <string>
#include <utility>
namespace nijigenerate::io::depthmap_psd {
namespace detail {
void enforce(bool condition, std::string_view message, std::size_t sourceLine = 0);
compat::RetainedString sourceBaseName(compat::RetainedString path);
compat::RetainedString sourceStem(compat::RetainedString path);
compat::RetainedString sourceExtensionLower(compat::RetainedString path);
void setCompositionMode(PsdDepthImportResult& result, PsdDepthCompositionMode mode);
void addCompositionDiagnostic(PsdDepthImportResult& result, compat::RetainedString type,
    compat::RetainedString message, compat::RetainedString layerPath = {},
    compat::RetainedString layerName = {}, std::size_t count = 0);
void validateCompositionDimensions(PsdDepthImportResult& result, compat::RetainedString context);
bool psdLayerVisible(psd::Layer& layer);
PsdDepthCompositeSourceLayer makeCompositeSourceLayer(compat::RetainedString id, compat::RetainedString name,
    int left, int top, int width, int height, bool visible, bool hasPixels, std::size_t maskedPixels = 0,
    DeformableRef target = {});
void setCompositeSourceLayerPixels(PsdDepthCompositeSourceLayer& layer, compat::Slice<std::uint8_t> rgba,
    compat::Slice<std::uint8_t> maskRgba = {});
void loadPsdCompositeSourceLayers(compat::RetainedString sourcePath, PsdDepthCompositeSource& source,
    std::size_t& layerCount, std::uint64_t& retainedBytes);
void addComposedLayer(PsdDepthImportResult& result, compat::RetainedString sourcePath,
    DepthLayerImage& image, PsdDepthImportSettings& settings, bool visible, bool enabled, DeformableRef target = {});
void addMissingComposedLayer(PsdDepthImportResult& result, compat::RetainedString path, compat::RetainedString name,
    int left, int top, int width, int height, DeformableRef target);
void addMissingComposedLayer(PsdDepthImportResult& result, DeformableRef target);
}
}
