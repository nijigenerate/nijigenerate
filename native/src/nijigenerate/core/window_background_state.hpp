#pragma once

#include <imgui.h>

// Implementation/package-facing state from core/window.d's package block.
// Do not include this header in the public window facade. C++ has no D package
// protection; consumers must retain the original core-package access boundary.
namespace nijigenerate::core {
extern thread_local bool viewportBackgroundOverrideActive;
extern thread_local ImVec4 viewportBackgroundOverrideColor;
extern thread_local ImVec4 viewportBackgroundStoredColor;
}
