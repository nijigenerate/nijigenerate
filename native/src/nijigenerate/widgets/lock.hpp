#pragma once
// Copyright 2020-2023 Inochi2D Project; 2024 nijigenerate Project.
// Author: Luna Nielsen. BSD-2-Clause; see LICENSE.
#include <string_view>
#include <nijigenerate/compat/retained_string_slice.hpp>
namespace nijigenerate::widgets {
// origin.data() must address readable NUL-terminated storage for ImGui PushID.
// The source descriptor retains that actual backing, including any suffix
// beyond its counted subview. The legacy view's caller supplies the lifetime.
bool ngLockButton(bool* value, compat::RetainedStringSlice origin);
bool ngLockButton(bool* value, std::string_view origin);
namespace lock {
using ::nijigenerate::widgets::ngLockButton;
}
}
