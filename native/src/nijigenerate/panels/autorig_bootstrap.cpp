// Actual manager bootstrap from independently authored panels/autorig.d; BSD-2-Clause.
#include "autorig.hpp"
#include "detail/autorig_sessions.hpp"
#include "detail/autorig_dispatch.hpp"
#include <nijigenerate/autorig/deterministic/processor.hpp>
#include <nijigenerate/autorig/deterministic/editor.hpp>
#include <nijigenerate/autorig/deterministic/native.hpp>
#include <nijigenerate/commands/base.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/path.hpp>
#include <nijigenerate/compat/phobos_build_path.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/source_allocator.hpp>
#include <array>
#include <memory>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::panels::autorig {
namespace {
template<class T, class Construct> std::shared_ptr<T> createSourceClass(Construct&& construct) {
    using Allocator = nicxlive::core::d_runtime::SourceAllocator<T>;
    Allocator allocator;
    T* storage = allocator.allocate(1);
    try {
        std::forward<Construct>(construct)(storage);
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    const auto destroy = [](T* value) noexcept {
        std::destroy_at(value);
        Allocator{}.deallocate(value, 1);
    };
    return std::shared_ptr<T>(storage, destroy, allocator);
}
} // namespace

std::shared_ptr<::nijigenerate::autorig::framework::AutoRigSessionManager> ngAutoRigSessionManager() {
    namespace framework = ::nijigenerate::autorig::framework;
    namespace deterministic = ::nijigenerate::autorig::deterministic;
    using MainThreadAction = api::mcp::task::Action;
    if (!detail::sharedSessions) {
        // Publish before installing callbacks: failed initialization is not retried
        // on the next invocation, just as the original module TLS assignment.
        detail::sharedSessions = createSourceClass<framework::AutoRigSessionManager>([](auto* storage) {
            const std::array segments{core::path::ngGetAppConfigPath(),
                compat::RetainedString::fromStaticLiteral("autorig"),
                compat::RetainedString::fromStaticLiteral("runs")};
            compat::RetainedString directory;
            try {
                directory = compat::phobos::ngBuildPath(segments);
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
            std::construct_at(storage, directory);
        });
        detail::sharedSessions->setTaskActionBoundary(framework::AutoRigTaskActionBoundary{
            [](framework::Text taskId, framework::Action execute) {
                (void)taskId;
                detail::runActionOnMainThread(MainThreadAction(framework::Action{[] {
                    core::actionstack::incActionPushGroup();
                }}));
                // A failed push above never installs the scope-exit pop.
                compat::ngRunWithScopeExit([execute] {
                    execute();
                }, [] {
                    detail::runActionOnMainThread(MainThreadAction(framework::Action{[] {
                        core::actionstack::incActionPopGroup();
                    }}));
                });
            }});
        detail::sharedSessions->setEditorDispatcher(framework::AutoRigEditorDispatcher{
            [](framework::Action action) {
                detail::runActionOnMainThread(MainThreadAction(framework::Action{[action] {
                    // Enter the actual command provider's existing permission
                    // TLS boundary; a direct action() would bypass that state.
                    commands::ngRunAutoRigEditorAction(action);
                }}));
            }});
        detail::sharedSessions->registerProcessor(
            createSourceClass<deterministic::processor::AnimeFrontViewRigProcessor>([](auto* storage) {
                std::construct_at(storage,
                    [](framework::Json projection, framework::Json target,
                        std::shared_ptr<framework::AutoRigTaskContext> context) {
                    return deterministic::editor::ngApplyFaceProjection(projection, target, context);
                    }, deterministic::pipeline::RigNativeStage{
                    [](compat::RetainedStringSlice stage, framework::Json state, framework::Json program,
                            compat::ConstSlice<std::uint8_t> model,
                            std::shared_ptr<framework::AutoRigTaskContext> context) {
                        return deterministic::native::ngRigNativeStage(
                            stage, state, program, model, context);
                    }});
            }));
        detail::sharedSessions->registerProcessorAlias(
            compat::RetainedString::fromStaticLiteral("deterministic-rig"),
            compat::RetainedString::fromStaticLiteral("anime-front-view-rig"));
    }
    return detail::sharedSessions;
}

} // namespace nijigenerate::panels::autorig
