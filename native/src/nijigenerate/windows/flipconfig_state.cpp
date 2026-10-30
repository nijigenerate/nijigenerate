#include <nijigenerate/windows/detail/flipconfig_state.hpp>
#include <nijigenerate/project.hpp>
#include <puppet.hpp>

namespace nijigenerate::windows::flipconfig {

namespace detail {
thread_local compat::Slice<std::shared_ptr<FlipPair>> flipPairs;
thread_local std::shared_ptr<nicxlive::core::Puppet> activePuppet;
} // namespace detail

compat::Slice<std::shared_ptr<FlipPair>> ngGetFlipPairs() {
    bool changed;
    {
        const auto currentPuppet = project::incActivePuppet();
        changed = !nicxlive::core::ngPuppetObjectEquals(currentPuppet, detail::activePuppet);
    }
    if (changed) {
        // The second call supplies the assigned reference; do not reuse the
        // comparison result or clear the backing storage of prior descriptors.
        detail::activePuppet = project::incActivePuppet();
        detail::flipPairs.resize(0);
    }
    return detail::flipPairs;
}

} // namespace nijigenerate::windows::flipconfig
