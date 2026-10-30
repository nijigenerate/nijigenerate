#include <nijigenerate/commands/binding/base.hpp>
#include <nijigenerate/commands/parameter/base.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/node_class_name.hpp>
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/core/shortcut/base.hpp>
#include "resource_sort.hpp"
#include <nijigenerate/project.hpp>
#include <algorithm>
#include <new>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace nijigenerate::commands::binding {
namespace {
template<class T> compat::Slice<T> makeBindingArray(std::size_t length,
        std::source_location source = std::source_location::current()) {
    if (!length) return {};
    try {
        return compat::Slice<T>(std::vector<T>(length));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}
}
void ngInitBindingProviders() {
    compat::ngRegisterResourceClassNames();
    core::shortcut::ngSetParamPointProvider([] { return cParamPoint; });
    core::shortcut::ngSetSelectedBindingsProvider([] {
        const auto selected = cSelectedBindings;
        auto values = makeBindingArray<std::shared_ptr<ParameterBinding>>(selected.size());
        std::size_t index = 0;
        for (const auto& [target, binding] : selected) values[index++] = binding;
        return values;
    });
}

void refreshBindingList(std::shared_ptr<Parameter> param, bool onlySelected) {
    SelectedBindings newSelected;
    const auto selected = onlySelected ? project::incSelectedNodes()
                                      : compat::Slice<std::shared_ptr<Node>>{};
    cParamBindingEntriesAll.clear();
    const auto bindings = actions::node::detail::requireActionObject(param)->bindings;
    for (auto binding : bindings) {
        const BindingTarget target(actions::node::detail::requireActionObject(binding)->getTarget());
        if (cSelectedBindings.contains(target)) newSelected[target] = binding;
        if (!onlySelected || std::find_if(selected.begin(), selected.end(),
                [resource = target.target.getResource()](const auto& node) {
                    return nicxlive::core::ngResourceObjectEquals(
                        std::static_pointer_cast<nicxlive::core::Resource>(node), resource);
                }) != selected.end())
            cParamBindingEntriesAll[binding->getResourceTarget()].append(binding);
    }
    compat::Slice<ResourceRef> keys;
    const auto entries = cParamBindingEntriesAll;
    if (!entries.empty()) {
        keys = makeBindingArray<ResourceRef>(entries.size());
        std::size_t index = 0;
        for (const auto& [resource, bindings] : entries) keys[index++] = resource;
    }
    auto nodes = keys.empty() ? compat::Slice<ResourceRef>{} : keys.duplicate();
    parameter::cAllBoundNodes = nodes;
    const auto compare = [](ResourceRef x, ResourceRef y) {
        const auto nx = std::dynamic_pointer_cast<Node>(x.getResource());
        const auto ny = std::dynamic_pointer_cast<Node>(y.getResource());
        if (nx && ny) {
            const auto leftName = nx->getName();
            const auto rightName = ny->getName();
            return leftName < rightName;
        }
        if (nx) return true;
        if (ny) return false;
        const auto px = std::dynamic_pointer_cast<Parameter>(x.getResource());
        const auto py = std::dynamic_pointer_cast<Parameter>(y.getResource());
        if (px && py) {
            const auto leftName = px->getName();
            const auto rightName = py->getName();
            return leftName < rightName;
        }
        return false;
    };
    detail::resource_sort::sort(nodes, compare);
    cSelectedBindings = newSelected;
    paramPointChanged(param);
}

void paramPointChanged(std::shared_ptr<Parameter> param) {
    cParamBindingEntries.clear();
    const auto point = actions::node::detail::requireActionObject(param)->findClosestKeypoint();
    cParamPoint = {static_cast<std::uint32_t>(point.x), static_cast<std::uint32_t>(point.y)};
    const auto bindings = param->bindings;
    for (auto binding : bindings)
        if (ngBindingIsSetAt(binding, cParamPoint))
            cParamBindingEntries[binding->getTarget().getResource()].append(binding);
}

compat::Slice<ResourceRef> getCompatibleNodes() {
    std::shared_ptr<Node> thisNode;
    const auto initialSelection = cSelectedBindings;
    for (const auto& entry : initialSelection) {
        const auto binding = entry.second;
        if (auto node = std::dynamic_pointer_cast<Node>(
                actions::node::detail::requireActionObject(binding)->getTarget().getResource())) {
            if (!thisNode) thisNode = node;
            else if (node != thisNode) return {};
        }
    }
    if (!thisNode) return {};
    compat::Slice<ResourceRef> compatible;
    const auto allEntries = cParamBindingEntriesAll;
    for (const auto& entry : allEntries) {
        const auto another = entry.first;
        const auto otherNode = std::dynamic_pointer_cast<Node>(another.getResource());
        if (otherNode == thisNode) continue;
        bool skip = false;
        const auto candidateSelection = cSelectedBindings;
        for (const auto& selectedEntry : candidateSelection) {
            const auto binding = selectedEntry.second;
            // Both branches in the original continue the outer loop. Preserve the call.
            (void)actions::node::detail::requireActionObject(binding)->isCompatibleWithResource(
                std::static_pointer_cast<nicxlive::core::Resource>(otherNode));
            skip = true;
            break;
        }
        if (!skip) compatible.append(ResourceRef(otherNode));
    }
    return compatible;
}

void copySelectionToNode(std::shared_ptr<Parameter> param, std::shared_ptr<Node> target) {
    compat::Slice<BindingTarget> keys;
    const auto keySource = cSelectedBindings;
    if (!keySource.empty()) {
        keys = makeBindingArray<BindingTarget>(keySource.size());
        std::size_t index = 0;
        for (const auto& entry : keySource) keys[index++] = entry.first;
    }
    if (keys.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, 0,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/binding/base.d"), 149);
    const auto src = keys[0].target;
    const auto selection = cSelectedBindings;
    for (const auto& entry : selection) {
        const auto binding = entry.second;
        if (auto node = std::dynamic_pointer_cast<Node>(
                actions::node::detail::requireActionObject(binding)->getTarget().getResource())) {
#ifndef NDEBUG
            if (binding->getTarget().getResource() != src.getResource())
                throw compat::ngStaticError<compat::AssertError>(
                    compat::RetainedString::fromStaticLiteral("selection mismatch"),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/binding/base.d"), 153);
#endif
            auto destination = actions::node::detail::requireActionObject(param)->getOrAddBinding(
                std::static_pointer_cast<nicxlive::core::Resource>(target), binding->getTarget().name);
            binding->copyKeypointToBinding({cParamPoint.x, cParamPoint.y}, destination,
                                          {cParamPoint.x, cParamPoint.y});
        }
    }
    actions::node::detail::requireActionObject(target)->notifyChange(
        target, ::nicxlive::core::nodes::NotifyReason::StructureChanged);
    refreshBindingList(param);
}

void swapSelectionWithNode(std::shared_ptr<Parameter> param, std::shared_ptr<Node> target) {
    compat::Slice<BindingTarget> keys;
    const auto keySource = cSelectedBindings;
    if (!keySource.empty()) {
        keys = makeBindingArray<BindingTarget>(keySource.size());
        std::size_t index = 0;
        for (const auto& entry : keySource) keys[index++] = entry.first;
    }
    if (keys.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, 0,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/binding/base.d"), 165);
    const auto src = keys[0].target;
    const auto selection = cSelectedBindings;
    for (const auto& entry : selection) {
        const auto binding = entry.second;
        if (auto node = std::dynamic_pointer_cast<Node>(
                actions::node::detail::requireActionObject(binding)->getTarget().getResource())) {
#ifndef NDEBUG
            if (binding->getTarget().getResource() != src.getResource())
                throw compat::ngStaticError<compat::AssertError>(
                    compat::RetainedString::fromStaticLiteral("selection mismatch"),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/binding/base.d"), 169);
#endif
            auto destination = actions::node::detail::requireActionObject(param)->getOrAddBinding(
                std::static_pointer_cast<nicxlive::core::Resource>(target), binding->getTarget().name);
            binding->swapKeypointWithBinding({cParamPoint.x, cParamPoint.y}, destination,
                                            {cParamPoint.x, cParamPoint.y});
        }
    }
    refreshBindingList(param);
}
} // namespace nijigenerate::commands::binding
