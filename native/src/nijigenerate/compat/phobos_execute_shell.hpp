#pragma once

// Used default-argument closure of Phobos std.process.executeShell (BSL-1.0).
// See native/port/licenses/phobos-videoexport.txt.
#include "retained_string.hpp"
#include <cstdint>

namespace nijigenerate::compat::phobos {
struct ExecuteShellResult {
    std::int32_t status;
    RetainedString output;
};

// Inherit environment, working directory and stdin; capture merged stdout and
// stderr without a byte limit, using the platform's Phobos nativeShell.
ExecuteShellResult ngExecuteShell(RetainedString command);
}
