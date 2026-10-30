#include "command_browser_internal.hpp"

#include <imgui.h>
#include <string_view>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>

namespace nijigenerate::windows::command_browser {

thread_local bool incIsCommandBrowserOpen = false;

namespace detail {
std::uint32_t gBindingNextId = 1;

compat::RetainedStringSlice trimAtNul(compat::RetainedStringSlice text) {
    using Cursor = compat::phobos::detail::UnicodeCursor<std::string_view, char>;
    auto cursor = Cursor(text.view());
    std::size_t count = 0;
    while (!cursor.empty()) {
        // countUntil counts decoded scalar steps; the original then uses
        // that count as a byte-slice endpoint, even after non-ASCII text.
        if (cursor.front() == U'\0') return text.slice(0, count);
        cursor.popFront();
        ++count;
    }
    return text;
}

void textWrappedInColumn(const char* text) {
    if (!text) {
        ImGui::Text("-");
        return;
    }
    const auto cursorPosition = ImGui::GetCursorPosX();
    const auto columnWidth = ImGui::GetColumnWidth();
    const auto wrapPosition = cursorPosition + columnWidth;
    ImGui::PushTextWrapPos(wrapPosition);
    ImGui::TextWrapped(text);
    ImGui::PopTextWrapPos();
}

} // namespace detail

} // namespace nijigenerate::windows::command_browser
