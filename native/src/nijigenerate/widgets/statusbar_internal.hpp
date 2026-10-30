#pragma once
#include "nijigenerate/widgets/statusbar.hpp"
#include "nijigenerate/compat/slice.hpp"
#include <cstddef>

namespace nijigenerate::widgets::statusbar::detail {
struct ToolTipItem {
    compat::RetainedString action;
    compat::RetainedString key;
};
std::size_t& itemIndex();
compat::Slice<ToolTipItem>& items();
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::widgets::statusbar::detail::ToolTipItem> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.widgets.statusbar.ToolTipItem)";
    inline static constexpr char unqualifiedArgument[] = "(ToolTipItem)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.widgets.statusbar.ToolTipItem";
    inline static constexpr char unqualifiedElement[] = "ToolTipItem";
};
}
