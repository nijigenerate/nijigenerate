#pragma once
#include "path.hpp"
namespace nijigenerate::core::font::detail {
inline void initFontList() {
    [[maybe_unused]] const auto fontsPath = path::ngGetAppFontsPath();
}
}
