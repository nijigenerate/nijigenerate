#pragma once
#include <nodes/node.hpp>
#include <memory>

namespace nijigenerate::runtime {
// D Node.parent(Node) calls insertInto; nicxlive Node::setParent only assigns a weak pointer.
inline void ngSetNodeParent(const std::shared_ptr<nicxlive::core::nodes::Node>& child,
    const std::shared_ptr<nicxlive::core::nodes::Node>& parent) {
    child->insertInto(parent, nicxlive::core::nodes::Node::OFFSET_END);
}
}
