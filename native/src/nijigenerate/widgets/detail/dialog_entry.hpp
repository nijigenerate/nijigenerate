#pragma once

#include "nijigenerate/widgets/dialog.hpp"
#include "nijigenerate/widgets/detail/dialog_values.hpp"

#include <cstdint>

namespace nijigenerate::widgets::dialog::detail {

// Internal value type; no cross-language binary layout is exposed.
struct DialogEntry {
    RetainedPointer<const char> tag;
    RetainedPointer<const char> title;
    DialogText text;
    DialogLevel level = DialogLevel::Info;
    DialogButtons btns = DialogButtons::NONE;
    DialogButtons selected = DialogButtons::NONE;
    std::int32_t btncount = 0;
    RetainedPointer<void> userData;
};

} // namespace nijigenerate::widgets::dialog::detail
