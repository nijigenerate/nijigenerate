#include "nijigenerate/backend/gl.hpp"
#include "nijigenerate/backend/gl_state_internal.hpp"
#include "nijigenerate/platform/gl_font_texture.hpp"

namespace nijigenerate::backend::gl {

bool ngGLBackendCreateFontsTexture() {
    auto& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, nullptr);
    auto& state = detail::glBackendState();
    const auto binding = platform::ngGLUploadFontTexture(state.fontTexture, pixels, width, height);
    // Only the font object owned by this backend has this GL-name convention.
    io.Fonts->SetTexID(static_cast<ImTextureID>(state.fontTexture));
    platform::ngGLRestoreFontTextureBinding(binding);
    return true;
}

void ngGLBackendDestroyFontsTexture() {
    auto& state = detail::glBackendState();
    if (state.fontTexture) {
        auto& io = ImGui::GetIO();
        platform::ngGLDeleteFontTexture(state.fontTexture);
        io.Fonts->SetTexID(static_cast<ImTextureID>(0));
        state.fontTexture = 0;
    }
}

} // namespace nijigenerate::backend::gl
