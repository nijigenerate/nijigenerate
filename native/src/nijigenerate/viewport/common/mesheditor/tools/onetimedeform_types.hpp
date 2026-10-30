#pragma once
#include <nijigenerate/viewport/common/mesheditor/operations/base.hpp>
#include <nodes/filter.hpp>
#include <compat/object_metadata.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <array>
#include <cstdint>
#include <optional>

namespace nijigenerate::viewport::common::mesheditor::tools::onetimedeform {
enum class SubToolMode : std::int32_t { Select = 0, Vertex = 1, Deform = 2 };
using EditorRef = std::shared_ptr<operations::base::IncMeshEditorOne>;
using NodeRef = std::shared_ptr<nicxlive::core::nodes::Node>;
EditorRef ngGetArmedParameterEditorFor(NodeRef node);
std::shared_ptr<nicxlive::core::nodes::NodeFilter> ngCurrentNodeFilter();
// These functions access the actual OneTimeDeformBase object, not separate mode storage.
std::optional<SubToolMode> ngOneTimeDeformMode(EditorRef editor);
void ngRestoreOneTimeDeformMode(EditorRef editor, SubToolMode mode);
}

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<
    ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::SubToolMode> {
    using Enum = ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::SubToolMode;
    inline static constexpr std::string_view name = "SubToolMode";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::Select, "Select"},
        FormatEnumMember<Enum>{Enum::Vertex, "Vertex"},
        FormatEnumMember<Enum>{Enum::Deform, "Deform"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<
    ::nijigenerate::viewport::common::mesheditor::tools::onetimedeform::SubToolMode> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.viewport.common.mesheditor.tools.onetimedeform.SubToolMode)";
    inline static constexpr char unqualifiedArgument[] = "(SubToolMode)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.viewport.common.mesheditor.tools.onetimedeform.SubToolMode";
    inline static constexpr char unqualifiedElement[] = "SubToolMode";
};
} // namespace nicxlive::core::d_runtime
