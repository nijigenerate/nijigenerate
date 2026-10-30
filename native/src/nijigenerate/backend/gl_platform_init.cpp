#include "nijigenerate/backend/gl.hpp"
#include "nijigenerate/platform/gl_backend_procedure.hpp"

namespace nijigenerate::backend::gl {

#if defined(NG_NO_UI_SCALING)
extern "C" void ngGLBackendPlatformRenderWindow(ImGuiViewport* viewport, void*) {
    if (!(viewport->Flags & ImGuiViewportFlags_NoRendererClear)) {
        using ClearColor = void (APIENTRY*)(GLfloat, GLfloat, GLfloat, GLfloat);
        using Clear = void (APIENTRY*)(GLbitfield);
        const auto clearColor = platform::ngGLBackendProcedure<ClearColor>("glClearColor");
        clearColor(0.f, 0.f, 0.f, 1.f);
        const auto clear = platform::ngGLBackendProcedure<Clear>("glClear");
        clear(GL_COLOR_BUFFER_BIT);
    }
    ngGLBackendRenderDrawData(viewport->DrawData);
}
#endif

void ngGLBackendPlatformInterfaceInit() {
#if defined(NG_NO_UI_SCALING)
    ImGui::GetPlatformIO().Renderer_RenderWindow = &ngGLBackendPlatformRenderWindow;
#endif
}

} // namespace nijigenerate::backend::gl
