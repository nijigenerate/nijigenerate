// Copyright 2020-2024 Inochi2D/nijigenerate contributors. BSD-2-Clause; see LICENSE.
#include "inputtext.hpp"
#include "inputtext_buffers.hpp"
#include "inputtext_internal.hpp"
#include "dummy.hpp"
#include <nijigenerate/compat/numeric.hpp>
#include <nijigenerate/platform/imgui_sdl3_window.hpp>
#include <imgui_internal.h>
#include <bit>
#include <exception>
#include <type_traits>

namespace nijigenerate::widgets {
namespace {
using namespace inputtext::detail;
struct IdScope {
    explicit IdScope(std::string_view id) {
        const char* first = id.data();
        const char* last = first ? first + id.size() : first;
        ImGui::PushID(std::bit_cast<int>(ImGui::GetID(first, last)));
    }
    ~IdScope() { ImGui::PopID(); }
};
struct WidthScope {
    explicit WidthScope(float width) { ImGui::PushItemWidth(width); }
    ~WidthScope() { ImGui::PopItemWidth(); }
};
template<class Label> struct SuccessLabel {
    Label label;
    int exceptions = std::uncaught_exceptions();
    ~SuccessLabel() noexcept(false) {
        if (std::uncaught_exceptions() == exceptions) {
            ImGui::SameLine(0, ImGui::GetStyle().ItemSpacing.x);
            const char* first = label.data();
            const char* last = first ? first + label.size() : first;
            ImGui::TextEx(first, last);
        }
    }
};
void setSourceTextInputRectangle() {
    const auto minimum = ImGui::GetItemRectMin();
    const auto maximum = ImGui::GetItemRectMax();
    // Preserve the source's numeric rectangle, including maximum.x as width.
    // The platform adapter transforms UI units into the owning viewport's
    // logical window units, including the source's unusual pre-transform extents.
    const auto x = static_cast<std::uint32_t>(compat::ngFloatToInt32(minimum.x)) + 32u;
    const SDL_Rect area{std::bit_cast<std::int32_t>(x), compat::ngFloatToInt32(minimum.y),
        compat::ngFloatToInt32(maximum.x), 32};
    platform::ngSetImGuiSDL3UITextInputArea(ImGui::GetCurrentContext(),
        ImGui::GetWindowViewport(), area);
}
template<class Destination>
bool inputSingle(Destination& destination, ImGuiInputTextFlags flags,
    WorkingBuffer& buffer, ImGuiInputTextCallback callback) {
    TextCallbackUserData data{&buffer};
    const bool changed = ImGui::InputText("###INPUT", buffer.data(), buffer.size(),
        flags | ImGuiInputTextFlags_CallbackResize, callback, &data);
    if constexpr (std::is_same_v<Destination, compat::RetainedString>) {
        compat::RetainedStringSlice stored;
        storeInputText(stored, std::span<const char>(buffer.data(), buffer.size()));
        destination = stored.retainedString();
    } else {
        storeInputText(destination, std::span<const char>(buffer.data(), buffer.size()));
    }
    if (changed) return true;
    setSourceTextInputRectangle();
    return false;
}
}

bool ngInputText(std::string_view id, InputTextString& buffer, ImGuiInputTextFlags flags) {
    return ngInputText(id, ngAvailableSpace().x, buffer, flags);
}
bool ngInputText(std::string_view id, float width, InputTextString& buffer, ImGuiInputTextFlags flags) {
    auto working = inputTextBuffer(buffer);
    IdScope idScope(id);
    WidthScope widthScope(width);
    return inputSingle(buffer, flags, working, ngInputTextResizeCallback);
}
bool ngInputText(std::string_view id, std::string_view label, InputTextString& buffer,
    ImGuiInputTextFlags flags) {
    return ngInputText(id, label, ngAvailableSpace().x, buffer, flags);
}
bool ngInputText(std::string_view id, std::string_view label, float width, InputTextString& buffer,
    ImGuiInputTextFlags flags) {
    auto working = inputTextBuffer(buffer);
    IdScope idScope(id);
    WidthScope widthScope(width);
    SuccessLabel<std::string_view> successLabel{label};
    return inputSingle(buffer, flags, working, ngLabeledInputTextResizeCallback);
}
bool ngInputTextMultiline(std::string_view id, InputTextString& buffer, ImVec2 size,
    ImGuiInputTextFlags flags) {
    // The independent source intentionally leaves the ID/width/label block commented out.
    (void)id;
    auto working = inputTextBuffer(buffer);
    TextCallbackUserData data{&working};
    const bool changed = ImGui::InputTextMultiline("###INPUT", working.data(), working.size(), size,
        flags | ImGuiInputTextFlags_CallbackResize, ngMultilineInputTextResizeCallback, &data);
    storeInputText(buffer, std::span<const char>(working.data(), working.size()));
    if (changed) return true;
    setSourceTextInputRectangle();
    return false;
}

bool ngInputText(std::string_view id, compat::RetainedStringSlice& buffer, ImGuiInputTextFlags flags) {
    return ngInputText(id, ngAvailableSpace().x, buffer, flags);
}
bool ngInputText(std::string_view id, float width, compat::RetainedStringSlice& buffer,
    ImGuiInputTextFlags flags) {
    auto working = inputTextBuffer(buffer);
    IdScope idScope(id);
    WidthScope widthScope(width);
    return inputSingle(buffer, flags, working, ngInputTextResizeCallback);
}
bool ngInputText(std::string_view id, std::string_view label, compat::RetainedStringSlice& buffer,
    ImGuiInputTextFlags flags) {
    return ngInputText(id, label, ngAvailableSpace().x, buffer, flags);
}
bool ngInputText(std::string_view id, std::string_view label, float width,
    compat::RetainedStringSlice& buffer, ImGuiInputTextFlags flags) {
    auto working = inputTextBuffer(buffer);
    IdScope idScope(id);
    WidthScope widthScope(width);
    SuccessLabel<std::string_view> successLabel{label};
    return inputSingle(buffer, flags, working, ngLabeledInputTextResizeCallback);
}
bool ngInputTextMultiline(std::string_view id, compat::RetainedStringSlice& buffer, ImVec2 size,
    ImGuiInputTextFlags flags) {
    (void)id;
    auto working = inputTextBuffer(buffer);
    TextCallbackUserData data{&working};
    const bool changed = ImGui::InputTextMultiline("###INPUT", working.data(), working.size(), size,
        flags | ImGuiInputTextFlags_CallbackResize, ngMultilineInputTextResizeCallback, &data);
    storeInputText(buffer, std::span<const char>(working.data(), working.size()));
    if (changed) return true;
    setSourceTextInputRectangle();
    return false;
}
bool ngInputText(std::string_view id, compat::RetainedStringSlice label,
    compat::RetainedStringSlice& buffer, ImGuiInputTextFlags flags) {
    return ngInputText(id, label, ngAvailableSpace().x, buffer, flags);
}
bool ngInputText(std::string_view id, compat::RetainedStringSlice label, float width,
    compat::RetainedStringSlice& buffer, ImGuiInputTextFlags flags) {
    auto working = inputTextBuffer(buffer);
    IdScope idScope(id);
    WidthScope widthScope(width);
    SuccessLabel<compat::RetainedStringSlice> successLabel{label};
    return inputSingle(buffer, flags, working, ngLabeledInputTextResizeCallback);
}
bool ngInputText(compat::RetainedStringSlice id, compat::RetainedStringSlice label,
    compat::RetainedStringSlice& buffer, ImGuiInputTextFlags flags) {
    return ngInputText(id, label, ngAvailableSpace().x, buffer, flags);
}
bool ngInputText(compat::RetainedStringSlice id, compat::RetainedStringSlice label, float width,
    compat::RetainedStringSlice& buffer, ImGuiInputTextFlags flags) {
    return ngInputText(id.view(), label, width, buffer, flags);
}
bool ngInputText(compat::RetainedStringSlice id, compat::RetainedStringSlice& buffer,
    ImGuiInputTextFlags flags) {
    return ngInputText(id.view(), ngAvailableSpace().x, buffer, flags);
}
bool ngInputText(compat::RetainedStringSlice id, float width, compat::RetainedStringSlice& buffer,
    ImGuiInputTextFlags flags) {
    return ngInputText(id.view(), width, buffer, flags);
}
bool ngInputTextMultiline(compat::RetainedStringSlice id, compat::RetainedStringSlice& buffer, ImVec2 size,
    ImGuiInputTextFlags flags) {
    return ngInputTextMultiline(id.view(), buffer, size, flags);
}
bool ngInputText(compat::RetainedStringSlice id, compat::RetainedString& buffer,
    ImGuiInputTextFlags flags) {
    return ngInputText(id, ngAvailableSpace().x, buffer, flags);
}
bool ngInputText(compat::RetainedStringSlice id, float width, compat::RetainedString& buffer,
    ImGuiInputTextFlags flags) {
    auto working = inputTextBuffer(compat::RetainedStringSlice(buffer));
    IdScope idScope(id.view());
    WidthScope widthScope(width);
    return inputSingle(buffer, flags, working, ngInputTextResizeCallback);
}
bool ngInputText(compat::RetainedStringSlice id, compat::RetainedStringSlice label,
    compat::RetainedString& buffer, ImGuiInputTextFlags flags) {
    return ngInputText(id, label, ngAvailableSpace().x, buffer, flags);
}
bool ngInputText(compat::RetainedStringSlice id, compat::RetainedStringSlice label, float width,
    compat::RetainedString& buffer, ImGuiInputTextFlags flags) {
    auto working = inputTextBuffer(compat::RetainedStringSlice(buffer));
    IdScope idScope(id.view());
    WidthScope widthScope(width);
    SuccessLabel<compat::RetainedStringSlice> successLabel{label};
    return inputSingle(buffer, flags, working, ngLabeledInputTextResizeCallback);
}
bool ngInputTextMultiline(compat::RetainedStringSlice id, compat::RetainedString& buffer, ImVec2 size,
    ImGuiInputTextFlags flags) {
    (void)id;
    auto working = inputTextBuffer(compat::RetainedStringSlice(buffer));
    TextCallbackUserData data{&working};
    const bool changed = ImGui::InputTextMultiline("###INPUT", working.data(), working.size(), size,
        flags | ImGuiInputTextFlags_CallbackResize, ngMultilineInputTextResizeCallback, &data);
    compat::RetainedStringSlice stored;
    storeInputText(stored, std::span<const char>(working.data(), working.size()));
    buffer = stored.retainedString();
    if (changed) return true;
    setSourceTextInputRectangle();
    return false;
}
} // namespace nijigenerate::widgets
