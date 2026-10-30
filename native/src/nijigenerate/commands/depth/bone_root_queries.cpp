#include "detail/bone_dirty_providers.hpp"
#include "detail/bone_math.hpp"
#include <nijigenerate/project.hpp>
#include <nodes/deformable.hpp>

namespace nijigenerate::commands::depth::bone::detail {
using ext::nodes::ExDepthRigRoot;
using ext::nodes::ExDepthRigBinding;
using ext::nodes::ExDepthBone;
using nicxlive::core::nodes::Node;
using nicxlive::core::nodes::Deformable;

compat::Slice<std::shared_ptr<ExDepthRigRoot>> depthBoneRoots() {
    compat::Slice<std::shared_ptr<ExDepthRigRoot>> result;
    auto puppet = project::incActivePuppet();
    if (!puppet || !puppet->root) return result;

    // Capture the result descriptor by reference. Do not allocate a type-erased
    // recursive callable or prune nested rig roots from the original preorder.
    const auto visit = [&result](const auto& self, std::shared_ptr<Node> node) -> void {
        if (!node) return;
        if (auto root = std::dynamic_pointer_cast<ExDepthRigRoot>(node)) result.append(root);
        const auto children = node->childrenList();
        for (auto child : children) self(self, child);
    };
    visit(visit, puppet->root);
    return result;
}

bool hasValidDepthBoneSources(std::shared_ptr<ExDepthRigRoot> root, ExDepthRigBinding& binding) {
    if (!root || binding.sourceBoneUuids.empty()) return false;
    auto sources = binding.sourceBoneUuids;
    for (auto uuid : sources) {
        if (findBoneByUuid(root, uuid)) return true;
    }
    return false;
}

std::shared_ptr<ExDepthRigRoot> findDepthRigRoot(std::shared_ptr<ExDepthBone> bone) {
    std::shared_ptr<Node> cursor = bone;
    while (cursor) {
        if (auto root = std::dynamic_pointer_cast<ExDepthRigRoot>(cursor)) return root;
        cursor = cursor->parentRef();
    }
    return {};
}

bool isSameOrAncestorNode(std::shared_ptr<Node> ancestor, std::shared_ptr<Node> node) {
    auto cursor = node;
    while (cursor) {
        if (cursor == ancestor) return true;
        cursor = cursor->parentRef();
    }
    return false;
}

bool depthBoneRootHasGeneratedTargets(std::shared_ptr<ExDepthRigRoot> root) {
    auto puppet = project::incActivePuppet();
    if (!root || !puppet) return false;
    auto bindings = root->bindings;
    for (auto& binding : bindings) {
        if (!hasValidDepthBoneSources(root, binding)) continue;
        auto target = puppet->findNodeById(static_cast<std::uint32_t>(binding.targetUuid));
        if (std::dynamic_pointer_cast<Deformable>(target)) return true;
    }
    return false;
}

} // namespace nijigenerate::commands::depth::bone::detail
