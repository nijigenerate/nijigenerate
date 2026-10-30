#pragma once

#include <imgui.h>
#include <limits>

namespace nijigenerate::core::font::detail {

// Private module state: the actual IO atlas is borrowed, never destroyed here.
inline thread_local ImFontAtlas* atlas = nullptr;

#if !defined(NG_NO_UI_SCALING)
// The original font module never reads or assigns this separate variable.
inline thread_local float uiScale = std::numeric_limits<float>::quiet_NaN();
#endif

} // namespace nijigenerate::core::font::detail
