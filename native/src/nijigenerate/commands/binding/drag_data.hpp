#pragma once

#include <param/parameter.hpp>
#include <memory>
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::binding {
struct ParamDragDropData {
    std::shared_ptr<::nicxlive::core::param::Parameter> param;
};
} // namespace nijigenerate::commands::binding

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::binding::ParamDragDropData> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.binding.base.ParamDragDropData)";
    inline static constexpr char unqualifiedArgument[] = "(ParamDragDropData)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.binding.base.ParamDragDropData";
    inline static constexpr char unqualifiedElement[] = "ParamDragDropData";
};
}
