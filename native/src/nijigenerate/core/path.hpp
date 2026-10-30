#pragma once

#include <string>
#include <optional>
#include <nijigenerate/compat/retained_string.hpp>

namespace nijigenerate::core::path {
inline constexpr auto APP_FOLDER_NAME = "nijigenerate";
inline constexpr auto ENV_CONFIG_PATH = "INOCHI_CONFIG_PATH";
std::string incGetAppConfigPath();
compat::RetainedString ngGetAppConfigPath();
compat::RetainedString ngGetAppImguiConfigFile();
std::string incGetAppImguiConfigFile();
compat::RetainedString ngGetAppFontsPath();
std::string incGetAppFontsPath();
compat::RetainedString ngGetAppLocalePath();
std::string incGetAppLocalePath();
compat::RetainedString ngGetAppLocalePathExtra();
std::optional<std::string> incGetAppLocalePathExtra();
} // namespace nijigenerate::core::path
