#pragma once

#include "common.hpp"
#include <nijigenerate/ext/nodes/exdepthbone_node.hpp>
#include <puppet.hpp>
#include <concepts>
#include <memory>

namespace nijigenerate::panels::inspector {
namespace depthbone {

inline std::shared_ptr<ext::nodes::ExDepthRigRoot> ngDepthRigRootForTarget(
    std::shared_ptr<nicxlive::core::Puppet> puppet, std::shared_ptr<Node> target) {
    if (!puppet || !puppet->root) return {};

    std::shared_ptr<ext::nodes::ExDepthRigRoot> firstRoot;
    std::shared_ptr<ext::nodes::ExDepthRigRoot> boundRoot;
    const auto visit = [&](auto&& recurse, std::shared_ptr<Node> node) -> void {
        if (!node || boundRoot) return;
        if (auto root = std::dynamic_pointer_cast<ext::nodes::ExDepthRigRoot>(node)) {
            if (!firstRoot) firstRoot = root;
            if (target && root->findBindingIndex(target->getUUID()) >= 0) {
                boundRoot = root;
                return;
            }
        }
        // The source foreach retains its initial child-array descriptor.
        const auto children = node->childrenList();
        for (auto child : children) recurse(recurse, std::move(child));
    };
    visit(visit, puppet->root);
    return boundRoot ? boundRoot : firstRoot;
}

template<ModelEditSubMode mode, class T> requires (
    mode == ModelEditSubMode::Layout &&
    (std::derived_from<T, ext::nodes::ExDepthRigRoot> || std::derived_from<T, ext::nodes::ExDepthBone>))
class NodeInspector;
} // namespace depthbone
} // namespace nijigenerate::panels::inspector

namespace nicxlive::core::d_runtime {
template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires (Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout &&
        (std::derived_from<T, ::nijigenerate::ext::nodes::ExDepthRigRoot> ||
            std::derived_from<T, ::nijigenerate::ext::nodes::ExDepthBone>))
struct SourceObjectTypeNames<::nijigenerate::panels::inspector::depthbone::NodeInspector<Mode, T>> {
    static RetainedString className() { return declaration(true, true, true); }
    static RetainedString nestedQualifiedScope() { return declaration(true, true, false); }
    static RetainedString nestedUnqualifiedScope() { return declaration(false, true, false); }
    static RetainedString element(bool qualified) { return declaration(qualified, false, false); }
    static RetainedString templateArgumentName() {
        return ::nijigenerate::panels::inspector::detail::inspectorClassArgument(element(true));
    }
private:
    static RetainedString declaration(bool qualified, bool classLeaf, bool classInfo) {
        const auto argument = ngSourceTypeSpelling<T>(classInfo ? SourceTypeSpelling::QualifiedElement :
            SourceTypeSpelling::UnqualifiedElement);
        return ::nijigenerate::panels::inspector::detail::inspectorNameBytes([&] {
            std::string name = qualified ? "nijigenerate.panels.inspector.depthbone." : "";
            name += "NodeInspector!(ModelEditSubMode.Layout, ";
            name.append(argument.view());
            name += ')';
            if (classLeaf) name += ".NodeInspector";
            return name;
        });
    }
};

template<::nijigenerate::panels::inspector::ModelEditSubMode Mode, class T>
    requires (Mode == ::nijigenerate::panels::inspector::ModelEditSubMode::Layout &&
        (std::derived_from<T, ::nijigenerate::ext::nodes::ExDepthRigRoot> ||
            std::derived_from<T, ::nijigenerate::ext::nodes::ExDepthBone>))
struct SourceTypeSpellings<::nijigenerate::panels::inspector::depthbone::NodeInspector<Mode, T>> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        using Names = SourceObjectTypeNames<::nijigenerate::panels::inspector::depthbone::NodeInspector<Mode, T>>;
        const auto qualified = Names::element(true);
        const auto unqualified = Names::element(false);
        return {::nijigenerate::panels::inspector::detail::inspectorClassArgument(qualified),
            ::nijigenerate::panels::inspector::detail::inspectorClassArgument(unqualified), qualified, unqualified};
    }
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::panels::inspector {
namespace depthbone {
template<ModelEditSubMode mode, class T> requires (
    mode == ModelEditSubMode::Layout &&
    (std::derived_from<T, ext::nodes::ExDepthRigRoot> || std::derived_from<T, ext::nodes::ExDepthBone>))
class NodeInspector : public BaseInspector<mode, T> {
    using Self = NodeInspector;
    using Base = BaseInspector<mode, T>;
protected:
    void prepareSourceClassInfo() override {
        Base::prepareSourceClassInfo();
        ::nijigenerate::panels::inspector::detail::prepareInspectorClassRecord<Self>();
    }
public:
    // The logical source constructor runs through ngCreateInspector after the
    // real shared receiver exists, then invokes BaseInspector::initialize.
    NodeInspector() = default;
    NodeInspector(const NodeInspector&) = delete;
    NodeInspector& operator=(const NodeInspector&) = delete;
    NodeInspector(NodeInspector&&) = delete;
    NodeInspector& operator=(NodeInspector&&) = delete;
    // Defined against the real depth command and widget providers. The root
    // and bone branches have different UI bodies, selected by the target type.
    void run() override;
};

} // namespace depthbone

using depthbone::ngDepthRigRootForTarget;

} // namespace nijigenerate::panels::inspector
#include "depthbone_run.hpp"
