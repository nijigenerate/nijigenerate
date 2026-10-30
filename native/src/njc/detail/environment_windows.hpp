#pragma once

#include "../config.hpp"

namespace njc::config::detail {

// Actual platform environment lookup; this header carries no OS handles or APIs.
Text ngEnvironmentValue(const char* name);

} // namespace njc::config::detail
