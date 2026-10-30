// Independent source/nijigenerate/commands/parameter/base.d complete bodies.
#include "base.hpp"
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/utils/transform.hpp>
#include <nijigenerate/windows/flipconfig.hpp>
#include <param/binding_impl.hpp>
#include <compat/native_access_fault.hpp>
#include <compat/source_allocator.hpp>
#include <resource.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <bit>
#include <cstddef>
#include <type_traits>

namespace nijigenerate::commands::parameter::base {
namespace pa = actions::parameter;
namespace runtime = nicxlive::core::param;
namespace stack = core::actionstack;
namespace flip = windows::flipconfig;
namespace transform = utils::transform;
using ::nicxlive::core::d_runtime::ngRequireObject;
namespace {
template<class Range> decltype(auto) sourceAt(Range& values, std::size_t index, std::size_t line) {
    if (index >= values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(
        index, values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/parameter/base.d"), line);
    return values[index];
}
template<class Range, class T> std::ptrdiff_t countUntil(Range array, T target) {
    using ResourceHandle = std::shared_ptr<::nicxlive::core::Resource>;
    const auto length = std::bit_cast<std::ptrdiff_t>(array.size());
    for (std::ptrdiff_t i = 0; i < length; ++i) {
        if constexpr (std::is_convertible_v<decltype(array[i]), ResourceHandle> &&
            std::is_convertible_v<T, ResourceHandle>) {
            if (::nicxlive::core::ngResourceObjectEquals(array[static_cast<std::size_t>(i)], target)) return i;
        } else if (array[static_cast<std::size_t>(i)] == target) return i;
    }
    return -1;
}

// The original unused nested template returns D long, even on a 32-bit target.
template<class Range, class T> std::int64_t findIndex(Range array, T target) {
    return countUntil(array, target);
}
template<class Range> Range removeAt(Range values, std::size_t index) {
    // Phobos stable remove first pops index elements, then one more element.
    // Those operations assert before slicing; neither is an indexed array read.
    if (index >= values.size()) {
#ifndef NDEBUG
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
            compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#else
        if (index > values.size())
            throw compat::ngStaticError<compat::ArraySliceError>(index, values.size(), values.size(),
                compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
        throw compat::ngStaticError<compat::ArraySliceError>(1, 0, 0,
            compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
#endif
    }
    for (std::size_t i = index; i + 1 < values.size(); ++i) values[i] = values[i + 1];
    return values.slice(0, values.size() - 1);
}
}
void mirrorAll(ParameterRef param, std::uint32_t axis) {
    auto action = pa::ngCreateParameterBindingsAction<pa::ParameterChangeBindingsAction>(
        compat::RetainedString::fromStaticLiteral("Mirror All"), param, pa::Bindings{});
    const auto bindings = ngRequireObject(param)->bindings;
    for (auto value : bindings) {
        const auto nx = static_cast<std::uint32_t>(ngRequireObject(param)->axisPointCount(0));
        const auto ny = static_cast<std::uint32_t>(ngRequireObject(param)->axisPointCount(1));
        for (std::uint32_t x = 0; x < nx; ++x) for (std::uint32_t y = 0; y < ny; ++y)
            if (ngRequireObject(value)->isSet({x, y}))
                ngRequireObject(value)->scaleValueAt({x, y}, std::bit_cast<std::int32_t>(axis), -1);
    }
    action->updateNewState(); stack::incActionPush(action);
}
void mirroredAutofill(ParameterRef param, std::uint32_t axis, float min, float max) {
    stack::incActionPushGroup();
    auto action = pa::ngCreateParameterBindingsAction<pa::ParameterChangeBindingsAction>(
        compat::RetainedString::fromStaticLiteral("Mirror Auto Fill"), param, pa::Bindings{});
    const auto bindings = ngRequireObject(param)->bindings;
    for (auto value : bindings) {
        if (auto node = ngRequireObject(value)->getTarget().target.lock()) {
            const auto pair = flip::ngGetFlipPairFor(node);
            const auto target = transform::ngBindingGetPairFor(param, node, pair,
                compat::RetainedString(ngRequireObject(value)->getTarget().name), true);
            if (!target) continue;
            const auto nx = static_cast<std::uint32_t>(ngRequireObject(param)->axisPointCount(0));
            const auto ny = static_cast<std::uint32_t>(ngRequireObject(param)->axisPointCount(1));
            for (std::uint32_t x = 0; x < nx; ++x) {
                const float offX = sourceAt(ngRequireObject(param)->axisPoints[0], x, 62);
                if (axis == 0 && (offX < min || offX > max)) continue;
                for (std::uint32_t y = 0; y < ny; ++y) {
                    const float offY = sourceAt(ngRequireObject(param)->axisPoints[1], y, 65);
                    if (axis == 1 && (offY < min || offY > max)) continue;
                    if (!target->isSet({x, y}))
                        transform::ngBindingAutoFlip(target, value, {x, y}, std::bit_cast<std::int32_t>(axis));
                }
            }
        }
    }
    action->updateNewState(); stack::incActionPush(action); stack::incActionPopGroup();
}
template<bool pushAction> void pasteParameter(ParameterRef param, ParameterRef source, std::uint32_t axis) {
    if (!source) source = binding::cClipboardParameter;
    if (!source) return;
    std::shared_ptr<pa::ParameterChangeBindingsAction> action;
    if constexpr (pushAction) {
        stack::incActionPushGroup();
        action = pa::ngCreateParameterBindingsAction<pa::ParameterChangeBindingsAction>(
            compat::RetainedString::fromStaticLiteral("Paste"), param, pa::Bindings{});
    }
    const auto bindings = source->bindings;
    for (auto value : bindings) {
        if (auto node = ngRequireObject(value)->getTarget().target.lock()) {
            std::shared_ptr<flip::FlipPair> pair;
            if (axis != 2) pair = flip::ngGetFlipPairFor(node);
            const auto target = transform::ngBindingGetPairFor(param, node, pair,
                compat::RetainedString(ngRequireObject(value)->getTarget().name), true);
            if (!target) continue;
            const auto nx = static_cast<std::uint32_t>(ngRequireObject(param)->axisPointCount(0));
            const auto ny = static_cast<std::uint32_t>(ngRequireObject(param)->axisPointCount(1));
            for (std::uint32_t x = 0; x < nx; ++x) for (std::uint32_t y = 0; y < ny; ++y)
                transform::ngBindingAutoFlip(target, value, {x, y}, std::bit_cast<std::int32_t>(axis), false);
        }
        // The source intentionally has no non-Node paste implementation.
    }
    if constexpr (pushAction) {
        action->updateNewState(); stack::incActionPush(action); stack::incActionPopGroup();
    }
    if (::nicxlive::core::ngResourceObjectEquals(source, binding::cClipboardParameter))
        binding::cClipboardParameter.reset();
}
template void pasteParameter<true>(ParameterRef, ParameterRef, std::uint32_t);
template void pasteParameter<false>(ParameterRef, ParameterRef, std::uint32_t);
void convertTo2D(ParameterRef param) {
    auto action = std::allocate_shared<actions::GroupAction>(
        ::nicxlive::core::d_runtime::SourceAllocator<actions::GroupAction>{});
    auto replacement = ext::ExParameter::createSourceDeferred(
        [&] { return ngRequireObject(param)->getName(); }, [] { return true; });
    replacement->setUUID(ngRequireObject(param)->getUUID());
    replacement->min = {ngRequireObject(param)->min.x, ngRequireObject(param)->min.x};
    replacement->max = {ngRequireObject(param)->max.x, ngRequireObject(param)->max.x};
    const auto points = ngRequireObject(param)->axisPoints[0];
    for (const float key : points) {
        if (key != 0 && key != 1) replacement->insertAxisPoint(0, key);
        const auto bindings = ngRequireObject(param)->bindings;
        for (const auto value : bindings) {
            const auto result = [&] {
                const auto target = ngRequireObject(value)->getTarget().getResource();
                const auto name = ngRequireObject(value)->getTarget().name;
                return replacement->getOrAddBinding(target, name);
            }();
            const auto sourceIndex = ngRequireObject(param)->findClosestKeypoint(
                ngRequireObject(param)->unmapValue({key, 0}));
            const auto targetIndex = replacement->findClosestKeypoint(replacement->unmapValue({key, replacement->min.y}));
            ngRequireObject(value)->copyKeypointToBinding(sourceIndex, result, targetIndex);
        }
    }
    const auto index = countUntil(ngRequireObject(project::incActivePuppet())->parameters, param);
    if (index >= 0) {
        action->addAction(pa::ParameterRemoveAction::createSourceDeferred(
            [&] { return param; },
            [] { return &ngRequireObject(project::incActivePuppet())->parameters; }));
        action->addAction(pa::ParameterAddAction::createSourceDeferred(
            [&] { return replacement; },
            [] { return &ngRequireObject(project::incActivePuppet())->parameters; }));
        sourceAt(ngRequireObject(project::incActivePuppet())->parameters,
            static_cast<std::size_t>(index), 147) = replacement;
        if (auto previous = std::dynamic_pointer_cast<ext::ExParameter>(param)) {
            auto parent = previous->getParent(); previous->setParent(nullptr); replacement->setParent(parent);
        }
    }
    stack::incActionPush(action);
}
bool removeParameter(ParameterRef param) {
    ParameterGroupRef parent;
    std::ptrdiff_t index = -1;
    const auto parameters = ngRequireObject(project::incActivePuppet())->parameters;
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        const auto value = parameters[i];
        const auto valueId = ngRequireObject(value)->getUUID();
        const auto parameterId = ngRequireObject(param)->getUUID();
        if (valueId == parameterId) {
            index = static_cast<std::ptrdiff_t>(i); break;
        }
        if (auto group = std::dynamic_pointer_cast<ext::ExParameterGroup>(value)) {
            const auto children = group->children;
            for (std::size_t x = 0; x < children.size(); ++x) {
                const auto child = children[x];
                const auto childId = ngRequireObject(child)->getUUID();
                const auto targetId = ngRequireObject(param)->getUUID();
                if (childId == targetId) {
                    index = static_cast<std::ptrdiff_t>(x); parent = group; break;
                }
            }
            if (index >= 0) break;
        }
    }
    if (index < 0) return false;
    if (parent) {
        if (!parent->children.empty()) parent->children = removeAt(parent->children, static_cast<std::size_t>(index));
        else parent->children.resize(0);
    }
    // The original also removes the root parameter at the child index.
    if (ngRequireObject(project::incActivePuppet())->parameters.size() > 1) {
        // Selected D lowering captures an ordinary member destination before its RHS.
        const auto destination = ngRequireObject(project::incActivePuppet());
        const auto values = ngRequireObject(project::incActivePuppet())->parameters;
        destination->parameters = removeAt(values, static_cast<std::size_t>(index));
    } else ngRequireObject(project::incActivePuppet())->parameters.resize(0);
    return true;
}
void incMoveParameter(ParameterRef from, ParameterGroupRef to, std::int32_t index) {
    (void)index;
    auto extended = std::dynamic_pointer_cast<ext::ExParameter>(from);
    nicxlive::core::d_runtime::ngRequireObject(extended)->setParent(to);
}
ParameterGroupRef incCreateParamGroup(std::int32_t index) {
    if (index < 0) index = 0;
    else if (static_cast<std::size_t>(index) > ngRequireObject(project::incActivePuppet())->parameters.size())
        index = std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(ngRequireObject(project::incActivePuppet())->parameters.size()) - 1u);
    (void)index; // Original computes the index but appends through addGroup.
    auto group = ext::ExParameterGroup::createSourceDeferred([] {
        return core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("New Parameter Group")).toNullableString();
    });
    auto puppet = std::dynamic_pointer_cast<ext::ExPuppet>(project::incActivePuppet());
    nicxlive::core::d_runtime::ngRequireObject(puppet)->addGroup(group);
    return group;
}
} // namespace nijigenerate::commands::parameter::base
