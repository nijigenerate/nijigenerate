#pragma once
#include "session.hpp"
#include <compat/object_metadata.hpp>
namespace nijigenerate::viewport::depth::draw::pngexport {
struct DepthDrawPngExportResult {
    bool succeeded = false;
    DepthDrawString manifestPath;
    compat::Slice<DepthDrawString> layerPaths;
    std::size_t exportedLayers = 0, skippedLayers = 0;
};
compat::Slice<std::uint8_t> ngDepthDrawLayerToExportRgba(DepthDrawLayer layer);
void ngSaveDepthDrawLayerPng(DepthDrawLayer layer, DepthDrawString path);
DepthDrawPngExportResult ngExportDepthDrawPngSession(std::shared_ptr<DepthDrawSession> session,
    DepthDrawString outputDir, DepthDrawString manifestPath);
}
namespace nijigenerate::viewport::depth::draw {
using pngexport::DepthDrawPngExportResult;
using pngexport::ngDepthDrawLayerToExportRgba;
using pngexport::ngSaveDepthDrawLayerPng;
using pngexport::ngExportDepthDrawPngSession;
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::depth::draw::pngexport::DepthDrawPngExportResult> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.draw.pngexport.DepthDrawPngExportResult)";
    inline static constexpr char unqualifiedArgument[] = "(DepthDrawPngExportResult)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.draw.pngexport.DepthDrawPngExportResult";
    inline static constexpr char unqualifiedElement[] = "DepthDrawPngExportResult";
};
} // namespace nicxlive::core::d_runtime
