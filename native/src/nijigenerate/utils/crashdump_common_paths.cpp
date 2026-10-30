#include "crashdump.hpp"
#include "nijigenerate/platform/environment.hpp"
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <array>

namespace nijigenerate::utils::crashdump {

compat::RetainedStringSlice linuxStateHome() {
    const auto home = platform::ngGetEnvironmentVariableRetained("HOME");
    if (home.empty()) throw compat::ThrowableException(
        detail::allocateCrashText([] { return compat::RetainedString("Environment variable not found: HOME"); }),
        compat::RetainedString::fromStaticLiteral("std/process.d"), 193);
    const std::array<compat::RetainedString, 3> segments{home,
        compat::RetainedString::fromStaticLiteral(".local"), compat::RetainedString::fromStaticLiteral("state")};
    const auto fallback = compat::phobos::ngBuildPath(segments);
    const auto selected = platform::ngGetEnvironmentVariableRetained("XDG_STATE_HOME");
    if (!selected.isNull()) return selected;
    return fallback;
}

} // namespace nijigenerate::utils::crashdump
