#include "task.hpp"
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <core/compat/source_mutex.hpp>
#include <core/compat/source_condition.hpp>

#include <mutex>
#include <utility>

namespace nijigenerate::api::mcp::task {
namespace detail {
std::shared_ptr<SourceMutex> ngCreateSourceMutex() {
    return std::allocate_shared<SourceMutex>(nicxlive::core::d_runtime::SourceAllocator<SourceMutex>{});
}
std::shared_ptr<SourceCondition> ngCreateSourceCondition(std::shared_ptr<SourceMutex> mutex) {
    return std::allocate_shared<SourceCondition>(
        nicxlive::core::d_runtime::SourceAllocator<SourceCondition>{},std::move(mutex));
}
void ngLockSourceMonitor(const std::shared_ptr<SourceMutex>& mutex) {
    compat::ngRequireObject(mutex)->lock_nothrow();
}
void ngUnlockSourceMonitor(const std::shared_ptr<SourceMutex>& mutex) {
    compat::ngRequireObject(mutex)->unlock_nothrow();
}
void ngNotifySourceCondition(const std::shared_ptr<SourceCondition>& condition) {
    compat::ngRequireObject(condition)->notify();
}
void ngWaitSourceCondition(const std::shared_ptr<SourceCondition>& condition) {
    compat::ngRequireObject(condition)->wait();
}
}
namespace {
struct EnqueuedCommand { Action action; };
compat::Slice<EnqueuedCommand> queue;
std::shared_ptr<detail::SourceMutex> queueMutex;
std::mutex initializationMutex;
thread_local bool externalCommandsBlocked = false;

std::shared_ptr<detail::SourceMutex> currentMutex() {
    const std::lock_guard lock(initializationMutex);
    return queueMutex;
}
} // namespace

void ngMcpSetExternalCommandsBlocked(bool blocked) {
    externalCommandsBlocked = blocked;
}

bool ngMcpExternalCommandsBlocked() {
    return externalCommandsBlocked;
}

void ngMcpInitTask() {
    const std::lock_guard lock(initializationMutex);
    if (!queueMutex) queueMutex = detail::ngCreateSourceMutex();
}

void ngMcpEnqueueAction(Action action) {
    auto mutex = currentMutex();
    if (!mutex) {
        ngMcpInitTask();
        mutex = currentMutex();
    }
    detail::withSourceMonitor(mutex, [&] {
        queue.append(EnqueuedCommand{std::move(action)});
    });
}

void ngMcpProcessQueue() {
    auto mutex = currentMutex();
    if (!mutex) return;
    compat::Slice<EnqueuedCommand> items;
    detail::withSourceMonitor(mutex, [&] {
        if (queue.empty()) return;
        items = queue;
        queue.clear();
    });
    // Detached work runs without the queue lock, including reentrant dispatch.
    for (auto item : items) {
        if (item.action) item.action();
    }
}

} // namespace nijigenerate::api::mcp::task
