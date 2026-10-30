#pragma once

#include <nijigenerate/core/settings_value.hpp>

namespace nijigenerate::autorig::deterministic::native_detail {

// Recursive JSON mutation used by the actual editorSignature serialization path.
void authoredState(core::settings::SettingsJsonValue node);

} // namespace nijigenerate::autorig::deterministic::native_detail
