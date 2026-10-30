#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nodes/common.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/typed_format_blend_mode.hpp>
#include <nijigenerate/core/typed_format_texture.hpp>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <tuple>

namespace nicxlive::core {
class Puppet;
class Texture;
namespace nodes { class Part; }
}
namespace nijigenerate::atlas::atlas { class Atlas; }

namespace nijigenerate::io::inpexport {

struct IncINPPreviewInfo {
    float outputScale = std::numeric_limits<float>::quiet_NaN();
    std::shared_ptr<nicxlive::core::Texture> preview;
};

struct IncINPExportSettings {
    std::size_t atlasResolution = 2048;
    bool nonLinearScaling = false;
    float scale = 1.0f;
    std::int32_t padding = 16;
    bool optimizePruneUnused = true;
    std::shared_ptr<nicxlive::core::Texture> decorateWatermark;
    float decorateWatermarkOpacity = 1.0f;
    nicxlive::core::nodes::BlendMode decorateWatermarkBlendMode =
        nicxlive::core::nodes::BlendMode::ClipToLower;
    std::uint32_t decorateWatermarkLoops = 10;
};

using ExportPuppet = std::shared_ptr<nicxlive::core::Puppet>;
using ExportPart = std::shared_ptr<nicxlive::core::nodes::Part>;
using ExportAtlas = std::shared_ptr<nijigenerate::atlas::atlas::Atlas>;
using ExportParts = compat::Slice<ExportPart>;
using ExportAtlases = compat::Slice<ExportAtlas>;

void ngINPExportPruneEditorOnlyNodes(ExportPuppet puppet);
ExportParts incINPExportGetBestSort(ExportPuppet puppet);
ExportPuppet incINPExportGenPuppet(ExportPuppet puppet, IncINPExportSettings settings, bool optimize = false);
void incINPExportOptimizePrune(ExportPuppet puppet, IncINPExportSettings settings);
void incINPExportDecorateWatermark(ExportAtlas& atlas, IncINPExportSettings& settings);
IncINPPreviewInfo incINPExportGenPreview(ExportPuppet puppet, IncINPExportSettings settings);
ExportAtlases incINPExportGenAtlasses(ExportPuppet puppet, IncINPExportSettings settings);
void incINPExportFlatten(ExportPuppet& target);
void incINPExportFinalizePacking(ExportPuppet& source, ExportAtlases atlasses);
void incINPExport(ExportPuppet puppet, IncINPExportSettings settings, compat::RetainedStringSlice file);

} // namespace nijigenerate::io::inpexport

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::io::inpexport::IncINPPreviewInfo> {
    using Value = ::nijigenerate::io::inpexport::IncINPPreviewInfo;
    inline static constexpr std::string_view typeName = "IncINPPreviewInfo";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("outputScale", offsetof(Value, outputScale), &Value::outputScale),
            ngFormatAggregateMember("preview", offsetof(Value, preview), &Value::preview)};
    }
};
template<> struct FormatAggregateTraits<::nijigenerate::io::inpexport::IncINPExportSettings> {
    using Value = ::nijigenerate::io::inpexport::IncINPExportSettings;
    inline static constexpr std::string_view typeName = "IncINPExportSettings";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("atlasResolution", offsetof(Value, atlasResolution), &Value::atlasResolution),
            ngFormatAggregateMember("nonLinearScaling", offsetof(Value, nonLinearScaling), &Value::nonLinearScaling),
            ngFormatAggregateMember("scale", offsetof(Value, scale), &Value::scale),
            ngFormatAggregateMember("padding", offsetof(Value, padding), &Value::padding),
            ngFormatAggregateMember("optimizePruneUnused", offsetof(Value, optimizePruneUnused),
                &Value::optimizePruneUnused),
            ngFormatAggregateMember("decorateWatermark", offsetof(Value, decorateWatermark), &Value::decorateWatermark),
            ngFormatAggregateMember("decorateWatermarkOpacity", offsetof(Value, decorateWatermarkOpacity),
                &Value::decorateWatermarkOpacity),
            ngFormatAggregateMember("decorateWatermarkBlendMode", offsetof(Value, decorateWatermarkBlendMode),
                &Value::decorateWatermarkBlendMode),
            ngFormatAggregateMember("decorateWatermarkLoops", offsetof(Value, decorateWatermarkLoops),
                &Value::decorateWatermarkLoops)};
    }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::io::inpexport::IncINPPreviewInfo> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.io.inpexport.IncINPPreviewInfo)";
    inline static constexpr char unqualifiedArgument[] = "(IncINPPreviewInfo)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.io.inpexport.IncINPPreviewInfo";
    inline static constexpr char unqualifiedElement[] = "IncINPPreviewInfo";
};
template<> struct SourceTypeSpellings<::nijigenerate::io::inpexport::IncINPExportSettings> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.io.inpexport.IncINPExportSettings)";
    inline static constexpr char unqualifiedArgument[] = "(IncINPExportSettings)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.io.inpexport.IncINPExportSettings";
    inline static constexpr char unqualifiedElement[] = "IncINPExportSettings";
};
}
