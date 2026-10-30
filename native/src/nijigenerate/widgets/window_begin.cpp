#include "nijigenerate/widgets/window.hpp"

#if defined(NG_USE_UI_SCALING) && defined(NG_NO_UI_SCALING)
#error "Conflicting UI scaling modes are not supported by the native DPI contract"
#elif !defined(NG_USE_UI_SCALING) && !defined(NG_NO_UI_SCALING)
#error "The Begin wrapper requires an explicit UI scaling mode"
#endif

#if defined(NG_NO_UI_SCALING)
#include "nijigenerate/core/window_platform.hpp"
#endif

namespace nijigenerate::widgets {
bool ngBegin(const char* name, bool* open, ImGuiWindowFlags flags) {
#if defined(NG_NO_UI_SCALING)
    return ImGui::Begin(name, open, core::ngIsWayland() ? flags : flags | ImGuiWindowFlags_NoDecoration);
#else
    return ImGui::Begin(name, open, flags);
#endif
}
}
