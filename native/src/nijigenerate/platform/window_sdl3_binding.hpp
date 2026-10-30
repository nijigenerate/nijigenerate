#pragma once

#include <string_view>

namespace nijigenerate::platform {

struct SDL3Binding {
    std::string_view targetFile;
    std::string_view targetType;
};

SDL3Binding ngSelectedSDL3Binding() noexcept;
// This validates the linked SDK version, not initialized video/window state.
void ngRequireSDL3Runtime();

} // namespace nijigenerate::platform
