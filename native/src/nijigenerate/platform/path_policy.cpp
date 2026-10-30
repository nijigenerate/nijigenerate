#include "path_policy.hpp"
#include "environment.hpp"
#include "detail/path_allocation.hpp"
#include <utility>

namespace nijigenerate::platform {
std::string ngJoinRelativePath(std::string base, std::string_view leaf) {
    return detail::ngPathAllocation([&] {
        if (!base.empty()) {
#if defined(_WIN32)
            if (base.back() != '/' && base.back() != '\\') base += '\\';
#else
            if (base.back() != '/') base += '/';
#endif
        }
        base += leaf;
        return std::move(base);
    });
}
compat::RetainedString ngAppDataDirectoryRetained() {
#if defined(_WIN32)
    return ngGetEnvironmentVariableRetained("AppData");
#elif defined(__linux__)
    auto value = ngGetEnvironmentVariableRetained("XDG_CONFIG_HOME");
    if (value.isNull()) {
        const auto home = ngGetEnvironmentVariableRetained("HOME");
        auto bytes = detail::ngPathAllocation([&] { return std::string(home.view()); });
        bytes = ngJoinRelativePath(std::move(bytes), ".config");
        value = detail::ngPathAllocation([&] { return compat::RetainedString(std::move(bytes)); });
    }
    return value;
#elif defined(__APPLE__)
    auto value = ngGetEnvironmentVariableRetained("HOME");
    if (!value.isNull()) {
        auto bytes = detail::ngPathAllocation([&] { return std::string(value.view()); });
        bytes = ngJoinRelativePath(std::move(bytes), "Library");
        bytes = ngJoinRelativePath(std::move(bytes), "Application Support");
        value = detail::ngPathAllocation([&] { return compat::RetainedString(std::move(bytes)); });
    }
    return value;
#else
    return ngGetEnvironmentVariableRetained("HOME");
#endif
}
std::optional<std::string> ngAppDataDirectory() {
    const auto value = ngAppDataDirectoryRetained();
    if (value.isNull()) return std::nullopt;
    return detail::ngPathAllocation([&] { return std::string(value.view()); });
}
bool ngUsePlainConfigFallback() {
#if defined(__linux__) || defined(__APPLE__)
    return true;
#else
    return false;
#endif
}
compat::RetainedString ngExtraLocaleDirectoryRetained() {
#if defined(__linux__)
    const auto value = ngGetEnvironmentVariableRetained("HERE");
    return value;
#else
    return {};
#endif
}
std::optional<std::string> ngExtraLocaleDirectory() {
    const auto value = ngExtraLocaleDirectoryRetained();
    if (value.isNull()) return std::nullopt;
    return detail::ngPathAllocation([&] { return std::string(value.view()); });
}
}
