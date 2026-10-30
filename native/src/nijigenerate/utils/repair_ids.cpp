#include <nijigenerate/utils/repair.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>

#include <nodes/node.hpp>

namespace nijigenerate::utils::repair {

void ngRegenerateNodeIDs(std::shared_ptr<nicxlive::core::nodes::Node> node) {
    const auto children = compat::ngRequireObject(node)->children;
    for (auto child : children) {
        ngRegenerateNodeIDs(child);
    }

    node->forceSetUUID(nicxlive::core::nodes::Node::inCreateUUID());
}

} // namespace nijigenerate::utils::repair
