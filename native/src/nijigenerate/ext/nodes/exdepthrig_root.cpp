#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/detail/plain_array_remove.hpp>
#include <compat/object_metadata.hpp>
#include <string>

namespace nijigenerate::ext::nodes {

ExDepthRigRoot::ExDepthRigRoot(FieldInitializationOnly) : Node() {}

std::shared_ptr<ExDepthRigRoot> ExDepthRigRoot::create(std::shared_ptr<Node> parent) {
    return createSourceDeferred([&] { return parent; });
}

nicxlive::core::common::NullableString ExDepthRigRoot::typeId() const {
    return nicxlive::core::common::NullableString::fromStaticLiteral("DepthRigRoot");
}

compat::Slice<std::shared_ptr<ExDepthBone>> ExDepthRigRoot::depthBones() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    compat::Slice<std::shared_ptr<ExDepthBone>> result;
    const auto visit = [&result](auto&& self, std::shared_ptr<Node> node) -> void {
        if (auto bone = std::dynamic_pointer_cast<ExDepthBone>(node)) result.append(bone);
        const auto nodeChildren = compat::ngRequireObject(node)->children;
        for (const auto child : nodeChildren) self(self, child);
    };
    const auto rootChildren = children;
    for (const auto child : rootChildren) visit(visit, child);
    return result;
}

std::ptrdiff_t ExDepthRigRoot::findBindingIndex(std::uint64_t targetUuid) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto currentBindings = bindings;
    std::size_t index = 0;
    for (const auto& binding : currentBindings) {
        if (binding.targetUuid == targetUuid) return static_cast<std::ptrdiff_t>(index);
        ++index;
    }
    return -1;
}

DepthRigBindingPointer ExDepthRigRoot::getOrCreateBinding(std::shared_ptr<Node> target,
        ExDepthTargetKind kind) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    const auto index = findBindingIndex(compat::ngRequireObject(target)->getUUID());
    if (index >= 0) return DepthRigBindingPointer::fromArray(bindings, static_cast<std::size_t>(index),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthbone.d"), 405);
    ExDepthRigBinding binding;
    binding.targetUuid = target->getUUID();
    binding.targetKind = kind;
    bindings.append(binding);
    return DepthRigBindingPointer::fromArray(bindings, bindings.size() - 1,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthbone.d"), 411);
}

void ExDepthRigRoot::addBoneSource(std::shared_ptr<Node> target, ExDepthTargetKind kind,
        std::shared_ptr<ExDepthBone> bone) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!target) throw compat::ThrowableException(detail::copyDepthRigEnforceMessage(
        compat::RetainedString::fromStaticLiteral("target is required")),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthbone.d"), 415);
    if (!bone) throw compat::ThrowableException(detail::copyDepthRigEnforceMessage(
        compat::RetainedString::fromStaticLiteral("bone is required")),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthbone.d"), 416);
    auto binding = getOrCreateBinding(target, kind);
    const auto sourceBoneUuids = compat::ngRequireObject(binding)->sourceBoneUuids;
    const auto searchedUuid = bone->getUUID();
    bool present = false;
    for (auto boneUuid : sourceBoneUuids) if (boneUuid == searchedUuid) { present = true; break; }
    if (!present) binding->sourceBoneUuids.append(bone->getUUID());
    binding->normalizeSourceSettings();
}

void ExDepthRigRoot::removeBoneSource(std::shared_ptr<Node> target,
        std::shared_ptr<ExDepthBone> bone) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!target) throw compat::ThrowableException(detail::copyDepthRigEnforceMessage(
        compat::RetainedString::fromStaticLiteral("target is required")),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthbone.d"), 425);
    if (!bone) throw compat::ThrowableException(detail::copyDepthRigEnforceMessage(
        compat::RetainedString::fromStaticLiteral("bone is required")),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthbone.d"), 426);
    const auto index = findBindingIndex(target->getUUID());
    if (index < 0) return;
    auto bindingOwner = bindings;
    auto bindingPointer = DepthRigBindingPointer::fromArray(bindingOwner, static_cast<std::size_t>(index),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/ext/nodes/exdepthbone.d"), 429);
    auto& binding = *bindingPointer;
    const auto sourceBoneUuids = binding.sourceBoneUuids;
    const auto searchedUuid = bone->getUUID();
    for (std::size_t i = 0; i < sourceBoneUuids.size(); ++i) {
        if (sourceBoneUuids[i] != searchedUuid) continue;
        binding.sourceBoneUuids = compat::detail::ngRemovePlainArrayStable(binding.sourceBoneUuids, i);
        break;
    }
    auto multipliers = binding.influenceRule.multipliersByBoneUuid;
    multipliers.remove(bone->getUUID());
    binding.normalizeSourceSettings();
    if (binding.sourceBoneUuids.empty())
        bindings = compat::detail::ngRemovePlainArrayStable(bindings, static_cast<std::size_t>(index));
}

nicxlive::core::serde::ReadError ExDepthRigRoot::deserializeFromFghj(
        nicxlive::core::serde::TypedJsonView data) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (auto error = Node::deserializeFromFghj(data)) return error;
    bindings.resize(0);
    const auto entries = data.findFirst("bindings");
    // Fghj byElement is an empty range for missing or non-array values.
    if (!entries.isMissing() && entries.kind() == nicxlive::core::serde::JsonKind::Array) {
        for (const auto entry : entries.byElement()) {
            ExDepthRigBinding binding;
            if (auto error = binding.deserializeFromFghj(entry)) return error;
            bindings.append(binding);
        }
    }
    return {};
}

void ExDepthRigRoot::serializeSelfImpl(nicxlive::core::serde::TypedJsonWriter& serializer,
        bool recursive, nicxlive::core::nodes::SerializeNodeFlags flags) const {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    Node::serializeSelfImpl(serializer, recursive, flags);
    if (nicxlive::core::nodes::has_flag(flags, nicxlive::core::nodes::SerializeNodeFlags::Links)
            && !bindings.empty()) {
        serializer.putKey("bindings");
        const auto currentBindings = bindings;
        serializer.serializeValue(currentBindings);
    }
}

} // namespace nijigenerate::ext::nodes
