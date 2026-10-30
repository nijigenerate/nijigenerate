#include "nijigenerate/widgets/tooltip.hpp"
#include "nijigenerate/core/mainfont.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <imgui.h>
#include <new>
#include <stdexcept>

namespace nijigenerate::widgets {
void ngTooltip(compat::RetainedStringSlice tip) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushFont(core::incMainFont());
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35);
        const char* begin = tip.data();
        // Preserve a null/empty source range without native null-pointer arithmetic.
        ImGui::TextUnformatted(begin, begin ? begin + tip.size() : begin);
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        ImGui::EndTooltip();
    }
    ImGui::PopStyleVar();
}
void ngTooltip(std::string_view tip) {
    const auto retained = [&] {
        try { return compat::RetainedStringSlice(compat::RetainedString(tip)); }
        catch (const std::bad_alloc&) { throw compat::ngStaticError<compat::OutOfMemoryError>(); }
        catch (const std::length_error&) { throw compat::ngStaticError<compat::OutOfMemoryError>(); }
    }();
    ngTooltip(retained);
}
} // namespace nijigenerate::widgets
