#pragma once
#include <cstdint>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>

namespace nijigenerate::commands::viewport::control {
enum class ViewportCommand : std::int32_t {
    ToggleMirrorView = 0,
    ToggleOnionSlice = 1,
    TogglePhysics = 2,
    TogglePostProcess = 3,
    ResetPhysics = 4,
    ResetParameters = 5,
    ListFlipPairs = 6,
    AddFlipPair = 7,
    AutoAddFlipPairs = 8,
    RemoveFlipPair = 9,
    OpenFlipPairWindow = 10,
    OpenAutomeshBatching = 11,
    ResetViewportZoom = 12,
    ResetViewportPosition = 13,
    FitViewportToModel = 14
};
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<commands::viewport::control::ViewportCommand> {
    using Enum = commands::viewport::control::ViewportCommand;
    inline static constexpr std::string_view name = "ViewportCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::ToggleMirrorView, "ToggleMirrorView"},
        FormatEnumMember<Enum>{Enum::ToggleOnionSlice, "ToggleOnionSlice"},
        FormatEnumMember<Enum>{Enum::TogglePhysics, "TogglePhysics"},
        FormatEnumMember<Enum>{Enum::TogglePostProcess, "TogglePostProcess"},
        FormatEnumMember<Enum>{Enum::ResetPhysics, "ResetPhysics"},
        FormatEnumMember<Enum>{Enum::ResetParameters, "ResetParameters"},
        FormatEnumMember<Enum>{Enum::ListFlipPairs, "ListFlipPairs"},
        FormatEnumMember<Enum>{Enum::AddFlipPair, "AddFlipPair"},
        FormatEnumMember<Enum>{Enum::AutoAddFlipPairs, "AutoAddFlipPairs"},
        FormatEnumMember<Enum>{Enum::RemoveFlipPair, "RemoveFlipPair"},
        FormatEnumMember<Enum>{Enum::OpenFlipPairWindow, "OpenFlipPairWindow"},
        FormatEnumMember<Enum>{Enum::OpenAutomeshBatching, "OpenAutomeshBatching"},
        FormatEnumMember<Enum>{Enum::ResetViewportZoom, "ResetViewportZoom"},
        FormatEnumMember<Enum>{Enum::ResetViewportPosition, "ResetViewportPosition"},
        FormatEnumMember<Enum>{Enum::FitViewportToModel, "FitViewportToModel"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::viewport::control::ViewportCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.viewport.control.ViewportCommand)";
    inline static constexpr char unqualifiedArgument[] = "(ViewportCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.viewport.control.ViewportCommand";
    inline static constexpr char unqualifiedElement[] = "ViewportCommand";
};
} // namespace nicxlive::core::d_runtime
