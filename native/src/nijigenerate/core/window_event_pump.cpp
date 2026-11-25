#include "nijigenerate/core/window.hpp"
#include "nijigenerate/core/window_state_internal.hpp"
#include "nijigenerate/core/asyncderivedupdate.hpp"
#include "nijigenerate/core/tasks.hpp"
#include "nijigenerate/core/settings.hpp"
#include "nijigenerate/backend/renderer.hpp"
#include "nijigenerate/io/save.hpp"
#include "nijigenerate/widgets/modal.hpp"

#include <SDL3/SDL_events.h>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <utility>

namespace nijigenerate::core {
namespace {
thread_local compat::RetainedStringSlice pendingProject;
}

void ngRequestProjectOpen(compat::RetainedStringSlice path) {
    pendingProject = std::move(path);
}

void ngBeginLoop() {
    SDL_Event event{};
    static thread_local SDL_Event deferredMouseRelease{};
    static thread_local bool hasDeferredMouseRelease = false;
    static thread_local std::uint32_t heldMouseButtons = 0;

    bool sawHeldMouseMotion = false;
    while (hasDeferredMouseRelease || SDL_PollEvent(&event)) {
        if (hasDeferredMouseRelease) {
            event = deferredMouseRelease;
            hasDeferredMouseRelease = false;
        }
        // Render held motion before delivering its release on the next pass.
        if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && sawHeldMouseMotion) {
            deferredMouseRelease = event;
            hasDeferredMouseRelease = true;
            break;
        }
        bool stepAfterEvent = false;
        switch (event.type) {
        case SDL_EVENT_QUIT:
            io::save::incExitSaveAsk();
            break;
        case SDL_EVENT_DROP_FILE: {
            // SDL3 owns these transient bytes; retain a copy before polling again.
            if (!event.drop.data) break;
            auto path = compat::RetainedStringSlice(compat::RetainedString(event.drop.data));
            // A file-open request has no drop target. Window-targeted drops stay in the viewport.
            const auto name = path.view();
            if (event.drop.windowID == 0 && name.size() >= 4 &&
                std::ranges::equal(name.substr(name.size() - 4), std::string_view(".inx"),
                    [](unsigned char a, char b) { return std::tolower(a) == b; })) {
                ngRequestProjectOpen(std::move(path));
            } else {
                detail::windowState().files.append(std::move(path));
            }
            SDL_RaiseWindow(detail::windowState().window);
            break;
        }
        default:
            backend::ngRendererProcessEvent(&event);
            if (event.type == SDL_EVENT_MOUSE_MOTION &&
                (event.motion.state != 0 || heldMouseButtons != 0)) {
                sawHeldMouseMotion = true;
            }
            if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
                event.button.button > 0 && event.button.button <= 32) {
                const std::uint32_t mask = std::uint32_t{1} << (event.button.button - 1);
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) heldMouseButtons |= mask;
                else heldMouseButtons &= ~mask;
            }
            stepAfterEvent = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
                (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) || event.type == SDL_EVENT_KEY_UP;
            break;
        }
        if (stepAfterEvent) break;
    }
    asyncderivedupdate::incAsyncDerivedUpdateUpdate();
    tasks::ngTaskUpdate();
    ngBeginLoopNoEv();
    if (!pendingProject.empty() && !widgets::modal::ngModalIsOpen() &&
        settings::incSettingsGet<bool>("hasDoneQuickSetup", false) &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
        io::save::incOpenProjectAsk(std::exchange(pendingProject, {}));
    }
}

} // namespace nijigenerate::core
