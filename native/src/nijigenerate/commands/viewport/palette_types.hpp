#pragma once

#include <cstdint>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>

namespace nijigenerate::commands::viewport::palette {
enum class PaletteCommand : std::int32_t {
    ListCommand = 0,
};
} // namespace nijigenerate::commands::viewport::palette

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<commands::viewport::palette::PaletteCommand> {
    using Enum = commands::viewport::palette::PaletteCommand;
    inline static constexpr std::string_view name = "PaletteCommand";
    inline static constexpr std::array members{FormatEnumMember<Enum>{Enum::ListCommand, "ListCommand"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::viewport::palette::PaletteCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.viewport.palette.PaletteCommand)";
    inline static constexpr char unqualifiedArgument[] = "(PaletteCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.viewport.palette.PaletteCommand";
    inline static constexpr char unqualifiedElement[] = "PaletteCommand";
};
} // namespace nicxlive::core::d_runtime
