#pragma once

#include "bone_refresh_state.hpp"
#include "../bone.hpp"
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <memory>
#include <new>
#include <source_location>
#include <utility>

namespace nijigenerate::commands::depth::bone {

namespace detail {

// Keep the actual object construction and translate only native allocation failure.
// Constructor exceptions pass through; the native diagnostic site
// does not claim to reproduce the original D allocator's call-site metadata.
template<class Object, class... Args>
std::shared_ptr<Object> sourceDepthBoneObject(Args&&... args) {
    std::allocator<Object> allocator;
    Object* storage;
    try {
        storage = allocator.allocate(1);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            std::source_location::current());
    }
    try {
        ::new (static_cast<void*>(storage)) Object(std::forward<Args>(args)...);
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    try {
        return std::shared_ptr<Object>(storage, [](Object* object) noexcept {
            std::destroy_at(object);
            std::allocator<Object>{}.deallocate(object, 1);
        });
    } catch (const std::bad_alloc&) {
        // shared_ptr calls the actual deleter if control-block allocation fails.
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            std::source_location::current());
    }
}

// Real lifecycle producers are defined against actual job/queue state by
// bone_job_control.cpp; registration is not native behavior acceptance.
void cancelDepthBoneRefreshForAction(std::shared_ptr<actions::AsyncGroupAction> action);
bool mergeDepthBoneAsyncActions(std::shared_ptr<actions::AsyncGroupAction> current,
    std::shared_ptr<actions::AsyncGroupAction> incoming);

void pushDepthBoneRefreshAction(std::shared_ptr<actions::GroupAction> group,
    actions::AsyncActionToken token = {});
void beginDepthBoneRefreshUndo(std::shared_ptr<actions::AsyncGroupAction> action);
void endDepthBoneRefreshUndo(std::shared_ptr<actions::AsyncGroupAction> action);
void beginDepthBoneRefreshRedo(std::shared_ptr<actions::AsyncGroupAction> action);
void endDepthBoneRefreshRedo(std::shared_ptr<actions::AsyncGroupAction> action);

// Native callers supply their actual bound closure by value. Do not type-erase
// it into a new heap allocation merely to run it inside this lexical scope.
// Original delegate ABI/environment/null-fault equivalence remains a separate
// provider gate; this source definition does not accept arbitrary substitutes.
template<class Callback> bool runWithDepthBoneRefreshActionSink(
        std::shared_ptr<actions::AsyncGroupAction> sink, Callback callback) {
    auto previous = depthBoneRefreshActionSink;
    depthBoneRefreshActionSink = sink;
    struct RestoreSink {
        std::shared_ptr<actions::AsyncGroupAction> previous;
        ~RestoreSink() { depthBoneRefreshActionSink = previous; }
    } restore{std::move(previous)};
    return callback();
}

} // namespace detail
} // namespace nijigenerate::commands::depth::bone
