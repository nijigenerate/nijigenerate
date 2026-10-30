#pragma once

#include <nijigenerate/compat/retained_string_slice.hpp>
#include <cstddef>
#include <memory>
#include <compat/source_mutex.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::commands::automesh::dynamic::detail {

// Actual captured locals shared by the worker's nested apply helpers. This does
// not supply a worker, queue, processor or callable-lifetime implementation.
class AutoMeshApplyEnvironment {
    std::shared_ptr<nicxlive::core::d_runtime::SourceMutex> applyLock =
        std::allocate_shared<nicxlive::core::d_runtime::SourceMutex>(
            nicxlive::core::d_runtime::SourceAllocator<nicxlive::core::d_runtime::SourceMutex>{});
    std::size_t applied = 0;
    compat::RetainedStringSlice applyError;
    bool actionGroupOpen = false;

public:
    AutoMeshApplyEnvironment() = default;
    AutoMeshApplyEnvironment(const AutoMeshApplyEnvironment&) = delete;
    AutoMeshApplyEnvironment& operator=(const AutoMeshApplyEnvironment&) = delete;
    AutoMeshApplyEnvironment(AutoMeshApplyEnvironment&&) = delete;
    AutoMeshApplyEnvironment& operator=(AutoMeshApplyEnvironment&&) = delete;

    void recordApplyError(compat::RetainedStringSlice message);
    compat::RetainedStringSlice currentApplyError();
    void incrementApplied();
    std::size_t appliedCount();
    void ensureActionGroupOpen();
    void closeActionGroup();
};

} // namespace nijigenerate::commands::automesh::dynamic::detail
