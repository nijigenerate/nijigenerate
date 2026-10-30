#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/node_class_name.hpp>
#include <resource.hpp>
#include <compat/source_allocator.hpp>
#include <algorithm>
#include <cstddef>
#include <new>

namespace nijigenerate::ext {
std::shared_ptr<ExPuppet> ExPuppet::create() {
    compat::ngRegisterResourceClassNames();
    using Allocator = nicxlive::core::d_runtime::SourceAllocator<ExPuppet>;
    Allocator allocator;
    auto* storage = allocator.allocate(1);
    try {
        ::new (static_cast<void*>(storage)) ExPuppet(DeferredConstruction{});
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    auto puppet = std::shared_ptr<ExPuppet>(storage, [](ExPuppet* object) noexcept {
        std::destroy_at(object);
        Allocator{}.deallocate(object, 1);
    }, allocator);
    puppet->constructOwned();
    return puppet;
}
std::shared_ptr<ExPuppet> ExPuppet::create(std::shared_ptr<nicxlive::core::nodes::Node> root) {
    compat::ngRegisterResourceClassNames();
    using Allocator = nicxlive::core::d_runtime::SourceAllocator<ExPuppet>;
    Allocator allocator;
    auto* storage = allocator.allocate(1);
    try {
        ::new (static_cast<void*>(storage)) ExPuppet(DeferredConstruction{});
    } catch (...) {
        allocator.deallocate(storage, 1);
        throw;
    }
    auto puppet = std::shared_ptr<ExPuppet>(storage, [](ExPuppet* object) noexcept {
        std::destroy_at(object);
        Allocator{}.deallocate(object, 1);
    }, allocator);
    puppet->constructOwned(std::move(root));
    return puppet;
}
ParameterPtr ExPuppet::findParameter(std::uint32_t uuid) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto traversal = parameters;
    for (const auto& parameter : traversal) {
        const auto receiver = parameter;
        if (!receiver) actions::node::detail::failActionObjectReference();
        if (receiver->getUUID() == uuid) return parameter;
    }
    return nullptr;
}
ParameterPtr ExPuppet::findParameter(compat::RetainedString name) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto traversal = parameters;
    for (const auto& parameter : traversal) {
        const auto receiver = parameter;
        if (!receiver) actions::node::detail::failActionObjectReference();
        if (receiver->getName().view() == name.view()) return parameter;
    }
    return nullptr;
}
bool ExPuppet::getIsNodeBound(const std::shared_ptr<nicxlive::core::nodes::Node>& node) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const std::shared_ptr<nicxlive::core::Resource> target = node;
    const auto traversal = parameters;
    const auto count = traversal.size();
    for (std::size_t index = 0; index < count; ++index) {
        const auto parameter = traversal[index];
        if (auto group = std::dynamic_pointer_cast<ExParameterGroup>(parameter)) {
            const auto children = group->children;
            const auto childCount = children.size();
            for (std::size_t childIndex = 0; childIndex < childCount; ++childIndex) {
                const auto child = children[childIndex];
                if (!child) actions::node::detail::failActionObjectReference();
                if (child->hasAnyBinding(target)) return true;
            }
        } else {
            if (!parameter) actions::node::detail::failActionObjectReference();
            if (parameter->hasAnyBinding(target)) return true;
        }
    }
    return false;
}
void ExPuppet::removeParameter(const ParameterPtr& parameter) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    // D passes a handle value; an argument borrowing a shifted slot must stay stable.
    const auto removed = parameter;
    const auto rootSearch = parameters;
    for (std::size_t i = 0; i < rootSearch.size(); ++i) {
        if (!nicxlive::core::ngResourceObjectEquals(rootSearch[i], removed)) continue;
        const auto range = parameters;
#ifndef NDEBUG
        if (i > range.size()) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
            compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#endif
        if (i > range.size()) throw compat::ngStaticError<compat::ArraySliceError>(
            i, range.size(), range.size(),
            compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
        auto source = range.slice(i, range.size());
        auto destination = source;
#ifndef NDEBUG
        if (source.empty()) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
            compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#endif
        if (source.empty()) throw compat::ngStaticError<compat::ArraySliceError>(
            1, 0, 0, compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
        source = source.slice(1, source.size());
        const auto result = range.slice(0, range.size() - 1);
        for (std::size_t j = 0; j < source.size(); ++j) destination[j] = source[j];
        parameters = result;
        if (auto extended = std::dynamic_pointer_cast<ExParameter>(removed)) extended->setParent(nullptr);
        return;
    }
    const auto groupSearch = parameters;
    for (const auto& candidate : groupSearch) {
        if (auto group = std::dynamic_pointer_cast<ExParameterGroup>(candidate)) {
            const auto childSearch = group->children;
            for (std::size_t i = 0; i < childSearch.size(); ++i) {
                if (!nicxlive::core::ngResourceObjectEquals(childSearch[i], removed)) continue;
                const auto range = group->children;
#ifndef NDEBUG
                if (i > range.size()) throw compat::ngStaticError<compat::AssertError>(
                    compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
                    compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#endif
                if (i > range.size()) throw compat::ngStaticError<compat::ArraySliceError>(
                    i, range.size(), range.size(),
                    compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
                auto source = range.slice(i, range.size());
                auto destination = source;
#ifndef NDEBUG
                if (source.empty()) throw compat::ngStaticError<compat::AssertError>(
                    compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
                    compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#endif
                if (source.empty()) throw compat::ngStaticError<compat::ArraySliceError>(
                    1, 0, 0, compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
                source = source.slice(1, source.size());
                const auto result = range.slice(0, range.size() - 1);
                for (std::size_t j = 0; j < source.size(); ++j) destination[j] = source[j];
                group->children = result;
                return;
            }
        }
    }
}
std::shared_ptr<ExParameterGroup> ExPuppet::findGroup(std::uint32_t uuid) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto traversal = groups;
    for (const auto& group : traversal) {
        const auto receiver = group;
        if (!receiver) actions::node::detail::failActionObjectReference();
        if (receiver->getUUID() == uuid) return group;
    }
    return nullptr;
}
std::shared_ptr<ExParameterGroup> ExPuppet::findGroup(compat::RetainedString name) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto traversal = groups;
    for (const auto& group : traversal) {
        const auto receiver = group;
        if (!receiver) actions::node::detail::failActionObjectReference();
        if (receiver->getName().view() == name.view()) return group;
    }
    return nullptr;
}
void ExPuppet::addGroup(std::shared_ptr<ExParameterGroup> group) { groups.append(std::move(group)); }
void ExPuppet::removeGroup(std::shared_ptr<ExParameterGroup> group) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto search = groups;
    for (std::size_t i = 0; i < search.size(); ++i) {
        if (!nicxlive::core::ngResourceObjectEquals(search[i], group)) continue;
        const auto range = groups;
#ifndef NDEBUG
        if (i > range.size()) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
            compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#endif
        if (i > range.size()) throw compat::ngStaticError<compat::ArraySliceError>(
            i, range.size(), range.size(),
            compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
        auto source = range.slice(i, range.size());
        auto destination = source;
#ifndef NDEBUG
        if (source.empty()) throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("range is smaller than amount of items to pop"),
            compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2137);
#endif
        if (source.empty()) throw compat::ngStaticError<compat::ArraySliceError>(
            1, 0, 0, compat::RetainedString::fromStaticLiteral("std/range/primitives.d"), 2140);
        source = source.slice(1, source.size());
        const auto result = range.slice(0, range.size() - 1);
        for (std::size_t j = 0; j < source.size(); ++j) destination[j] = source[j];
        groups = result;
        return;
    }
}
serde::SerdeException ExPuppet::deserializeFromFghj(const serde::Fghj& data) {
    if (auto value = data.get_child_optional("groups")) {
        for (const auto& child : *value) {
            groups.append(std::dynamic_pointer_cast<ExParameterGroup>(
                nicxlive::core::param::inParameterCreate(child.second)));
        }
    }
    if (auto value = data.get_optional<std::string>("depthDrawSessionManifest"))
        depthDrawSessionManifestJson = compat::RetainedString(*value);
    Puppet::deserializeFromFghj(data);
    return std::nullopt;
}
void ExPuppet::reconstruct() {
    const auto self = shared_from_this();
    Puppet::reconstruct();
    const auto snapshot = groups.duplicate();
    for (auto group : snapshot) compat::ngRequireObject(group)->reconstruct(self);
}
void ExPuppet::finalize() {
    const auto receiverLifetime = weak_from_this().lock();
    Puppet::finalize();
    const auto self = shared_from_this();
    // D foreach captures the array descriptor/length, but observes shared slots.
    const auto traversal = groups;
    const auto count = traversal.size();
    for (std::size_t index = 0; index < count; ++index) {
        // Retain the receiver if its callback replaces the current owner slot.
        const auto group = traversal.get(index);
        compat::ngRequireObject(group)->finalize(self);
    }
}
} // namespace nijigenerate::ext
