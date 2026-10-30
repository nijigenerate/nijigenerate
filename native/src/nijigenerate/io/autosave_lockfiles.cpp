#include "autosave.hpp"
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <nijigenerate/platform/autosave_files.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <array>

namespace nijigenerate::io::autosave {
namespace detail { std::string_view lockfileName() noexcept; }

void incCreateLockfile(AutosaveString projectPath) {
    projectPath = projectPath.slice(0, platform::ngAutosaveStripExtension(projectPath.view()).size());
    auto directory = getAutosaveDir(projectPath);
    const std::array segments{directory.retainedString(),
        compat::RetainedString::fromStaticView(detail::lockfileName())};
    const AutosaveString lockfile(compat::phobos::ngBuildPath(segments));
    platform::ngAutosaveCreateDirectories(directory.view());
    platform::ngAutosaveWriteEmpty(lockfile);
}

bool incCheckLockfile(AutosaveString projectPath) {
    projectPath = projectPath.slice(0, platform::ngAutosaveStripExtension(projectPath.view()).size());
    const auto directory = getAutosaveDir(projectPath);
    const std::array segments{directory.retainedString(),
        compat::RetainedString::fromStaticView(detail::lockfileName())};
    const AutosaveString lockfile(compat::phobos::ngBuildPath(segments));
    return platform::ngAutosaveExists(lockfile.view());
}
}
