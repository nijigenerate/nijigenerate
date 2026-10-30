#pragma once

#include "font_entries.hpp"

namespace nijigenerate::core::font {
void incInitFonts();
inline compat::Slice<FontEntry> incFontsGet() { return ngFontsGet(); }
} // namespace nijigenerate::core::font
