// Stable remove: independent Phobos std.algorithm.mutation/std.range.primitives.
// Copyright Andrei Alexandrescu 2008- and Phobos contributors. BSL-1.0;
// full notice/license: port/licenses/phobos-selection-remove.txt.
#include <nijigenerate/ext/param.hpp>
#include <nijigenerate/ext/package.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <algorithm>
#include <array>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>

namespace nijigenerate::ext {
namespace {
// The actual callers use only passive string, array and cache backing storage.
template<class Operation> decltype(auto) allocateParameterMemory(Operation&& operation) {
    try { return std::forward<Operation>(operation)(); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
// Source ClassInfo is available without first constructing an unrelated ExPuppet.
[[maybe_unused]] const bool sourceParameterClassNamesRegistered = [] {
    using nicxlive::core::d_runtime::ngRegisterObjectClassName;
    allocateParameterMemory([] {
        ngRegisterObjectClassName(typeid(ExParameterGroup), compat::RetainedString::fromStaticLiteral(
            "nijigenerate.ext.param.ExParameterGroup"));
    });
    allocateParameterMemory([] {
        ngRegisterObjectClassName(typeid(ExParameter), compat::RetainedString::fromStaticLiteral(
            "nijigenerate.ext.param.ExParameter"));
    });
    return true;
}();
}
void ExParameterGroup::serializeSelf(serde::TypedJsonWriter& serializer) const {
    [[maybe_unused]] const auto receiverLifetime = shared_from_this();
    serializer.putKey("groupUUID");
    serializer.putValue(getUUID());
    serializer.putKey("name");
    const auto label = getName();
    serializer.putValue(label);
    serializer.putKey("color");
    serializer.serializeValue(std::array<float, 3>{color.x, color.y, color.z});
}

void ExParameterGroup::serializeSelf(serde::InochiSerializer& serializer) const {
    [[maybe_unused]] const auto receiverLifetime = shared_from_this();
    auto& data = serializer.root;
    serializer.putKey("groupUUID");
    serializer.putValue(getUUID());
    serializer.putKey("name");
    serializer.putValue(getName());
    serde::Fghj colorData;
    for (float value : {color.x, color.y, color.z}) {
        serde::Fghj item;
        item.put_value(value);
        colorData.push_back({"", item});
    }
    data.put_child("color", colorData);
    // The D serializer deliberately has no children field.
}

serde::ReadError ExParameterGroup::deserializeFromFghj(serde::TypedJsonView data) {
    [[maybe_unused]] const auto receiverLifetime = shared_from_this();
    (void)serde::deserializeValue(data.findFirst("groupUUID"), uuid);
    const auto label = data.findFirst("name");
    if (!label.isMissing()) (void)serde::deserializeValue(label, name);
    const auto inputColor = data.findFirst("color");
    if (!inputColor.isMissing() && inputColor.kind() == serde::JsonKind::Array) {
        std::array<float, 3> components{color.x, color.y, color.z};
        auto values = inputColor.byElement();
        try {
            for (std::size_t i = 0; !values.empty() && i < components.size(); ++i) {
                if (serde::deserializeValue(values.front(), components[i])) break;
                values.popFront();
            }
        } catch (...) { color = {components[0], components[1], components[2]}; throw; }
        color = {components[0], components[1], components[2]};
    }
    const auto entries = data.findFirst("children");
    if (!entries.isMissing() && entries.kind() == serde::JsonKind::Array)
        for (const auto child : entries.byElement())
            children.append(nicxlive::core::param::inParameterCreate(child));
    return {};
}

serde::SerdeException ExParameterGroup::deserializeFromFghj(const serde::Fghj& data) {
    [[maybe_unused]] const auto receiverLifetime = shared_from_this();
    uuid = data.get<std::uint32_t>("groupUUID");
    if (auto value = data.get_optional<std::string>("name")) name = *value;
    if (auto value = data.get_child_optional("color")) {
        if (value->size() != 3) throw std::runtime_error("Expected three color components");
        auto it = value->begin();
        color.x = (it++)->second.get_value<float>();
        color.y = (it++)->second.get_value<float>();
        color.z = it->second.get_value<float>();
    }
    if (auto value = data.get_child_optional("children")) {
        for (const auto& child : *value) children.append(nicxlive::core::param::inParameterCreate(child.second));
    }
    return std::nullopt;
}

void ExParameterGroup::reconstruct(const std::shared_ptr<Puppet>& basePuppet) {
    const auto receiverLifetime = shared_from_this();
    const auto originalPuppet = basePuppet;
    if (auto puppet = std::dynamic_pointer_cast<ExPuppet>(originalPuppet)) {
        const auto self = std::static_pointer_cast<ExParameterGroup>(receiverLifetime);
        const auto traversal = children;
        for (auto child : traversal) {
            if (auto parameter = std::dynamic_pointer_cast<ExParameter>(child)) {
                parameter->parent = self;
                parameter->parentUUID = uuid;
            }
            // Preserve the source's group-name lookup, not the child's name.
            if (!puppet->findParameter(getName())) {
                auto& parameters = puppet->parameters;
                allocateParameterMemory([&] { parameters.push_back(child); });
            }
        }
        if (puppet->findParameter(getUUID())) {
            puppet->removeParameter(self);
            puppet->addGroup(self);
        }
    }
    Parameter::reconstruct(originalPuppet);
}

serde::ReadError ExParameter::deserializeFromFghj(serde::TypedJsonView data) {
    [[maybe_unused]] const auto receiverLifetime = shared_from_this();
    const auto owner = data.findFirst("parentUUID");
    if (!owner.isMissing()) (void)serde::deserializeValue(owner, parentUUID);
    return Parameter::deserializeFromFghj(data);
}

serde::SerdeException ExParameter::deserializeFromFghj(const serde::Fghj& data) {
    [[maybe_unused]] const auto receiverLifetime = shared_from_this();
    if (auto value = data.get_optional<std::uint32_t>("parentUUID")) parentUUID = *value;
    return Parameter::deserializeFromFghj(data);
}

void ExParameter::serializeSelf(serde::InochiSerializer& serializer) const {
    [[maybe_unused]] const auto receiverLifetime = shared_from_this();
    if (parent) {
        serializer.putKey("parentUUID");
        const auto owner = compat::ngRequireObject(parent);
        serializer.putValue(owner->getUUID());
    }
    Parameter::serializeSelf(serializer);
}

void ExParameter::serializeSelf(serde::TypedJsonWriter& serializer) const {
    [[maybe_unused]] const auto receiverLifetime = shared_from_this();
    if (parent) {
        serializer.putKey("parentUUID");
        const auto owner = compat::ngRequireObject(parent);
        serializer.putValue(owner->getUUID());
    }
    Parameter::serializeSelf(serializer);
}

void ExParameterGroup::serializeSelf(serde::TypedJsonWriter& serializer) {
    const auto receiverLifetime = shared_from_this();
    serializer.putKey("groupUUID");
    serializer.putValue(getUUID());
    serializer.putKey("name");
    const auto label = getName();
    serializer.putValue(label);
    serializer.putKey("color");
    serializer.serializeValue(std::array<float, 3>{color.x, color.y, color.z});
}

void ExParameter::serializeSelf(serde::TypedJsonWriter& serializer) {
    const auto receiverLifetime = shared_from_this();
    if (parent) {
        serializer.putKey("parentUUID");
        const auto owner = compat::ngRequireObject(parent);
        serializer.putValue(owner->getUUID());
    }
    Parameter::serializeSelf(serializer);
}

void ExParameter::setParent(std::shared_ptr<ExParameterGroup> newParent) {
    const auto receiverLifetime = shared_from_this();
    [[maybe_unused]] ParameterPtr removedLifetime;
    if (parent && !nicxlive::core::ngResourceObjectEquals(parent, newParent)) {
        const auto searchParent = compat::ngRequireObject(parent);
        const auto search = searchParent->children;
        for (std::size_t i = 0; i < search.size(); ++i) {
            const auto candidate = search[i];
            if (!nicxlive::core::ngResourceObjectEquals(candidate, receiverLifetime)) continue;
            // countUntil's callback can change the public current parent and
            // its array. Remove from the descriptor evaluated after the search.
            const auto removalParent = compat::ngRequireObject(parent);
            const auto range = removalParent->children;
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
            removedLifetime = source[0];
            source = source.slice(1, source.size());
            const auto shortened = range.slice(0, range.size() - 1);
            for (std::size_t j = 0; j < source.size(); ++j) destination[j] = source[j];
            removalParent->children = shortened;
            break;
        }
    }
    const auto oldParent = parent;
    parent = newParent;
    if (parent) {
        const auto uuidReceiver = compat::ngRequireObject(parent);
        parentUUID = uuidReceiver->getUUID();
        if (!nicxlive::core::ngResourceObjectEquals(oldParent, parent)) {
            const auto appendReceiver = compat::ngRequireObject(parent);
            appendReceiver->children.append(receiverLifetime);
        }
    } else parentUUID = std::numeric_limits<std::uint32_t>::max();
}

void ExParameter::finalize(const std::shared_ptr<Puppet>& basePuppet) {
    [[maybe_unused]] const auto receiverLifetime = shared_from_this();
    const auto originalPuppet = basePuppet;
    if (auto puppet = std::dynamic_pointer_cast<ExPuppet>(originalPuppet);
        puppet && !parent && parentUUID != std::numeric_limits<std::uint32_t>::max()) {
        setParent(puppet->findGroup(parentUUID));
    }
    Parameter::finalize(originalPuppet);
}

ParameterPtr ExParameter::dup() {
    [[maybe_unused]] const auto receiverLifetime = shared_from_this();
    auto result = ExParameter::createSourceDeferred([&] {
        const auto originalName = getName();
        return allocateParameterMemory([&] {
            std::string copiedName(originalName.view());
            copiedName += " (Copy)";
            return nicxlive::core::common::NullableString(std::move(copiedName));
        });
    }, [&] { return isVec2; });
    result->min = min;
    result->max = max;
    {
        const auto copiedAxes = duplicateSourceAxisDescriptors();
        result->axisPoints[0] = copiedAxes[0];
        result->axisPoints[1] = copiedAxes[1];
    }
    const auto traversal = bindings;
    for (std::size_t i = 0; i < traversal.size(); ++i) {
        const auto binding = traversal[i];
        const auto target = compat::ngRequireObject(binding)->getTarget().getResource();
        const auto key = compat::ngRequireObject(binding)->getTarget().name;
        auto copied = result->createBinding(target, key, false);
        const auto mode = compat::ngRequireObject(binding)->interpolateMode();
        compat::ngRequireObject(copied)->setInterpolateMode(mode);
        const auto xCount = axisPointCount(0);
        for (std::uint32_t x = 0; x < xCount; ++x) {
            const auto yCount = axisPointCount(1);
            for (std::uint32_t y = 0; y < yCount; ++y) {
                compat::ngRequireObject(binding)->copyKeypointToBinding({x, y}, copied, {x, y});
            }
        }
        result->addBinding(copied);
    }
    return result;
}

void incRegisterExParameter() {
    nicxlive::core::param::inParameterSetTypedFactory([](serde::TypedJsonView data) -> ParameterPtr {
        ParameterPtr result;
        if (!data.findFirst("groupUUID").isMissing())
            result = std::allocate_shared<ExParameterGroup>(
                nicxlive::core::d_runtime::SourceAllocator<ExParameterGroup>{});
        else result = std::allocate_shared<ExParameter>(
            nicxlive::core::d_runtime::SourceAllocator<ExParameter>{});
        (void)result->deserializeFromFghj(data);
        return result;
    });
    nicxlive::core::param::inParameterSetFactory([](const serde::Fghj& data) -> ParameterPtr {
        ParameterPtr result;
        if (data.get_child_optional("groupUUID"))
            result = std::allocate_shared<ExParameterGroup>(
                nicxlive::core::d_runtime::SourceAllocator<ExParameterGroup>{});
        else result = std::allocate_shared<ExParameter>(
            nicxlive::core::d_runtime::SourceAllocator<ExParameter>{});
        (void)result->deserializeFromFghj(data);
        return result;
    });
}
} // namespace nijigenerate::ext
