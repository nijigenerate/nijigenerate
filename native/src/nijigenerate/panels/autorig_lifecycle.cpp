// Original panels/autorig.d worker lifecycle; BSD-2-Clause.
#include "autorig.hpp"
#include "detail/autorig_worker.hpp"
#include <nijigenerate/api/mcp/task.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/autorig/deterministic/native.hpp>
#include <nijigenerate/utils/crashdump.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::panels::autorig {

void AutoRigPanel::finishWorker() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    if (worker && !compat::ngRequireObject(worker)->isRunning()) {
        compat::ngRequireObject(worker)->join();
        worker.reset();
        activeRun.reset();
        reclaimSessionMemory = true;
        api::mcp::task::ngMcpSetExternalCommandsBlocked(false);
        restoreDriverState();
    }
    if (!worker && reclaimSessionMemory) {
        // The selected native ownership policy releases actual RAII owners.
        // The human no-GC direction excludes a replacement collector call.
        reclaimSessionMemory = false;
    }
}

void AutoRigPanel::startRun(
        std::shared_ptr<::nijigenerate::autorig::workflow::AutoRigWorkflowRun> run,
        compat::RetainedStringSlice stepId, bool force) {
    const auto owner = shared_from_this();
    finishWorker();
    if (worker) return;
    lastError.reset();
    activeRun = run;
    const auto puppet = project::incActivePuppet();
    controlledRoot = puppet ? compat::ngRequireObject(puppet->root)->getUUID() : 0;
    previousDriversEnabled = puppet && puppet->enableDrivers;
    if (puppet) puppet->enableDrivers = false;
    using WorkerAllocator = nicxlive::core::d_runtime::SourceAllocator<detail::AutoRigWorker>;
    WorkerAllocator allocator;
    auto* storage = allocator.allocate(1);
    try {
        std::construct_at(storage, compat::SharedDelegate<void()>(
        [owner, run, stepId, force] {
            utils::crashdump::installNativeCrashDumpThreadHandler();
            try {
                if (!stepId.empty()) compat::ngRequireObject(run)->executeStep(stepId.retainedString(), true);
                else compat::ngRequireObject(run)->execute(force);
            } catch (const compat::Throwable& error) {
                const std::lock_guard lock(owner->stateMonitor);
                owner->lastError = error.msg;
            }
        }));
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    worker = std::shared_ptr<detail::AutoRigWorker>(storage, [](auto* value) noexcept {
        std::destroy_at(value);
        WorkerAllocator{}.deallocate(value, 1);
    }, allocator);
    api::mcp::task::ngMcpSetExternalCommandsBlocked(true);
    try {
        compat::ngRequireObject(worker)->start();
    } catch (const compat::Throwable&) {
        api::mcp::task::ngMcpSetExternalCommandsBlocked(false);
        worker.reset();
        activeRun.reset();
        restoreDriverState();
        throw;
    }
}

void AutoRigPanel::stop() {
    [[maybe_unused]] const auto owner = weak_from_this().lock();
    if (worker) {
        if (activeRun) compat::ngRequireObject(activeRun)->cancel();
        while (compat::ngRequireObject(worker)->isRunning()) {
            api::mcp::task::ngMcpProcessQueue();
            detail::AutoRigWorker::sleepOneMillisecond();
        }
        compat::ngRequireObject(worker)->join();
    }
    worker.reset();
    activeRun.reset();
    api::mcp::task::ngMcpSetExternalCommandsBlocked(false);
    restoreDriverState();
    if (workflows) compat::ngRequireObject(workflows)->disposeAll();
    ::nijigenerate::autorig::deterministic::native::ngRigForgetNativeSession();
    runs = {};
    inputDrafts = {};
    inputObserved = {};
    inputTextCache = {};
    contextDrafts = {};
    materialDrafts = {};
    reviewDrafts = {};
    workflowOptionsDrafts = {};
}

} // namespace nijigenerate::panels::autorig
