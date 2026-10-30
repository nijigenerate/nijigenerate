#include "nijigenerate/widgets/category_internal.hpp"
#include "nijigenerate/widgets/dummy.hpp"
#include "nijigenerate/core/window_theme.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <imgui_internal.h>
#include <algorithm>
#include <cstring>
#include <type_traits>

namespace nijigenerate::widgets {
bool ngBeginCategory(const char* title, ImVec4 color, IncCategoryFlags flags, CategoryCallback callback) {
    auto clamp = [](float value) { return static_cast<float>(std::min(std::max(double(value), 0.15), 1.0)); };
    color.w = 1.f;
    color.x = clamp(color.x);
    color.y = clamp(color.y);
    color.z = clamp(color.z);
    ImVec4 hover, active, background, shadow, text;
    bool isDark;
    category::detail::getCategoryColors(color, hover, active, background, shadow, text, isDark);
    ImGui::PushID(title);
    auto* storage = ImGui::GetStateStorage();
    const auto id = ImGui::GetID("CATEGORYDATA");
    auto* window = ImGui::GetCurrentWindow();
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, hover);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, active);
    ImGui::PushStyleColor(ImGuiCol_Text, text);
    auto* data = static_cast<category::detail::CategoryData*>(compat::ngRequireObject(storage)->GetVoidPtr(id));
    if (!data) {
        using Data = category::detail::CategoryData;
        static_assert(std::is_aggregate_v<Data> && std::is_trivially_copyable_v<Data>);
        data = static_cast<Data*>(ImGui::MemAlloc(sizeof(Data)));
        if (!data) compat::ngFailNullAccess(true);
        // C++20 memmove starts the implicit-lifetime record without running
        // default constructors or changing the allocator-provided bytes.
        // The source initializes only these two fields before SetVoidPtr.
        data = static_cast<Data*>(std::memmove(data, data, sizeof(Data)));
        data->open = false;
        data->contentBounds = ImVec4(0, 0, 0, 0);
        storage->SetVoidPtr(id, data);
    }
    data->flags = flags;
    data->badColor = isDark != core::ngGetDarkMode();
    data->isDark = isDark;
    data->contentBounds.x = ImGui::GetCursorPosX();
    data->contentBounds.y = ImGui::GetCursorPosY();
    data->contentBounds.z = ngAvailableSpace().x;
    const auto cursor = ImGui::GetCursorScreenPos();
    auto* backgroundDraw = ImGui::GetWindowDrawList();
    const ImVec2 backgroundMin(cursor.x, cursor.y);
    const ImVec2 backgroundMax(cursor.x + data->contentBounds.z, cursor.y + data->contentBounds.w + 1);
    const auto backgroundColor = ImGui::GetColorU32(background);
    compat::ngRequireObject(backgroundDraw)->AddRectFilled(backgroundMin, backgroundMax, backgroundColor, 6);
    auto* shadowDraw = ImGui::GetWindowDrawList();
    const ImVec2 shadowMin(cursor.x, cursor.y + data->contentBounds.w - 1);
    const ImVec2 shadowMax(cursor.x + data->contentBounds.z, cursor.y + data->contentBounds.w + 1);
    const auto shadowColor = ImGui::GetColorU32(shadow);
    compat::ngRequireObject(shadowDraw)->AddRectFilled(shadowMin, shadowMax, shadowColor, 6);
    ngDummy(ImVec2(0, 2));
    const float paddingX = ImGui::GetStyle().WindowPadding.x / 2;
    compat::ngRequireObject(window)->ContentRegionRect.Min.x -= paddingX;
    window->WorkRect.Min.x -= paddingX;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + paddingX);
    if (data->open) {
        const auto nextCursor = ImGui::GetCursorScreenPos();
        const float diffY = nextCursor.y - cursor.y;
        auto* contentDraw = ImGui::GetWindowDrawList();
        const ImVec2 contentMin(cursor.x + 1, cursor.y + 1);
        const ImVec2 contentMax(cursor.x + data->contentBounds.z - 1,
            nextCursor.y + data->contentBounds.w - diffY);
        const auto contentColor = ImGui::GetColorU32(color);
        compat::ngRequireObject(contentDraw)->AddRectFilled(contentMin, contentMax, contentColor, 6);
    }
    if (callback) callback(data->contentBounds.z, data->contentBounds.w);
    window->ContentRegionRect.Min.x += paddingX;
    window->WorkRect.Min.x += paddingX;
    if ((data->flags & IncCategoryFlags::NoCollapse) == IncCategoryFlags::NoCollapse) {
        data->open = true;
        ImGui::Indent();
        ImGui::Text(title);
        ImGui::Unindent();
    } else {
        const bool defaultClosed = (data->flags & IncCategoryFlags::DefaultClosed) == IncCategoryFlags::DefaultClosed;
        if (category::detail::readOnlyCategoryContents()) ImGui::EndDisabled();
        try {
            data->open = ImGui::TreeNodeEx(title, ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowItemOverlap |
                (defaultClosed ? 0 : ImGuiTreeNodeFlags_DefaultOpen));
        } catch (...) {
            if (category::detail::readOnlyCategoryContents()) ImGui::BeginDisabled();
            throw;
        }
        if (category::detail::readOnlyCategoryContents()) ImGui::BeginDisabled();
    }
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
    if (data->open) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
        ImGui::Indent();
    }
    ImGui::PopStyleColor(3);
    const float indentSpacing = ImGui::GetStyle().IndentSpacing;
    window->ContentRegionRect.Min.x -= indentSpacing;
    window->WorkRect.Min.x -= indentSpacing;
    if (data->badColor && data->isDark) core::ngPushDarkColorScheme();
    else if (data->badColor && !data->isDark) core::ngPushLightColorScheme();
    return data->open;
}

void ngEndCategory() {
    auto* window = ImGui::GetCurrentWindow();
    auto& contentMinX = compat::ngRequireObject(window)->ContentRegionRect.Min.x;
    contentMinX += ImGui::GetStyle().IndentSpacing;
    window->WorkRect.Min.x += ImGui::GetStyle().IndentSpacing;
    auto* storage = ImGui::GetStateStorage();
    const auto id = ImGui::GetID("CATEGORYDATA");
    if (auto* data = static_cast<category::detail::CategoryData*>(compat::ngRequireObject(storage)->GetVoidPtr(id))) {
        if (data->badColor) core::ngPopColorScheme();
        if (data->open) {
            ImGui::Unindent();
            ngDummy(ImVec2(0, 2));
        }
        data->contentBounds.w = ImGui::GetCursorPosY() - data->contentBounds.y;
        ImGui::Spacing();
        ImGui::Spacing();
    }
    ImGui::PopID();
}
}
