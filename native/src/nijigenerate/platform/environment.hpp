#pragma once

#include <optional>
#include <string>
#include <nijigenerate/compat/retained_string.hpp>

namespace nijigenerate::platform {
// Live process environment; nullopt means absent, an empty string means present.
// POSIX callers serialize environment mutation.
compat::RetainedString ngGetEnvironmentVariableRetained(const char* name);
std::optional<std::string> ngGetEnvironmentVariable(const char* name);
// Source assignment takes value before name and returns the same descriptor.
compat::RetainedString ngSetEnvironmentVariableRetained(
    compat::RetainedString value, compat::RetainedString name);
} // namespace nijigenerate::platform
