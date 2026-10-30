#pragma once

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <imgui.h>
#include <cstdint>

namespace nijigenerate::core::font::detail {

void _incAddFontData(compat::RetainedString name, compat::Slice<std::uint8_t>& data,
                     float size = 14.0f, const ImWchar* ranges = nullptr,
                     ImVec2 offset = ImVec2(0.0f, 0.0f));

} // namespace nijigenerate::core::font::detail
