#pragma once

#include "nijigenerate/core/input_shortcut_spec.hpp"

namespace nijigenerate::core::input {

template<detail::ShortcutLiteral Spec> struct ShortcutSpec {
    inline static constexpr auto value = detail::formatShortcutSpec<Spec>();
};

} // namespace nijigenerate::core::input
