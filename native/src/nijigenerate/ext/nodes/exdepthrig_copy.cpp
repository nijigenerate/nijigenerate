#include "exdepthrig_copy.hpp"
#include "exdepthrig_helpers.hpp"
#include "exdepthbone.hpp"
#include <nijigenerate/compat/array_access.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <algorithm>
#include <functional>
namespace nijigenerate::ext::nodes {
void ExDepthRigRoot::copyFrom(const Node& source, bool clone, bool deepCopy) {
    [[maybe_unused]] const auto sourceLifetime = source.weak_from_this().lock();
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    Node::copyFrom(source, clone, deepCopy);
    if (!dynamic_cast<const ExDepthRigRoot*>(&source)) return;
    ngRemapCopiedDepthRigReferences(std::const_pointer_cast<Node>(source.shared_from_this()), shared_from_this());
}
namespace {
using UuidMap = detail::DepthRigUuidMap;
ExDepthRigBinding copyDepthRigBinding(const ExDepthRigBinding& source, UuidMap map) {
    auto result = source;
    result.targetUuid = detail::remapDepthRigUuid(source.targetUuid, map);
    {
        const auto sourceBoneUuids = source.sourceBoneUuids;
        result.sourceBoneUuids = sourceBoneUuids.duplicate();
    }
    for (auto& uuid : result.sourceBoneUuids) uuid = detail::remapDepthRigUuid(uuid, map);
    {
        const auto sourceSettings = source.sourceSettings;
        result.sourceSettings = sourceSettings.duplicate();
    }
    for (auto& setting : result.sourceSettings) setting.boneUuid = detail::remapDepthRigUuid(setting.boneUuid, map);
    result.influenceRule.multipliersByBoneUuid.reset();
    const auto multipliers = source.influenceRule.multipliersByBoneUuid;
    for (const auto [uuid, multiplier] : multipliers.entries())
        result.influenceRule.multipliersByBoneUuid[detail::remapDepthRigUuid(uuid, map)] = multiplier;
    return result;
}
}
void ngRemapCopiedDepthRigReferences(CopiedRigNode source, CopiedRigNode copy) {
    compat::Slice<CopiedRigNode> sources{source};
    compat::Slice<CopiedRigNode> copies{copy};
    ngRemapCopiedDepthRigReferences(sources, copies);
}
void ngRemapCopiedDepthRigReferences(compat::Slice<CopiedRigNode> sources, compat::Slice<CopiedRigNode> copies) {
    if (sources.size() != copies.size()) return;
    UuidMap map;
    const auto collectionCount = sources.size();
    for (std::size_t i = 0; i < collectionCount; ++i) {
        const auto sourceRoot = compat::ngArrayAt(sources, i);
        const auto copiedRoot = compat::ngArrayAt(copies, i);
        detail::collectDepthRigUuidMap(sourceRoot, copiedRoot, map);
    }
    const auto copyBindings = [&](auto&& self, CopiedRigNode source, CopiedRigNode copy) -> void {
        auto sourceRig = std::dynamic_pointer_cast<ExDepthRigRoot>(source);
        auto copiedRig = std::dynamic_pointer_cast<ExDepthRigRoot>(copy);
        if (sourceRig && copiedRig) {
            copiedRig->bindings.resize(0);
            auto bindings = sourceRig->bindings;
            for (auto& binding : bindings) copiedRig->bindings.append(copyDepthRigBinding(binding, map));
        }
        const auto sourceLength = compat::ngRequireObject(source)->children.size();
        const auto copiedLength = compat::ngRequireObject(copy)->children.size();
        const auto count = sourceLength < copiedLength ? source->children.size() : copy->children.size();
        for (std::size_t i = 0; i < count; ++i) {
            const auto sourceChild = detail::depthRigChildAt(source->children, i, 502);
            const auto copiedChild = detail::depthRigChildAt(copy->children, i, 502);
            self(self, sourceChild, copiedChild);
        }
    };
    const auto copyingCount = sources.size();
    for (std::size_t i = 0; i < copyingCount; ++i) {
        const auto sourceRoot = compat::ngArrayAt(sources, i);
        const auto copiedRoot = compat::ngArrayAt(copies, i);
        copyBindings(copyBindings, sourceRoot, copiedRoot);
    }
}
}
