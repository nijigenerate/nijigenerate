#pragma once
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/shared_delegate.hpp>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

struct ImGuiIO;

namespace nijigenerate::widgets::detail {

// Internal value type; field implementations belong to the M307.S0003-S0008 leaves.
struct NotificationItem {
    std::uint64_t id = 0;
    float remaining = std::numeric_limits<float>::quiet_NaN();
    bool infinite = false;
    bool closed = false;
    // Null denotes callback-only; text retains the actual toStringz buffer.
    std::optional<compat::SourceCString> message;
    compat::SharedDelegate<void(ImGuiIO*)> callback;
};

} // namespace nijigenerate::widgets::detail
