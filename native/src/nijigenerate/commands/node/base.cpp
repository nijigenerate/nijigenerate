#include "base.hpp"
#include <nijigenerate/actions/node_hierarchy_actions.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/ext/nodes/exdepthrig_copy.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <resource.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <utility>
namespace nijigenerate::commands::node::base {
namespace {
compat::RetainedString concatenateNodeName(const compat::RetainedString& base,
    const compat::RetainedString& suffix) {
    try {
        std::string text(base.view());
        text.append(suffix.view());
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
}
using namespace actions::node;
using namespace core::actionstack;
using nicxlive::core::nodes::Node;
void copyToClipboard(Nodes nodes) {
    const auto contains = [](Nodes range, NodeRef needle) {
        for (auto candidate : range)
            if (nicxlive::core::ngResourceObjectEquals(candidate, needle)) return true;
        return false;
    };
    Nodes roots;
    for (auto node : nodes) {
        if (!node || contains(roots, node)) continue;
        bool ancestor = false;
        for (auto parent = node->parent(); parent; parent = parent->parent())
            if (contains(nodes, parent)) { ancestor = true; break; }
        if (!ancestor) roots.append(node);
    }
    clipboardNodes.resize(0);
    for (auto node : roots) clipboardNodes.append(node->dup());
    ext::nodes::ngRemapCopiedDepthRigReferences(roots, clipboardNodes);
}
void pasteFromClipboard(NodeRef parent) {
    if (!parent) return;
    incActionPush(std::allocate_shared<NodeMoveAction>(
        nicxlive::core::d_runtime::SourceAllocator<NodeMoveAction>{}, clipboardNodes, parent, 0));
    auto nodes = clipboardNodes;
    for (auto node : nodes) incReloadNode({node});
    clipboardNodes.resize(0);
}
Nodes insertNodesAux(Nodes parents, Nodes children, compat::RetainedString className, compat::RetainedString suffixName) {
    // Source array truth preserves initialized-empty descriptors until reset.
    if (!children.isNull() && parents.size() != children.size()) children = {};
    Nodes created;
    incActionPushGroup();
    for (std::size_t i = 0; i < parents.size(); ++i) {
        auto p = parents[i];
        auto child = Node::inInstantiateNodeRetained(className, nullptr);
        if (!child) continue;
        created.append(child);
        compat::RetainedString name;
        if (!suffixName.empty()) {
            const auto nameReceiver = children.isNull() ? p : children[i];
            const auto baseName = compat::ngRequireObject(nameReceiver)->getName();
            name = concatenateNodeName(baseName, suffixName);
        }
        incAddChildWithHistory(child, p, name);
        if (!children.isNull()) {
            child->localTransform.translation = compat::ngRequireObject(children[i])->localTransform.translation;
            child->transformChanged();
            incActionPush(actions::node::detail::createNodeAction<NodeMoveAction>([&](auto* storage) {
                std::construct_at(storage, Nodes{children[i]}, child);
            }));
        }
    }
    incActionPopGroup(); return created;
}
void incReloadNode(Nodes nodes) {
    for (auto node : nodes) {
        const auto receiver = compat::ngRequireObject(node);
        {
            auto childRange = receiver->children;
            Nodes children;
            if (!childRange.empty())
                children = Nodes::retainedView(childRange.retainPointer(0), childRange.data(), childRange.size());
            incReloadNode(children);
        }
        const auto snapshot = node->children;
        for (auto child : snapshot) compat::ngRequireObject(child)->notifyChange(child);
        node->clearCache();
    }
}
Nodes ngAddNodes(Nodes parents, compat::RetainedString className, compat::RetainedString suffix) {
    return insertNodesAux(parents, {}, className, suffix);
}
Nodes ngInsertNodes(Nodes children, compat::RetainedString className, compat::RetainedString suffix) {
    const auto count = children.size();
    Nodes parents;
    if (count) parents.resize(count);
    for (std::size_t i = 0; i < count; ++i)
        parents[i] = compat::ngRequireObject(children[i])->parent();
    return insertNodesAux(parents, children, className, suffix);
}
compat::RetainedString ngGetCommonNodeType(Nodes nodes) {
    compat::RetainedString type;
    for (auto node : nodes) {
        if (type.isNull()) type = compat::RetainedString(compat::ngRequireObject(node)->typeId());
        else if (type.view() != compat::ngRequireObject(node)->typeId()) return {};
    }
    return type;
}
Nodes ngConvertTo(Nodes nodes, compat::RetainedString toType) {
    // D string truth tests distinguish null from an allocated empty descriptor.
    if (toType.isNull() || nodes.empty()) return {};
    auto group = std::allocate_shared<actions::GroupAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::GroupAction>{});
    Nodes result;
    for (auto node : nodes.duplicate()) {
        auto replacement = Node::inInstantiateNodeRetained(toType);
        compat::ngRequireObject(replacement)->copyFromSource(node, true, false);
        group->addAction(NodeReplaceAction::create(node, replacement, true));
        result.append(replacement);
        replacement->notifyChange(replacement, nicxlive::core::nodes::NotifyReason::StructureChanged);
    }
    incActionPush(group); project::incSelectNodes(result); return result;
}
}
