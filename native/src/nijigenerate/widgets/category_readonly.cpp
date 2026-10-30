#include "nijigenerate/widgets/category_internal.hpp"
namespace nijigenerate::widgets::category::detail {
bool& readOnlyCategoryContents() {
    thread_local bool value = false;
    return value;
}
}
namespace nijigenerate::widgets {
void ngWithReadOnlyCategoryContents(compat::SharedDelegate<void()> draw) {
    auto& state = category::detail::readOnlyCategoryContents();
    const bool previous = state;
    state = true;
    ImGui::BeginDisabled();
    // Explicit cleanup preserves throwing-boundary propagation without a noexcept destructor.
    try {
        draw();
    } catch (...) {
        ImGui::EndDisabled();
        state = previous;
        throw;
    }
    ImGui::EndDisabled();
    state = previous;
}
}
