#include <nijigenerate/compat/retained_string.hpp>
#include <nodes/node.hpp>
#include <memory>

namespace nijigenerate::commands::node::dynamic {
namespace {
[[maybe_unused]] bool tryInstantiateNode(compat::RetainedString className) {
    std::shared_ptr<nicxlive::core::nodes::Node> node;
    try {
        node = nicxlive::core::nodes::Node::inInstantiateNodeRetained(className, nullptr);
    } catch (...) {
        return false;
    }
    return node != nullptr;
}
} // namespace
} // namespace nijigenerate::commands::node::dynamic
