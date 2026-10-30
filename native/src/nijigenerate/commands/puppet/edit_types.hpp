#pragma once
#include <cstdint>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>

namespace nijigenerate::commands::puppet::edit {
enum class EditCommand : std::int32_t {
    Undo = 0,
    Redo = 1,
    ShowSettingsWindow = 2,
    ShowCommandBrowserWindow = 3
};
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<commands::puppet::edit::EditCommand> {
    using Enum = commands::puppet::edit::EditCommand;
    inline static constexpr std::string_view name = "EditCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Undo, "Undo"},
        FormatEnumMember<Enum>{Enum::Redo, "Redo"},
        FormatEnumMember<Enum>{Enum::ShowSettingsWindow, "ShowSettingsWindow"},
        FormatEnumMember<Enum>{Enum::ShowCommandBrowserWindow, "ShowCommandBrowserWindow"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::puppet::edit::EditCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.puppet.edit.EditCommand)";
    inline static constexpr char unqualifiedArgument[] = "(EditCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.puppet.edit.EditCommand";
    inline static constexpr char unqualifiedElement[] = "EditCommand";
};
} // namespace nicxlive::core::d_runtime
