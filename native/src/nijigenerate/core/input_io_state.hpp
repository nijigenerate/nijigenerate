#pragma once

#include <imgui.h>

namespace nijigenerate::core::input::detail {

inline thread_local ImGuiIO* io = nullptr;

} // namespace nijigenerate::core::input::detail
