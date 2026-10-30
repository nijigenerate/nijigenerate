#pragma once

#include "nijigenerate/core/input_shortcut_spec.hpp"

namespace nijigenerate::core::input {

inline constexpr std::string_view ngModifierLabelCtrl() noexcept {
    return detail::ngTokCtrl.view();
}

inline constexpr std::string_view ngModifierLabelSuper() noexcept {
    return detail::ngTokSuper.view();
}

} // namespace nijigenerate::core::input
