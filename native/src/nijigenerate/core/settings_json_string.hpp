#pragma once
#include "settings_value.hpp"
#include <string>
namespace nijigenerate::core::settings {
std::string ngSettingsJsonString(const SettingsJsonValue& value);
}
