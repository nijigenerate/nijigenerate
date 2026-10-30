#pragma once
#include <optional>
#include <string>
#include <string_view>
#include <nijigenerate/compat/retained_string.hpp>

namespace nijigenerate::platform {
std::string ngJoinRelativePath(std::string base, std::string_view leaf);
std::optional<std::string> ngAppDataDirectory();
compat::RetainedString ngAppDataDirectoryRetained();
bool ngUsePlainConfigFallback();
std::optional<std::string> ngExtraLocaleDirectory();
compat::RetainedString ngExtraLocaleDirectoryRetained();
}
