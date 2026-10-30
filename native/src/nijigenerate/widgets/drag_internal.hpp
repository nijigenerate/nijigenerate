#pragma once
#include <compat/object_metadata.hpp>
#include <limits>
namespace nijigenerate::widgets::detail {
struct DragState {
    float initialState=std::numeric_limits<float>::quiet_NaN();
    bool isActive=false;
    bool wasJustCreated=false;
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::widgets::detail::DragState> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.drag.DragState)";
    inline static constexpr char unqualifiedArgument[] = "(DragState)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.drag.DragState";
    inline static constexpr char unqualifiedElement[] = "DragState";
};
}
