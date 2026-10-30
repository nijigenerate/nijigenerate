// Altered independent D Nagscreen port; the original BSD notice is in nagscreen.hpp.
#include "nagscreen.hpp"
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/detail/source_c_string_contract.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/utils/link.hpp>
#include <nijigenerate/widgets/label.hpp>
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <array>
#include <optional>
#include <string_view>

namespace nijigenerate::widgets::modal {
using core::translation::ngTranslateRetained;
namespace {
std::int64_t clockSeconds() {
    // Preserve the source conversion contract without requiring an x86 intrinsic.
    // 2^63 is exactly representable; double(INT64_MAX) rounds up to that boundary.
    constexpr double limit = 9223372036854775808.0;
    const double integral = std::trunc(ImGui::GetTime());
    if (!std::isfinite(integral) || integral < -limit || integral >= limit)
        return std::numeric_limits<std::int64_t>::min();
    return static_cast<std::int64_t>(integral);
}
std::int64_t subtract(std::int64_t a, std::int64_t b) {
    return std::bit_cast<std::int64_t>(std::bit_cast<std::uint64_t>(a) - std::bit_cast<std::uint64_t>(b));
}
}

Nagscreen::Nagscreen(compat::RetainedString title, compat::RetainedStringSlice msg,
    std::int32_t timeout, float baseHeight) : Modal(title, false) {
    message = msg;
    this->timeout = timeout;
    cfg.headingFormats[0] = {2, true};
    cfg.headingFormats[1] = {1.5f, false};
    cfg.headingFormats[2] = {1.2f, false};
    cfg.linkCallback = [](markdown::MarkdownLinkCallbackData data) {
        utils::link::ngOpenLink(data.link.view());
    };
    startTime = clockSeconds();
    uiSize = {512, baseHeight};
}

void Nagscreen::onUpdate() {
    const auto currentTime = clockSeconds();
    const auto passedTime = subtract(currentTime, startTime);
    const auto timeLeft = subtract(timeout, passedTime);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, origWindowPadding);
    auto* viewport = ImGui::GetWindowViewport();
    compat::ngRequireObject(viewport)->Flags |= ImGuiViewportFlags_TopMost;
    viewport->Flags |= ImGuiViewportFlags_NoDecoration;
    viewport->Flags |= ImGuiViewportFlags_NoTaskBarIcon;
    const ImVec2 origin = ImGui::GetCursorStartPos();
    (void)origin;
    ImGui::Indent();
    if (ImGui::BeginChild("##BODY", ImVec2(-4, 0), false, ImGuiWindowFlags_NoScrollbar)) {
        const ImVec2 available = ngAvailableSpace();
        ImGui::PushTextWrapPos(available.x);
        ImGui::SetCursorPosY(16);
        ImGui::SetWindowFontScale(1.8f);
        const ImVec2 size = ngMeasureString(compat::RetainedStringSlice(ngTranslateRetained(title())));
        ngDummy({(available.x / 2) - (size.x / 2), size.y});
        ImGui::SameLine(0, 0);
        ngTextShadowed(compat::RetainedStringSlice(ngTranslateRetained(title())));
        ImGui::SetWindowFontScale(1);
        ImGui::NewLine();
        ngDummy({16, 64});
        ImGui::SameLine(0, 0);
        const ImVec2 bodyAvailable = ngAvailableSpace();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.176f, 0.447f, 0.698f, 1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.313f, 0.521f, 0.737f, 1));
        ImGui::BeginGroup();
        ImGui::PushItemWidth(bodyAvailable.x - 16);
        markdown::ngMarkdown(compat::RetainedStringSlice(ngTranslateRetained(message.retainedString())), cfg);
        ImGui::PopItemWidth();
        ImGui::EndGroup();
        ImGui::PopStyleColor();
        ImGui::PopStyleColor();
        ngDummy({0, -40});
        ngDummy({(available.x / 2) - 32, 24});
        ImGui::SameLine(0, 0);
        std::optional<compat::SourceCString> numericButton;
        std::optional<core::translation::TranslatedCString> translatedButton;
        const char* button;
        if (timeLeft > 0) {
            const std::array arguments{core::translation::ngFormatArgument(timeLeft)};
            const auto text = core::translation::ngFormatTyped("(%s)", arguments);
            numericButton.emplace(std::string_view(text));
            compat::detail::ngSourceCStringPostconditions(text, numericButton->data());
            button = numericButton->data();
        } else {
            translatedButton.emplace(core::translation::ngTranslateCString("Close"));
            button = translatedButton->data();
        }
        ImGui::BeginDisabled(timeLeft > 0);
        if (ngButtonColored(button, {64, 24})) ngModalCloseTop();
        ImGui::EndDisabled();
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild();
    ImGui::Unindent();
    ImGui::PopStyleVar();
}
}
