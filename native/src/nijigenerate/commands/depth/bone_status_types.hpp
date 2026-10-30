#pragma once

#include <cstdint>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>

namespace nijigenerate::commands::depth::bone_status {
inline constexpr std::uint32_t NgDepthBoneDerivedUpdateProviderId = 0x44424F4E;

enum class DepthBoneUpdateState : std::int32_t {
    Detected,
    Queued,
    Processing,
    Applied,
    Stale,
    Failed,
    Canceled,
};
} // namespace nijigenerate::commands::depth::bone_status

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::commands::depth::bone_status::DepthBoneUpdateState> {
    using Enum = ::nijigenerate::commands::depth::bone_status::DepthBoneUpdateState;
    inline static constexpr std::string_view name = "DepthBoneUpdateState";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Detected, "Detected"},
        FormatEnumMember<Enum>{Enum::Queued, "Queued"},
        FormatEnumMember<Enum>{Enum::Processing, "Processing"},
        FormatEnumMember<Enum>{Enum::Applied, "Applied"},
        FormatEnumMember<Enum>{Enum::Stale, "Stale"},
        FormatEnumMember<Enum>{Enum::Failed, "Failed"},
        FormatEnumMember<Enum>{Enum::Canceled, "Canceled"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::depth::bone_status::DepthBoneUpdateState> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.depth.bone_status.DepthBoneUpdateState)";
    inline static constexpr char unqualifiedArgument[] = "(DepthBoneUpdateState)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.depth.bone_status.DepthBoneUpdateState";
    inline static constexpr char unqualifiedElement[] = "DepthBoneUpdateState";
};
}
