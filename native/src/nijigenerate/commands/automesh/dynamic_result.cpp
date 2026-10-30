#include "detail/dynamic_result.hpp"
#include "detail/dynamic_sync.hpp"
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/translation.hpp>
#include <compat/source_allocator.hpp>

namespace nijigenerate::commands::automesh::dynamic::detail {
void AutoMeshApplyResult::prepareSourceClassInfo() {
    CommandResult::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshApplyResult>();
}
AutoMeshApplyResult::AutoMeshApplyResult()
    : CommandResult(true,core::translation::ngTranslateRetained(
          compat::RetainedString::fromStaticLiteral("AutoMesh queued"))) {
    lock = std::allocate_shared<nicxlive::core::d_runtime::SourceMutex>(
        nicxlive::core::d_runtime::SourceAllocator<nicxlive::core::d_runtime::SourceMutex>{});
    condition = std::allocate_shared<nicxlive::core::d_runtime::SourceCondition>(
        nicxlive::core::d_runtime::SourceAllocator<nicxlive::core::d_runtime::SourceCondition>{}, lock);
}
void AutoMeshApplyResult::complete(std::shared_ptr<CommandResult> result) {
    synchronizedMonitor(lock, [&] {
        if(done) return;
        finalResult=result;
        if(result) { succeeded=result->succeeded; message=result->message; }
        done=true;
        compat::ngRequireObject(condition)->notify();
    });
}
std::shared_ptr<CommandResult> AutoMeshApplyResult::waitForCompletion() {
    auto owner=shared_from_this();
    if(core::tasks::TaskFiber::isVibeTask()) {
        for(;;) {
            bool completed = false;
            synchronizedMonitor(lock, [&] { completed = done; });
            if(completed) break;
            core::tasks::TaskFiber::yield();
        }
    } else {
        synchronizedMonitor(lock, [&] {
            while(!done) compat::ngRequireObject(condition)->wait();
        });
    }
    return finalResult?finalResult:owner;
}
}
