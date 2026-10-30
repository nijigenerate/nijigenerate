#pragma once
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>

#include <cstdint>

namespace nijigenerate::commands::automesh::config {

enum class AutoMeshConfigCommand : std::int32_t {
    AutoMeshListProcessors,
    AutoMeshGetActive,
    AutoMeshSetActive,
    AutoMeshApplyActive,
    AutoMeshGetSchema,
    AutoMeshGetValues,
    AutoMeshSetPreset,
    AutoMeshSetValues,
};

} // namespace nijigenerate::commands::automesh::config

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::commands::automesh::config::AutoMeshConfigCommand> {
    using Enum = ::nijigenerate::commands::automesh::config::AutoMeshConfigCommand;
    inline static constexpr std::string_view name = "AutoMeshConfigCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::AutoMeshListProcessors, "AutoMeshListProcessors"},
        FormatEnumMember<Enum>{Enum::AutoMeshGetActive, "AutoMeshGetActive"},
        FormatEnumMember<Enum>{Enum::AutoMeshSetActive, "AutoMeshSetActive"},
        FormatEnumMember<Enum>{Enum::AutoMeshApplyActive, "AutoMeshApplyActive"},
        FormatEnumMember<Enum>{Enum::AutoMeshGetSchema, "AutoMeshGetSchema"},
        FormatEnumMember<Enum>{Enum::AutoMeshGetValues, "AutoMeshGetValues"},
        FormatEnumMember<Enum>{Enum::AutoMeshSetPreset, "AutoMeshSetPreset"},
        FormatEnumMember<Enum>{Enum::AutoMeshSetValues, "AutoMeshSetValues"}
    };
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::automesh::config::AutoMeshConfigCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.automesh.config.AutoMeshConfigCommand)";
    inline static constexpr char unqualifiedArgument[] = "(AutoMeshConfigCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.automesh.config.AutoMeshConfigCommand";
    inline static constexpr char unqualifiedElement[] = "AutoMeshConfigCommand";
};
} // namespace nicxlive::core::d_runtime
