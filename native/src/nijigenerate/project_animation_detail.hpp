#pragma once

#include <animation/animation.hpp>
#include <memory>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include <nijigenerate/core/animation/player_runtime.hpp>

namespace nijigenerate::project::detail {

// Module-private project.d helpers shared by its production translation units.
using Animation = nicxlive::core::Animation;
using AnimationLane = nicxlive::core::AnimationLane;
using AnimationLanes = nicxlive::core::common::SharedArray<AnimationLane>;

AnimationLanes ngCloneAnimationLanes(AnimationLanes lanes);
Animation ngCloneAnimation(Animation animation);
void ngRestoreAnimationLanes(Animation* animation, AnimationLanes lanes);
void installAnimationPlayer(std::shared_ptr<core::animation::AnimationPlayer> player);

template<class Operation> auto ngAllocateProjectStorage(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
inline compat::RetainedString ngDuplicateProjectText(std::string_view text) {
    if (text.empty()) return {};
    return ngAllocateProjectStorage([&] { return compat::RetainedString(text); });
}
inline compat::RetainedString ngRetainProjectText(std::string text) {
    return ngAllocateProjectStorage([&] { return compat::RetainedString(std::move(text)); });
}
template<class Action, class Producer> std::shared_ptr<Action> ngCreateProjectAction(Producer&& producer) {
    auto action = std::allocate_shared<Action>(nicxlive::core::d_runtime::SourceAllocator<Action>{});
    // Real Source arguments and initialization execute after the same object's allocation.
    auto arguments = std::forward<Producer>(producer)();
    std::apply([&](auto&&... values) {
        action->initialize(std::forward<decltype(values)>(values)...);
    }, std::move(arguments));
    return action;
}

} // namespace nijigenerate::project::detail
