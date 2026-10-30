#include "autosave.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/platform/autosave_path_policy.hpp>
#include <nijigenerate/platform/autosave_files.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <array>

namespace nijigenerate::io::autosave {
namespace detail { std::string_view lockfileName() noexcept; }
void incReleaseLockfile() {
    auto path = project::incProjectPath();
    path = path.slice(0, platform::ngAutosaveStripExtension(path.view()).size());
    if (path.empty()) return;
    const auto directory = getAutosaveDir(path);
    const std::array segments{directory.retainedString(),
        compat::RetainedString::fromStaticView(detail::lockfileName())};
    const AutosaveString lockfile(compat::phobos::ngBuildPath(segments));
    if (platform::ngAutosaveExists(lockfile.view())) platform::ngAutosaveRemove(lockfile);
}
}
