#pragma once
#include "depthmap_psd_values.hpp"
#include <array>
namespace nijigenerate::io::depthmap_psd {
PsdDepthComposedSource ngPsdDepthComposedSourceFromImportResult(PsdDepthImportResult& result);
void ngPsdDepthApplyPreviousComposedLayerState(PsdDepthImportResult& result, PsdDepthImportResult& previous);
void ngPsdDepthApplyPreviousComposedSourceState(PsdDepthImportResult& result, PsdDepthComposedSource previous);
void ngPsdDepthRefreshDerivedState(PsdDepthImportResult& result);
bool ngPsdDepthMappingAccountsForColorLayer(PsdDepthLayerMapping mapping);
bool ngPsdDepthComposedLayerSurfaceCoversDocumentPixel(PsdDepthComposedLayer& layer,
    std::int32_t documentX, std::int32_t documentY, std::size_t& rgbaIndex);
bool ngPsdDepthResolvedPixelAt(PsdDepthImportResult& imported, std::size_t layerIndex,
    std::int32_t documentX, std::int32_t documentY, std::ptrdiff_t& sourceLayerIndex,
    std::array<std::uint8_t, 4>& pixel);
std::ptrdiff_t ngPsdDepthResolvedLayerIndexAt(PsdDepthImportResult& imported, std::size_t layerIndex,
    std::int32_t documentX, std::int32_t documentY, std::size_t& rgbaIndex);
namespace detail {
PsdDepthCompositeSourceLayer cloneCompositeSourceLayer(PsdDepthCompositeSourceLayer layer);
PsdDepthCompositeSource cloneCompositeSource(PsdDepthCompositeSource source);
PsdDepthComposedLayer cloneComposedLayer(PsdDepthComposedLayer layer);
void copyComposedLayerState(PsdDepthComposedLayer& layer, PsdDepthComposedLayer previous);
void applySingleLayerReplacementFallback(PsdDepthImportResult& result, PsdDepthImportResult& previous);
}
}
