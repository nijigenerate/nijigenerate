#pragma once
#include <string>
#include <string_view>

namespace nijigenerate::platform {
bool ngSettingsFileExists(std::string_view name);
std::string ngSettingsReadFileBytes(std::string_view name);
}
