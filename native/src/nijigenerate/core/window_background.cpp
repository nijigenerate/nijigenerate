#include "window.hpp"
#include "window_background_state.hpp"
#include <runtime_state.hpp>
#include <bit>
#include <cstdint>

namespace nijigenerate::core {
thread_local bool viewportBackgroundOverrideActive = false;
thread_local ImVec4 viewportBackgroundOverrideColor{
    std::bit_cast<float>(std::uint32_t{0x7fc00000u}), std::bit_cast<float>(std::uint32_t{0x7fc00000u}),
    std::bit_cast<float>(std::uint32_t{0x7fc00000u}), std::bit_cast<float>(std::uint32_t{0x7fc00000u})};
thread_local ImVec4 viewportBackgroundStoredColor{
    std::bit_cast<float>(std::uint32_t{0x7fc00000u}), std::bit_cast<float>(std::uint32_t{0x7fc00000u}),
    std::bit_cast<float>(std::uint32_t{0x7fc00000u}), std::bit_cast<float>(std::uint32_t{0x7fc00000u})};
void ngResetClearColor() {
    const ImVec4 color = ngGetDarkMode() ? ImVec4(0, 0, 0, 1) : ImVec4(1, 1, 1, 1);
    nicxlive::core::inSetClearColor(color.x, color.y, color.z, color.w);
    if (viewportBackgroundOverrideActive) {
        viewportBackgroundStoredColor = color;
        const auto& over = viewportBackgroundOverrideColor;
        nicxlive::core::inSetClearColor(over.x, over.y, over.z, over.w);
    }
}
bool ngViewportHasTemporaryBackgroundColor() { return viewportBackgroundOverrideActive; }
void ngViewportGetBackgroundColor(ImVec4& color) {
    if (viewportBackgroundOverrideActive) color = viewportBackgroundOverrideColor;
    else nicxlive::core::inGetClearColor(color.x, color.y, color.z, color.w);
}
ImVec4 ngViewportGetBackgroundColor() {
    ImVec4 color;
    ngViewportGetBackgroundColor(color);
    return color;
}
void ngViewportSetTemporaryBackgroundColor(float red, float green, float blue, float alpha) {
    if (!viewportBackgroundOverrideActive) {
        auto& color = viewportBackgroundStoredColor;
        nicxlive::core::inGetClearColor(color.x, color.y, color.z, color.w);
        viewportBackgroundOverrideActive = true;
    }
    viewportBackgroundOverrideColor = ImVec4(red, green, blue, alpha);
    nicxlive::core::inSetClearColor(red, green, blue, alpha);
}
void ngViewportSetTemporaryBackgroundColor(ImVec4 color) {
    ngViewportSetTemporaryBackgroundColor(color.x, color.y, color.z, color.w);
}
void ngViewportClearTemporaryBackgroundColor() {
    if (!viewportBackgroundOverrideActive) return;
    const auto& color = viewportBackgroundStoredColor;
    nicxlive::core::inSetClearColor(color.x, color.y, color.z, color.w);
    viewportBackgroundOverrideActive = false;
}
}
