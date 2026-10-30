// Copyright 2020-2023 Inochi2D Project; 2024 nijigenerate Project.
// Author: Luna Nielsen. BSD-2-Clause; see LICENSE.
#include <nijigenerate/widgets/dummy.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <stdexcept>

namespace nijigenerate::widgets {
ImVec2 ngAvailableSpace() { return ImGui::GetContentRegionAvail(); }

ImVec2 ngMeasureString(compat::RetainedStringSlice text) {
    const auto* begin = text.data();
    const auto* end = begin ? begin + text.size() : begin;
    return ImGui::CalcTextSize(begin, end);
}
ImVec2 ngMeasureString(std::string_view text) {
    const auto retained = [&] {
        try { return compat::RetainedStringSlice(compat::RetainedString(text)); }
        catch (const std::bad_alloc&) { throw compat::ngStaticError<compat::OutOfMemoryError>(); }
        catch (const std::length_error&) { throw compat::ngStaticError<compat::OutOfMemoryError>(); }
    }();
    return ngMeasureString(retained);
}
} // namespace nijigenerate::widgets
