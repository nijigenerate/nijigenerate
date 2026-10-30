#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/toggle.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace nijigenerate::widgets {
namespace {
template<class Operation> auto allocateCheckboxText(Operation&& operation) {
    try { return std::forward<Operation>(operation)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
bool ngCheckbox(const char* text, bool* value) {
    if (!text || !value) return false;
    auto& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(2, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
    const auto label = allocateCheckboxText([&] { return std::string(text); });
    const bool hasLabel = !label.empty() && !label.starts_with("###");
    const float toggleWidth = 36;
    const float toggleHeight = ImGui::GetFrameHeight();
    const auto lineStart = ImGui::GetCursorScreenPos();
    if (hasLabel) {
        ImGui::AlignTextToFramePadding();
        ImGui::Text(text);
        ImGui::SameLine(0, style.ItemInnerSpacing.x);
        const float textHeight = ImGui::GetTextLineHeight();
        const float centerY = lineStart.y + textHeight * .5f;
        float toggleTop = centerY - toggleHeight * .5f;
        if (toggleTop < lineStart.y) toggleTop = lineStart.y;
        const auto available = ImGui::GetContentRegionAvail();
        auto position = ImGui::GetCursorScreenPos();
        float switchX = position.x + available.x - toggleWidth;
        if (switchX < position.x) switchX = position.x;
        position.x = switchX;
        position.y = toggleTop;
        ImGui::SetCursorScreenPos(position);
    }
    const std::string_view id = label.empty() ? std::string_view("##toggle") : std::string_view(label);
    const auto switchId = allocateCheckboxText([&] {
        std::string result;
        constexpr std::string_view suffix = "##switch";
        if (id.size() > result.max_size() - suffix.size())
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        result.reserve(id.size() + suffix.size());
        result.append(id);
        result.append(suffix);
        return result;
    });
    const bool result = ngToggleSwitch(switchId, *value, ImVec2(toggleWidth, toggleHeight));
    ImGui::PopStyleVar(3);
    return result;
}
}
