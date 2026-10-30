#include "detail/bone_action_scope.hpp"
#include <nijigenerate/core/actionstack.hpp>

namespace nijigenerate::actions::node::detail {
[[noreturn]] void failActionObjectReference();
}

namespace nijigenerate::commands::depth::bone::detail {

void pushDepthBoneRefreshAction(std::shared_ptr<actions::GroupAction> group,
        actions::AsyncActionToken token) {
    if (!group || group->empty()) return;
    if (depthBoneRefreshActionSink) {
        if (!token.valid()) {
            const auto receiver = depthBoneRefreshActionSink;
            if (!receiver) actions::node::detail::failActionObjectReference();
            token = receiver->asyncToken();
        }
        bool accepted = false;
        if (token.acceptsCompletion()) {
            const auto receiver = depthBoneRefreshActionSink;
            if (!receiver) actions::node::detail::failActionObjectReference();
            accepted = receiver->addCompletedAsyncAction(token.generation(), group);
        }
        if (!accepted) group->rollback();
    } else {
        core::actionstack::incActionPush(group);
    }
}

void beginDepthBoneRefreshUndo([[maybe_unused]] std::shared_ptr<actions::AsyncGroupAction> action) {
    ++depthBoneRefreshSuppressed;
}

void endDepthBoneRefreshUndo([[maybe_unused]] std::shared_ptr<actions::AsyncGroupAction> action) {
    --depthBoneRefreshSuppressed;
}

void beginDepthBoneRefreshRedo(std::shared_ptr<actions::AsyncGroupAction> action) {
    depthBoneRefreshActionSink = action;
    if (!action) actions::node::detail::failActionObjectReference();
    if (action->replaysCompletedResults()) ++depthBoneRefreshSuppressed;
}

void endDepthBoneRefreshRedo(std::shared_ptr<actions::AsyncGroupAction> action) {
    if (depthBoneRefreshActionSink == action) depthBoneRefreshActionSink = nullptr;
    if (!action) actions::node::detail::failActionObjectReference();
    if (action->replaysCompletedResults()) --depthBoneRefreshSuppressed;
}

} // namespace nijigenerate::commands::depth::bone::detail

namespace nijigenerate::commands::depth::bone {

void ngBeginDepthBoneRefreshActionSink(std::shared_ptr<actions::AsyncGroupAction> sink) {
    using namespace detail;
    if (!sink) return;
    sink->setLifecycleHandlers(&cancelDepthBoneRefreshForAction,
        &beginDepthBoneRefreshUndo, &endDepthBoneRefreshUndo,
        &beginDepthBoneRefreshRedo, &endDepthBoneRefreshRedo);
    sink->setMergeHandler(&mergeDepthBoneAsyncActions);
    depthBoneRefreshActionSink = sink;
}

void ngEndDepthBoneRefreshActionSink(std::shared_ptr<actions::AsyncGroupAction> sink) {
    if (detail::depthBoneRefreshActionSink == sink) detail::depthBoneRefreshActionSink = nullptr;
}

} // namespace nijigenerate::commands::depth::bone
