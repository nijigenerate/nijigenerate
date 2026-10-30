#pragma once

#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <cstdint>

namespace nijigenerate::autorig::deterministic::native_detail {

// Private helpers consumed by the actual native.d stage implementation.
std::uint32_t uuid(core::settings::SettingsJsonValue value);
compat::Slice<float> nativeNumbers(core::settings::SettingsJsonValue value);

} // namespace nijigenerate::autorig::deterministic::native_detail
