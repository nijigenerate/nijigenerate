#include "nijigenerate/core/window.hpp"
#include "nijigenerate/core/window_state_internal.hpp"
#include <stdexcept>

namespace nijigenerate::core {

bool ngIsTilingWM() {
    return detail::windowState().isTilingWM;
}

SDL_Window* ngGetWindowPtr() {
    return detail::windowState().window;
}

ImGuiID ngGetViewportDockSpace() {
    return detail::windowState().viewportDock;
}

bool ngShouldProcess() {
    return (SDL_GetWindowFlags(detail::windowState().window) & SDL_WINDOW_MINIMIZED) == 0;
}

void ngFinishFileDrag() {
    // Shrink this descriptor only, retaining backing storage and other slices.
    detail::windowState().files.resize(0);
}

compat::Slice<compat::RetainedStringSlice> ngRetainFileDropPayload(const WindowFileDropPayload& payload) {
    const auto owner = detail::windowState().fileDropPayloadOwner;
    // Only this actual producer publishes __PARTS_DROP. A foreign native
    // payload cannot be made owning by inventing an unrelated current array.
    if (payload.length != owner.size() || payload.data != owner.data())
        throw std::logic_error("File-drop payload does not name the retained window producer allocation");
    return owner;
}

bool ngIsCloseRequested() {
    return detail::windowState().done;
}

} // namespace nijigenerate::core
