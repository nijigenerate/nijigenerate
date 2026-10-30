#pragma once
#include <cstdint>
#include <array>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>

namespace nijigenerate::commands::node::node {
enum class NodeCommand : std::int32_t {
    AddNode = 0,
    InsertNode = 1,
    SetNodeName = 2,
    DeleteNode = 3,
    MoveNode = 4,
    ConvertTo = 5,
    CutNode = 6,
    CopyNode = 7,
    PasteNode = 8,
    ReloadNode = 9,
    VertexMode = 10,
    ToggleVisibility = 11,
    CentralizeNode = 12
};
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::commands::node::node::NodeCommand> {
    using Enum = ::nijigenerate::commands::node::node::NodeCommand;
    inline static constexpr std::string_view name = "NodeCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::AddNode, "AddNode"},
        FormatEnumMember<Enum>{Enum::InsertNode, "InsertNode"},
        FormatEnumMember<Enum>{Enum::SetNodeName, "SetNodeName"},
        FormatEnumMember<Enum>{Enum::DeleteNode, "DeleteNode"},
        FormatEnumMember<Enum>{Enum::MoveNode, "MoveNode"},
        FormatEnumMember<Enum>{Enum::ConvertTo, "ConvertTo"},
        FormatEnumMember<Enum>{Enum::CutNode, "CutNode"},
        FormatEnumMember<Enum>{Enum::CopyNode, "CopyNode"},
        FormatEnumMember<Enum>{Enum::PasteNode, "PasteNode"},
        FormatEnumMember<Enum>{Enum::ReloadNode, "ReloadNode"},
        FormatEnumMember<Enum>{Enum::VertexMode, "VertexMode"},
        FormatEnumMember<Enum>{Enum::ToggleVisibility, "ToggleVisibility"},
        FormatEnumMember<Enum>{Enum::CentralizeNode, "CentralizeNode"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::node::node::NodeCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.node.node.NodeCommand)";
    inline static constexpr char unqualifiedArgument[] = "(NodeCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.node.node.NodeCommand";
    inline static constexpr char unqualifiedElement[] = "NodeCommand";
};
}
