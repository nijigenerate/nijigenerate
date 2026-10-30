#include "node.hpp"
#include "node_stable_sort.hpp"
#include "nijigenerate/viewport/depth/detail/ieee_float_order.hpp"
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nodes/composite.hpp>
#include <nodes/grid_deformer.hpp>
#include <memory>

namespace nijigenerate::viewport::depth::mesheditor::node {

compat::Slice<std::shared_ptr<nicxlive::core::nodes::Node>> DepthMeshEditorOne::vertexModeDrawableChildren() {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    compat::Slice<Node> subParts;
    const auto findSubDrawable = [&subParts](const auto& recurse, Node n) -> void {
        if (compat::ngRequireObject(n)->coverOthers()) {
            const auto coveredChildren = n->children;
            for (auto child : coveredChildren) recurse(recurse, child);
        }
        if (auto composite = std::dynamic_pointer_cast<nicxlive::core::nodes::Composite>(n)) {
            if (composite->propagateMeshGroup) subParts.append(composite);
        } else if (auto drawable = std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(n)) {
            subParts.append(drawable);
            // A coverOthers Drawable traverses its children twice in the source.
            const auto drawableChildren = n->children;
            for (auto child : drawableChildren) recurse(recurse, child);
        }
    };
    findSubDrawable(findSubDrawable, target);

    detail::phobosStableNodeSort(subParts, [](Node a, Node b) {
        const float left = compat::ngRequireObject(a)->zSort();
        const float right = compat::ngRequireObject(b)->zSort();
        return depth::detail::ieeeFloatOrderKey(left) > depth::detail::ieeeFloatOrderKey(right);
    });
    return subParts;
}

} // namespace nijigenerate::viewport::depth::mesheditor::node
