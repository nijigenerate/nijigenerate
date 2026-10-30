#pragma once

#include <string_view>
#include <type_traits>
#include <nijigenerate/compat/retained_string.hpp>

namespace nijigenerate::utils {
std::string_view incTypeIdToIcon(std::string_view typeId);
compat::RetainedString ngTypeIdToIconSource(compat::RetainedString typeId);
template<class T> requires std::is_same_v<std::remove_cvref_t<T>, compat::RetainedString>
compat::RetainedString incTypeIdToIcon(T&& typeId) { return ngTypeIdToIconSource(typeId); }
}
