#pragma once

#include <cstdint>
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::depth::bone_status::detail {

// The actual private D value has exactly these two unsigned fields.
struct DepthBoneUpdateWorkKey {
    std::uint32_t batchId{};
    std::uint32_t targetUuid{};
};

} // namespace nijigenerate::commands::depth::bone_status::detail

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
        ::nijigenerate::commands::depth::bone_status::detail::DepthBoneUpdateWorkKey> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.depth.bone_status.DepthBoneUpdateWorkKey)";
    inline static constexpr char unqualifiedArgument[] = "(DepthBoneUpdateWorkKey)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.depth.bone_status.DepthBoneUpdateWorkKey";
    inline static constexpr char unqualifiedElement[] = "DepthBoneUpdateWorkKey";
};
}
