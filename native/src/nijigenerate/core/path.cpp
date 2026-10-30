#include "path.hpp"

#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/phobos_file_directory.hpp>

#include <nijigenerate/platform/environment.hpp>
#include <nijigenerate/platform/path_policy.hpp>
#include <nijigenerate/platform/detail/path_allocation.hpp>
#include <string_view>
#include <utility>

namespace nijigenerate::core::path {
namespace {
thread_local compat::RetainedString cachedConfigDir, cachedImguiFileDir, cachedFontDir, cachedLocaleDir, inForcedConfigDir;

std::string currentDirectory() {
    return compat::phobos::ngGetCurrentDirectory();
}
bool exists(std::string_view value) { return compat::phobos::ngFileExists(value); }
void createDirectory(std::string_view value) {
    compat::phobos::ngMkdirRecurse(value);
}

// All second segments in this module are nonempty relative literals. Preserve
// Phobos buildPath's existing separators, including repeated/trailing separators.
std::string join(std::string base, std::string_view leaf) {
    return platform::ngJoinRelativePath(std::move(base), leaf);
}
compat::RetainedString retainedJoin(compat::RetainedString base, std::string_view leaf) {
    auto bytes = platform::detail::ngPathAllocation([&] { return std::string(base.view()); });
    bytes = join(std::move(bytes), leaf);
    return platform::detail::ngPathAllocation([&] { return compat::RetainedString(std::move(bytes)); });
}
}

compat::RetainedString ngGetAppConfigPath() {
    if (!cachedConfigDir.isNull()) return cachedConfigDir;
    if (!inForcedConfigDir.isNull()) return inForcedConfigDir;
    auto appData = platform::ngAppDataDirectoryRetained();
    auto forced = platform::ngGetEnvironmentVariableRetained(ENV_CONFIG_PATH);
    inForcedConfigDir = forced;
    if (!inForcedConfigDir.isNull()) {
        const auto selected = inForcedConfigDir;
        // D scope(success) runs after assigning the retained forced path, so a
        // failed mkdir leaves the forced path cached for the next invocation.
        if (!exists(inForcedConfigDir.view())) createDirectory(inForcedConfigDir.view());
        return selected;
    }
    auto appDataDir = !appData.isNull()
        ? platform::detail::ngPathAllocation([&] { return std::string(appData.view()); }) : currentDirectory();
    auto base = platform::detail::ngPathAllocation([&] { return std::string(appDataDir); });
    const auto dotted = platform::detail::ngPathAllocation([] { return std::string(".") + APP_FOLDER_NAME; });
    auto candidate = join(std::move(base), dotted);
    if (platform::ngUsePlainConfigFallback() && !exists(candidate)) {
        base = platform::detail::ngPathAllocation([&] { return std::string(appDataDir); });
        candidate = join(std::move(base), APP_FOLDER_NAME);
    }
    appDataDir = std::move(candidate);
    const auto selected = platform::detail::ngPathAllocation([&] {
        return compat::RetainedString(std::move(appDataDir));
    });
    cachedConfigDir = selected;
    if (!exists(cachedConfigDir.view())) createDirectory(cachedConfigDir.view());
    return selected;
}
std::string incGetAppConfigPath() {
    const auto value = ngGetAppConfigPath();
    return platform::detail::ngPathAllocation([&] { return std::string(value.view()); });
}
compat::RetainedString ngGetAppImguiConfigFile() {
    if (!cachedImguiFileDir.isNull()) return cachedImguiFileDir;
    const auto config = ngGetAppConfigPath();
    cachedImguiFileDir = retainedJoin(config, "imgui.ini");
    return cachedImguiFileDir;
}
std::string incGetAppImguiConfigFile() {
    const auto value = ngGetAppImguiConfigFile();
    return platform::detail::ngPathAllocation([&] { return std::string(value.view()); });
}
compat::RetainedString ngGetAppFontsPath() {
    if (!cachedFontDir.isNull()) return cachedFontDir;
    const auto config = ngGetAppConfigPath();
    cachedFontDir = retainedJoin(config, "fonts");
    if (!exists(cachedFontDir.view())) createDirectory(cachedFontDir.view());
    return cachedFontDir;
}
std::string incGetAppFontsPath() {
    const auto value = ngGetAppFontsPath();
    return platform::detail::ngPathAllocation([&] { return std::string(value.view()); });
}
compat::RetainedString ngGetAppLocalePath() {
    if (!cachedLocaleDir.isNull()) return cachedLocaleDir;
    const auto config = ngGetAppConfigPath();
    cachedLocaleDir = retainedJoin(config, "i18n");
    if (!exists(cachedLocaleDir.view())) createDirectory(cachedLocaleDir.view());
    return cachedLocaleDir;
}
std::string incGetAppLocalePath() {
    const auto value = ngGetAppLocalePath();
    return platform::detail::ngPathAllocation([&] { return std::string(value.view()); });
}
compat::RetainedString ngGetAppLocalePathExtra() {
    return platform::ngExtraLocaleDirectoryRetained();
}
std::optional<std::string> incGetAppLocalePathExtra() {
    const auto value = ngGetAppLocalePathExtra();
    if (value.isNull()) return std::nullopt;
    return platform::detail::ngPathAllocation([&] { return std::string(value.view()); });
}
} // namespace nijigenerate::core::path
