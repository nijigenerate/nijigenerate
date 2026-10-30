#pragma once
#include <cstdint>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>

namespace nijigenerate::commands::puppet::view {

enum class ViewCommand : std::int32_t {
    SetDefaultLayout = 0,
    ShowSaveScreenshotDialog = 1,
    SaveScreenshot = 2,
    CaptureLiveScreenshot = 3,
    ShowStatusForNerds = 4,
    ToggleDifferenceAggregation = 5
};

}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<commands::puppet::view::ViewCommand> {
    using Enum = commands::puppet::view::ViewCommand;
    inline static constexpr std::string_view name = "ViewCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::SetDefaultLayout, "SetDefaultLayout"},
        FormatEnumMember<Enum>{Enum::ShowSaveScreenshotDialog, "ShowSaveScreenshotDialog"},
        FormatEnumMember<Enum>{Enum::SaveScreenshot, "SaveScreenshot"},
        FormatEnumMember<Enum>{Enum::CaptureLiveScreenshot, "CaptureLiveScreenshot"},
        FormatEnumMember<Enum>{Enum::ShowStatusForNerds, "ShowStatusForNerds"},
        FormatEnumMember<Enum>{Enum::ToggleDifferenceAggregation, "ToggleDifferenceAggregation"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::puppet::view::ViewCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.puppet.view.ViewCommand)";
    inline static constexpr char unqualifiedArgument[] = "(ViewCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.puppet.view.ViewCommand";
    inline static constexpr char unqualifiedElement[] = "ViewCommand";
};
} // namespace nicxlive::core::d_runtime

// Export the original module's concrete commands, metadata, initializer and
// existing table. Include guards also permit the table provider to import this enum.
#include "view_simple_commands.hpp"
#include <nijigenerate/commands/additional_tables.hpp>
