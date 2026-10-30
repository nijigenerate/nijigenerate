#pragma once

#include <cstdint>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>

namespace nijigenerate::commands::depth::editor {
enum class DepthEditorOperationCommand : std::int32_t {
    AddEditorDepthOp = 0,
    UpdateEditorDepthOp = 1,
    RemoveEditorDepthOp = 2
};
} // namespace nijigenerate::commands::depth::editor

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::commands::depth::editor::DepthEditorOperationCommand> {
    using Enum = ::nijigenerate::commands::depth::editor::DepthEditorOperationCommand;
    inline static constexpr std::string_view name = "DepthEditorOperationCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::AddEditorDepthOp, "AddEditorDepthOp"},
        FormatEnumMember<Enum>{Enum::UpdateEditorDepthOp, "UpdateEditorDepthOp"},
        FormatEnumMember<Enum>{Enum::RemoveEditorDepthOp, "RemoveEditorDepthOp"}};
};
}
namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::depth::editor::DepthEditorOperationCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.depth.editor.DepthEditorOperationCommand)";
    inline static constexpr char unqualifiedArgument[] = "(DepthEditorOperationCommand)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.depth.editor.DepthEditorOperationCommand";
    inline static constexpr char unqualifiedElement[] = "DepthEditorOperationCommand";
};
}
