#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>

namespace nijigenerate::ext::nodes {

void ngRegisterExDepthBoneNodes() {
    using nicxlive::core::nodes::Node;
    Node::inRegisterNodeTypeRetained(compat::RetainedString::fromStaticLiteral("DepthRigRoot"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) -> std::shared_ptr<Node> {
            return ExDepthRigRoot::create(parent);
        }));
    Node::inRegisterNodeTypeRetained(compat::RetainedString::fromStaticLiteral("DepthBone"),
        Node::ParentNodeFactory([](const std::shared_ptr<Node>& parent) -> std::shared_ptr<Node> {
            return ExDepthBone::create(parent);
        }));
}

} // namespace nijigenerate::ext::nodes
