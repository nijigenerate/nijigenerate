#include "autosave.hpp"
#include <nijigenerate/core/path.hpp>
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <array>

namespace nijigenerate::io::autosave {
AutosaveString getAutosaveDir(AutosaveString projectPath) {
    // projectPath retains the basename bytes across the config-provider call.
    const auto basename = platform::ngAutosaveBaseName(projectPath.view());
    AutosaveString retainedBasename;
    if (!basename.empty()) {
        const auto offset = static_cast<std::size_t>(basename.data() - projectPath.data());
        retainedBasename = projectPath.slice(offset, offset + basename.size());
    }
    const auto config = core::path::ngGetAppConfigPath();
    const std::array segments{config, compat::RetainedString::fromStaticLiteral("autosaves"),
        retainedBasename.retainedString()};
    return AutosaveString(compat::phobos::ngBuildPath(segments));
}
} // namespace nijigenerate::io::autosave
