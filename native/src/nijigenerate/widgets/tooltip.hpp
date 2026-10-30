#pragma once
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <string_view>
#include <type_traits>
namespace nicxlive::core::common { class NullableString; }
namespace nijigenerate::widgets {
void ngTooltip(compat::RetainedStringSlice tip);
// Native borrowed inputs are copied before any UI callback can replace them.
void ngTooltip(std::string_view tip);
template<class T>
    requires std::is_same_v<std::remove_cvref_t<T>, nicxlive::core::common::NullableString>
void ngTooltip(T&& tip) {
    ngTooltip(compat::RetainedStringSlice(compat::RetainedString(tip)));
}
namespace tooltip {
using ::nijigenerate::widgets::ngTooltip;
}
}
