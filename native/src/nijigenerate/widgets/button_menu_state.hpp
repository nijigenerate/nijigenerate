#pragma once
#include <imgui.h>
#include <compat/object_metadata.hpp>
#include <limits>
namespace nijigenerate::widgets::detail {
// Typed state skeleton; raw ImGui-storage allocation is a separate contract.
struct DropDownMenuData {
    bool wasOpen = false;
    ImVec2 winSize = ImVec2(std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN());
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::widgets::detail::DropDownMenuData> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.button.DropDownMenuData)";
    inline static constexpr char unqualifiedArgument[] = "(DropDownMenuData)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.button.DropDownMenuData";
    inline static constexpr char unqualifiedElement[] = "DropDownMenuData";
};
}
