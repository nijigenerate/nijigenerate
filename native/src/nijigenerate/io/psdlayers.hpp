#pragma once
#include "nijigenerate/compat/retained_string.hpp"
#include "nijigenerate/compat/slice.hpp"
#include "nijigenerate/compat/const_slice.hpp"
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <cstddef>
#include <span>
#include <tuple>

namespace nijigenerate::io::psd { struct Layer; }

namespace nijigenerate::io::psdlayers {
struct PsdLayerGroupState {
    compat::RetainedString path{};
    bool visible = true;
    float opacity = 1.0f;
};

compat::Slice<PsdLayerGroupState> ngPsdLayerGroupStates(compat::Slice<psd::Layer> layers);
compat::Slice<PsdLayerGroupState> ngPsdLayerGroupStates(compat::ConstSlice<psd::Layer> layers);
compat::Slice<PsdLayerGroupState> ngPsdLayerGroupStates(std::span<const psd::Layer> layers);
} // namespace nijigenerate::io::psdlayers

namespace nijigenerate::core::translation {
template<> struct FormatAggregateTraits<::nijigenerate::io::psdlayers::PsdLayerGroupState> {
    using Value = ::nijigenerate::io::psdlayers::PsdLayerGroupState;
    inline static constexpr std::string_view typeName = "PsdLayerGroupState";
    static constexpr auto fields() {
        return std::tuple{
            ngFormatAggregateMember("path", offsetof(Value, path), &Value::path),
            ngFormatAggregateMember("visible", offsetof(Value, visible), &Value::visible),
            ngFormatAggregateMember("opacity", offsetof(Value, opacity), &Value::opacity)};
    }
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::io::psdlayers::PsdLayerGroupState> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.io.psdlayers.PsdLayerGroupState)";
    inline static constexpr char unqualifiedArgument[] = "(PsdLayerGroupState)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.io.psdlayers.PsdLayerGroupState";
    inline static constexpr char unqualifiedElement[] = "PsdLayerGroupState";
};
}
