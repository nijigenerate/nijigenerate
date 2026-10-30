#pragma once
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>
#include "string.hpp"
#include <compat/object_metadata.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace nijigenerate::viewport::depth::draw {

enum class DepthMergePolicy : std::int32_t {
    Replace = 0,
    Frontmost = 1,
    Backmost = 2,
    Add = 3,
    KeepExistingWhereMissing = 4,
};
struct DepthDrawBinding {
    DepthDrawString layerId;
    std::uint64_t targetNodeUuid = 0;
    std::uint64_t targetGridUuid = 0;
    std::int32_t order = 0;
    bool enabled = true;
    bool useNormalLayerAlpha = true;
    float coverageThreshold = 0.01f;
    DepthMergePolicy mergePolicy = DepthMergePolicy::Frontmost;

    bool appliesTo(std::uint64_t gridUuid) const {
        return enabled && targetGridUuid == gridUuid && !layerId.empty();
    }
};

} // namespace nijigenerate::viewport::depth::draw

namespace nijigenerate::viewport::depth::draw::binding {
using ::nijigenerate::viewport::depth::draw::DepthMergePolicy;
using ::nijigenerate::viewport::depth::draw::DepthDrawBinding;
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::viewport::depth::draw::DepthMergePolicy> {
    using Enum = ::nijigenerate::viewport::depth::draw::DepthMergePolicy;
    inline static constexpr std::string_view name = "DepthMergePolicy";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Replace, "Replace"},
        FormatEnumMember<Enum>{Enum::Frontmost, "Frontmost"},
        FormatEnumMember<Enum>{Enum::Backmost, "Backmost"},
        FormatEnumMember<Enum>{Enum::Add, "Add"},
        FormatEnumMember<Enum>{Enum::KeepExistingWhereMissing, "KeepExistingWhereMissing"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {

template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::DepthMergePolicy> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.viewport.depth.draw.binding.DepthMergePolicy)";
    inline static constexpr char unqualifiedArgument[] = "(DepthMergePolicy)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.viewport.depth.draw.binding.DepthMergePolicy";
    inline static constexpr char unqualifiedElement[] = "DepthMergePolicy";
};
template<> struct SourceTypeSpellings<::nijigenerate::viewport::depth::draw::DepthDrawBinding> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.depth.draw.binding.DepthDrawBinding)";
    inline static constexpr char unqualifiedArgument[] = "(DepthDrawBinding)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.depth.draw.binding.DepthDrawBinding";
    inline static constexpr char unqualifiedElement[] = "DepthDrawBinding";
};
}
