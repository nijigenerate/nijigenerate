#include "detail/dynamic_apply_environment.hpp"
#include "detail/dynamic_sync.hpp"
#include <nijigenerate/core/actionstack.hpp>

namespace nijigenerate::commands::automesh::dynamic::detail {

void AutoMeshApplyEnvironment::recordApplyError(compat::RetainedStringSlice message) {
    synchronizedMonitor(applyLock, [&] { if (applyError.empty()) applyError = message; });
}

compat::RetainedStringSlice AutoMeshApplyEnvironment::currentApplyError() {
    compat::RetainedStringSlice result;
    synchronizedMonitor(applyLock, [&] { result = applyError; });
    return result;
}

void AutoMeshApplyEnvironment::incrementApplied() {
    synchronizedMonitor(applyLock, [&] { ++applied; });
}

std::size_t AutoMeshApplyEnvironment::appliedCount() {
    std::size_t result = 0;
    synchronizedMonitor(applyLock, [&] { result = applied; });
    return result;
}

void AutoMeshApplyEnvironment::ensureActionGroupOpen() {
    synchronizedMonitor(applyLock, [&] {
        if (!actionGroupOpen) {
            core::actionstack::incActionPushGroup();
            actionGroupOpen = true;
        }
    });
}

void AutoMeshApplyEnvironment::closeActionGroup() {
    synchronizedMonitor(applyLock, [&] {
        if (actionGroupOpen) {
            core::actionstack::incActionPopGroup();
            actionGroupOpen = false;
        }
    });
}

} // namespace nijigenerate::commands::automesh::dynamic::detail
