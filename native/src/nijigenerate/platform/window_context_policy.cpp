#include "nijigenerate/platform/window_context_policy.hpp"
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <imgui.h>

namespace nijigenerate::platform::detail {
bool windowConfigFileExists(std::string_view path) {
    return compat::phobos::ngFileExists(path);
}

void configureWindowContextHost(ImGuiIO& io) {
#if defined(NG_OSX)
    io.ConfigMacOSXBehaviors = true;
#else
    (void)io;
#endif
}
}
