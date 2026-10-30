#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/const_slice.hpp>
#include <cstddef>
#include <cstdint>

namespace nijigenerate::api::acp::client::detail {

// Actual byte preparation used by the real client methods.
std::ptrdiff_t findByteIndex(compat::ConstSlice<std::uint8_t> bytes, std::uint8_t value);
compat::Slice<std::uint8_t> truncateAtNull(compat::Slice<std::uint8_t> bytes);
compat::RetainedStringSlice sanitizeJsonPayload(compat::RetainedStringSlice payload);
compat::RetainedStringSlice escapeJsonString(compat::RetainedStringSlice text);

} // namespace nijigenerate::api::acp::client::detail
