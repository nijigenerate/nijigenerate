#include "font.hpp"
#include "font_add_data.hpp"
#include "font_init_state.hpp"
#include "font_list_init.hpp"
#include "settings.hpp"
#include <nijigenerate/resources/embedded.hpp>
#include <imgui_internal.h>
#include <initializer_list>
#include <vector>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string_view>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>

namespace nijigenerate::core::font {
namespace {
template<class Operation> auto allocateGlyphStorage(Operation operation) {
    try { return operation(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
compat::Slice<std::uint8_t> fontBytes(std::string_view name) {
    const auto bytes = resources::ngEmbeddedResource(name);
    if (!bytes) throw std::runtime_error("Missing embedded font resource");
    return compat::Slice<std::uint8_t>::fromStaticBuffer(
        const_cast<std::uint8_t*>(bytes->data()), bytes->size());
}

struct FontResources {
    compat::Slice<std::uint8_t> dyslexic = fontBytes("OpenDyslexic.otf");
    compat::Slice<std::uint8_t> noto = fontBytes("NotoSans-Regular.ttf");
    compat::Slice<std::uint8_t> cjk = fontBytes("NotoSansCJK-Regular.ttc");
    compat::Slice<std::uint8_t> symbols = fontBytes("MaterialSymbolsOutlined.ttf");
};

// These are the actual mutable buffers passed to the atlas, not copies of an
// atlas buffer. The production context owns its atlas; its shutdown hook runs
// after that atlas is destroyed in the pinned ImGui::Shutdown implementation.
struct GlyphRangeStorage {
    std::vector<compat::Slice<ImWchar>> buffers;
};
void releaseGlyphRanges(ImGuiContext*, ImGuiContextHook* hook) {
    delete static_cast<GlyphRangeStorage*>(hook->UserData);
    hook->UserData = nullptr;
}
GlyphRangeStorage& glyphRangeStorage() {
    auto* context = ImGui::GetCurrentContext();
    for (const auto& hook : context->Hooks)
        if (hook.Type == ImGuiContextHookType_Shutdown && hook.Callback == releaseGlyphRanges)
            return *static_cast<GlyphRangeStorage*>(hook.UserData);
    auto storage = allocateGlyphStorage([] { return std::make_unique<GlyphRangeStorage>(); });
    ImGuiContextHook hook;
    hook.Type = ImGuiContextHookType_Shutdown;
    hook.Callback = releaseGlyphRanges;
    hook.UserData = storage.get();
    ImGui::AddContextHook(context, &hook);
    return *storage.release();
}
const ImWchar* glyphRange(std::initializer_list<ImWchar> values) {
    auto buffer = compat::Slice<ImWchar>(values);
    auto& storage = glyphRangeStorage();
    allocateGlyphStorage([&] { storage.buffers.push_back(buffer); });
    return buffer.data();
}

}

void incInitFonts() {
    // Native TLS initialization is lazy; module-init allocation timing remains
    // a separate D runtime parity requirement.
    thread_local FontResources fonts;
    detail::initFontList();
    detail::atlas = ImGui::GetIO().Fonts;
    const auto appName = compat::RetainedString::fromStaticLiteral("APP\0");
    if (settings::incSettingsGet<bool>(settings::SettingsString::fromStaticLiteral("useOpenDyslexic"))) {
        detail::_incAddFontData(appName, fonts.dyslexic, 24.0f, glyphRange({0x0020, 0x024F, 0}), ImVec2(0, -8));
        detail::_incAddFontData(appName, fonts.noto, 26.0f, glyphRange({0x0250, 0x036F, 0x0370, 0x03FF, 0x0400, 0x052F,
            0x2000, 0x206F, 0xFFFD, 0xFFFD, 0}), ImVec2(0, -6));
    } else {
        detail::_incAddFontData(appName, fonts.noto, 26.0f, glyphRange({0x0020, 0x024F, 0x0250, 0x036F, 0x0370, 0x03FF,
            0x0400, 0x052F, 0x2000, 0x206F, 0xFFFD, 0xFFFD, 0}), ImVec2(0, -6));
    }
    detail::_incAddFontData(appName, fonts.cjk, 26.0f, glyphRange({0x3000, 0x30FF, 0x31F0, 0x31FF, 0x1100, 0x11FF,
        0x3130, 0x318F, 0xA960, 0xA97F, 0xAC00, 0xD7AF, 0xD7B0, 0xD7FF,
        0xFF00, 0xFFEF, 0x4E00, 0x9FAF, 0}), ImVec2(0, -6));
    detail::_incAddFontData(compat::RetainedString::fromStaticLiteral("Material Symbols"),
        fonts.symbols, 32.0f, glyphRange({0xE000, 0xF8FF, 0}), ImVec2(0, 2));
    detail::atlas->Build();
    ImGui::GetIO().FontGlobalScale = 0.5f;
}
} // namespace nijigenerate::core::font
