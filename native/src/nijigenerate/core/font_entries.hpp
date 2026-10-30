#pragma once
#include "font_state.hpp"
namespace nijigenerate::core::font {
inline compat::Slice<FontEntry> ngFontsGet() {
    return detail::families;
}
}
