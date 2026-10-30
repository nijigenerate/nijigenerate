// Altered independent D Markdown port; the original zlib notice is in markdown.hpp.
#include "nijigenerate/widgets/markdown.hpp"
#include "nijigenerate/widgets/markdown_internal.hpp"
#include "nijigenerate/compat/array_range_error.hpp"
#include <bit>
#include <cstddef>
#include <cstdint>

namespace nijigenerate::widgets::markdown::detail {
void formatDefault(MarkdownFormatInfo& info, bool start) {
    switch (info.type) {
    case MarkdownFormatType::Heading: {
        std::size_t index;
        if (info.level > MarkdownConfig::NUMHEADINGS) {
            index = MarkdownConfig::NUMHEADINGS - 1;
        } else {
            // D wraps the signed32 subtraction before native-width conversion.
            const auto wrapped = std::uint32_t(info.level) - std::uint32_t(1);
            index = static_cast<std::size_t>(std::bit_cast<std::int32_t>(wrapped));
        }
        if (index >= info.config.headingFormats.size()) {
            throw compat::ngStaticError<compat::ArrayIndexError>(
                index, info.config.headingFormats.size(),
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/widgets/markdown.d"), 116);
        }
        const auto format = info.config.headingFormats[index];
        if (start) {
            ImGui::SetWindowFontScale(format.scale);
        } else {
            if (format.separator) ImGui::Separator();
            ImGui::SetWindowFontScale(1.0f);
        }
        break;
    }
    case MarkdownFormatType::Link:
        if (start) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_Button]);
        } else {
            ImGui::PopStyleColor();
            underline(ImGui::GetStyle().Colors[info.itemHovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button]);
        }
        break;
    default:
        break;
    }
}
}
