#include "viewport.hpp"
#include "viewport_internal.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <imgui_internal.h>
#include <algorithm>
#include <limits>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::widgets {
namespace {
compat::RetainedStringSlice retainNativeToolAreaId(std::string_view id) {
    try { return compat::RetainedStringSlice(compat::RetainedString(id)); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}

void ngBeginViewportToolArea(compat::RetainedStringSlice id, ImVec2 pos, bool pad) {
    ngBeginViewportToolArea(id, ImGuiDir_Right, ImGuiDir_Up, pad, &pos);
}

void ngBeginViewportToolArea(compat::RetainedStringSlice id, ImGuiDir horizontal, ImGuiDir vertical,
                            bool pad, ImVec2* pos) {
    ImGui::SetItemAllowOverlap();
    const char* begin = id.data();
    ImGui::PushID(begin, begin ? begin + id.size() : begin);
    auto* storage = ImGui::GetStateStorage();
    auto* window = ImGui::GetCurrentWindow();
    auto& style = ImGui::GetStyle();
    const auto contentId = ImGui::GetID("CONTENT_SIZE");
    auto* data = static_cast<detail::ViewportToolAreaData*>(
        compat::ngRequireObject(storage)->GetVoidPtr(contentId));
    if (!data) {
        void* allocation = ImGui::MemAlloc(sizeof(detail::ViewportToolAreaData));
        data = static_cast<detail::ViewportToolAreaData*>(allocation);
        if (!data) compat::ngFailNullAccess(true);
        ::new (data) detail::ViewportToolAreaData{ImVec2(1, 1)};
        storage->SetVoidPtr(contentId, data);
    }
    const float paddingX = pad ? style.FramePadding.x : 0;
    const float paddingY = pad ? style.FramePadding.y : 0;
    ImVec2 cursor;
    if (pos) {
        const auto x = pos->x + compat::ngRequireObject(window)->InnerRect.Max.x;
        const auto y = pos->y + window->InnerRect.Max.y;
        cursor = ImVec2(x, y);
    } else {
        const auto x = horizontal == ImGuiDir_Right
            ? compat::ngRequireObject(window)->InnerRect.Min.x - (paddingX + data->contentSize.x)
            : compat::ngRequireObject(window)->InnerRect.Max.x + paddingX;
        const auto y = vertical == ImGuiDir_Down
            ? window->InnerRect.Min.y - (paddingY + data->contentSize.y)
            : window->InnerRect.Max.y + paddingY;
        cursor = ImVec2(x, y);
    }
    ImGui::SetCursorScreenPos(cursor);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::BeginChild("CONTENT_CHILD", data->contentSize, ImGuiChildFlags_None, flags);
}

void ngEndViewportToolArea() {
    auto* window = ImGui::GetCurrentWindow();
    ImGui::EndChild();
    auto* storage = ImGui::GetStateStorage();
    const auto contentId = ImGui::GetID("CONTENT_SIZE");
    auto* data = static_cast<detail::ViewportToolAreaData*>(
        compat::ngRequireObject(storage)->GetVoidPtr(contentId));
    if (data) {
        const auto width = std::clamp(compat::ngRequireObject(window)->ContentSize.x,
            1.0f, std::numeric_limits<float>::max());
        const auto height = std::clamp(window->ContentSize.y, 1.0f, std::numeric_limits<float>::max());
        data->contentSize = ImVec2(width, height);
    }
    ImGui::PopID();
}

void ngBeginViewportToolArea(std::string_view id, ImVec2 pos, bool pad) {
    ngBeginViewportToolArea(retainNativeToolAreaId(id), pos, pad);
}

void ngBeginViewportToolArea(std::string_view id, ImGuiDir horizontal, ImGuiDir vertical,
                            bool pad, ImVec2* pos) {
    ngBeginViewportToolArea(retainNativeToolAreaId(id),
        horizontal, vertical, pad, pos);
}

} // namespace nijigenerate::widgets
