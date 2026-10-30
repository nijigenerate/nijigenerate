#pragma once
#include <nodes/node.hpp>
#include <memory>
#include <span>

namespace nijigenerate::commands::node::base {
inline void incReloadNode(std::span<const std::shared_ptr<::nicxlive::core::nodes::Node>> nodes) {
    for (const auto& node : nodes) {
        incReloadNode(node->children);
        for (const auto& child : node->children) {
            child->notifyChange(child);
        }
        node->clearCache();
    }
}
}
