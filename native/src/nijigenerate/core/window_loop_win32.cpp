// Portable SDL3 frame bodies from the independent core/window.d source module.
#include "window.hpp"
#include "window_state_internal.hpp"
#include "settings.hpp"
#include "dpi.hpp"
#include <nijigenerate/backend/gl.hpp>
#include <nijigenerate/widgets/modal.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/io/autosave.hpp>
#if defined(__linux__)
#include <nijigenerate/platform/portal_filechooser.hpp>
#endif
#include <type_traits>

namespace nijigenerate::core {
void ngBeginLoopNoEv() {
    backend::gl::ngGLBackendNewFrame();
    backend::gl::ngGLBackendPlatformNewFrame();
    ImGui::NewFrame();
    backend::gl::ngGLBackendBeginRender();
#if defined(__linux__)
    (void)platform::portal::ngUpdatePortal();
#endif
    auto& state = detail::windowState();
    if (!state.files.empty() && !widgets::modal::ngModalIsOpen()) {
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceExtern)) {
            state.fileDropPayloadOwner = state.files;
            const WindowFileDropPayload payload{state.fileDropPayloadOwner.size(), state.fileDropPayloadOwner.data()};
            static_assert(std::is_trivially_copyable_v<WindowFileDropPayload>);
            ImGui::SetDragDropPayload("__PARTS_DROP", &payload, sizeof(payload));
            ImGui::BeginTooltip();
            const auto files = state.files;
            for (const auto file : files) widgets::ngText(file.view());
            ImGui::EndTooltip();
            ImGui::EndDragDropSource();
        }
    } else if (widgets::modal::ngModalIsOpen()) {
        state.files.resize(0);
    }
    ngUpdateWindowTitleTick();
    state.viewportDock = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_NoDockingInCentralNode);
    if (!settings::incSettingsCanGet(compat::RetainedString::fromStaticLiteral("firstrun_complete"))) {
        settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("Armed Parameters.visible"), true);
        ngSetDefaultLayout();
        settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("firstrun_complete"), true);
    }
    if (state.firstFrame) state.firstFrame = false;
    else {
        if (widgets::modal::ngModalIsOpen()) widgets::modal::ngModalRender();
        else widgets::dialog::ngRenderDialogs();
    }
    widgets::dialog::ngHandleDialogHandlers();
}
void ngEndLoop() {
    widgets::dialog::ngCleanupDialogs();
    ImGui::Render();
    // Preserve each source scale query and float product before integer cast.
    const auto displayWidth = compat::ngRequireObject(detail::windowState().io)->DisplaySize.x;
    const auto width = compat::ngFloatToInt32(displayWidth * dpi::incGetUIScale());
    const auto displayHeight = compat::ngRequireObject(detail::windowState().io)->DisplaySize.y;
    const auto height = compat::ngFloatToInt32(displayHeight * dpi::incGetUIScale());
    glViewport(0, 0, width, height);
    glClearColor(.5f, .5f, .5f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    backend::gl::ngGLBackendRenderDrawData(ImGui::GetDrawData());
    if (compat::ngRequireObject(detail::windowState().io)->ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        auto* currentWindow = SDL_GL_GetCurrentWindow();
        const auto currentContext = SDL_GL_GetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        (void)SDL_GL_MakeCurrent(currentWindow, currentContext);
    }
    (void)SDL_GL_SwapWindow(detail::windowState().window);
}
void ngExit() {
    auto& state = detail::windowState();
    state.done = true;
    int width = 0, height = 0;
    const auto flags = SDL_GetWindowFlags(state.window);
    (void)SDL_GetWindowSize(state.window, &width, &height);
    settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("WinW"), width);
    settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("WinH"), height);
    settings::incSettingsSet(compat::RetainedString::fromStaticLiteral("WinMax"),
        (flags & SDL_WINDOW_MAXIMIZED) > 0);
    // This is the declared real autosave provider, never a successful no-op.
    io::autosave::incReleaseLockfile();
}
}
