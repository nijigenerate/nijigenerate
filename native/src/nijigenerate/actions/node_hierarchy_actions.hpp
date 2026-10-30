#pragma once
#include "node_value_action.hpp"
#include "depthbone.hpp"
#include "parameter.hpp"
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <nodes/part.hpp>
#include <common/resource_key_map.hpp>
#include <functional>
#include <memory>
#include <utility>
#include <unordered_map>
#include <limits>

namespace nijigenerate::actions::node {
using Node = nicxlive::core::nodes::Node;
using Drawable = nicxlive::core::nodes::Drawable;
using Part = nicxlive::core::nodes::Part;
using Transform = nicxlive::core::nodes::Transform;
using MaskingMode = nicxlive::core::nodes::MaskingMode;
using MaskBinding = nicxlive::core::nodes::MaskBinding;
using NodeRef = std::shared_ptr<Node>;
using Nodes = compat::Slice<NodeRef>;
using Indices = nicxlive::core::common::SharedArray<std::ptrdiff_t>;
using Masks = nicxlive::core::common::SharedArray<MaskBinding>;

class NodeMoveAction : public Action, public std::enable_shared_from_this<NodeMoveAction> {
public:
    struct UuidHash {
        std::size_t operator()(std::uint32_t value) const noexcept { return value; }
    };
    template<class Value> using SnapshotMap = nicxlive::core::common::ResourceKeyMap<
        std::uint32_t, Value, UuidHash, std::equal_to<std::uint32_t>>;
    virtual compat::RetainedString toString() {
        const auto keepAlive = this->weak_from_this().lock();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
protected:
    virtual void prepareSourceClassInfo();
public:
    NodeMoveAction(const NodeMoveAction&) = delete;
    NodeMoveAction& operator=(const NodeMoveAction&) = delete;
    NodeMoveAction(NodeMoveAction&&) = delete;
    NodeMoveAction& operator=(NodeMoveAction&&) = delete;
    ActionText descrName;
    std::size_t parentOffset = 0;
    SnapshotMap<float> zSort, originalRelativeSort, newRelativeSort;
    SnapshotMap<NodeRef> prevParents;
    SnapshotMap<std::size_t> prevOffsets;
    Nodes nodes;
    NodeRef newParent;
    SnapshotMap<Transform> originalTransform, newTransform;
    NodeMoveAction(Nodes nodes, NodeRef newParent, std::size_t offset = 0);
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
};
class NodeCentralizeAction : public Action, public std::enable_shared_from_this<NodeCentralizeAction> {
    struct TransformUuidHash {
        std::size_t operator()(std::uint32_t value) const noexcept { return value; }
    };
public:
    using TransformMap = nicxlive::core::common::ResourceKeyMap<std::uint32_t, Transform,
        TransformUuidHash, std::equal_to<std::uint32_t>>;
    virtual compat::RetainedString toString() {
        const auto keepAlive = this->weak_from_this().lock();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
protected:
    virtual void prepareSourceClassInfo();
public:
    NodeCentralizeAction(const NodeCentralizeAction&) = delete;
    NodeCentralizeAction& operator=(const NodeCentralizeAction&) = delete;
    NodeCentralizeAction(NodeCentralizeAction&&) = delete;
    NodeCentralizeAction& operator=(NodeCentralizeAction&&) = delete;
    NodeRef root;
    Nodes affectedNodes;
    TransformMap oldTransforms, newTransforms;
    explicit NodeCentralizeAction(NodeRef root);
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
private:
    void collect(NodeRef node);
    void applyTransforms(TransformMap transforms);
};
class NodeReplaceAction : public Action, public std::enable_shared_from_this<NodeReplaceAction> {
public:
    virtual compat::RetainedString toString() {
        const auto keepAlive = this->weak_from_this().lock();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
protected:
    virtual void prepareSourceClassInfo();
public:
    NodeReplaceAction(const NodeReplaceAction&) = delete;
    NodeReplaceAction& operator=(const NodeReplaceAction&) = delete;
    NodeReplaceAction(NodeReplaceAction&&) = delete;
    NodeReplaceAction& operator=(NodeReplaceAction&&) = delete;
    ActionText descrName;
    NodeRef srcNode, toNode;
    Nodes children;
    bool deepCopy = false;
    compat::Slice<std::shared_ptr<parameter::ParameterChangeBindingsValueAction>> bindingReorderActions;
    virtual void updateParameterBindings(bool recordActions = true);
    static std::shared_ptr<NodeReplaceAction> create(NodeRef src, NodeRef to, bool deepCopy);
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
protected:
    struct FieldInitializationOnly {};
    explicit NodeReplaceAction(FieldInitializationOnly);
    void initialize(NodeRef src, NodeRef to, bool deepCopy);
private:
    struct DepthRigBindingReplacement {
        std::shared_ptr<ext::nodes::ExDepthRigRoot> root;
        depthbone::Bindings before, after;
    };
    compat::Slice<DepthRigBindingReplacement> depthRigBindingReplacements;
    void captureDepthRigBindings();
    void applyDepthRigBindings(bool forward);
    static float nonZeroScale(float value);
    static Transform rootTransformFor();
    static void restoreWorldTransformUnder(NodeRef child, NodeRef parent, Transform worldTransform);
    void applyReorderOps(bool forward);
};
template<bool Add = false> class PartAddRemoveMaskAction : public Action,
    public std::enable_shared_from_this<PartAddRemoveMaskAction<Add>> {
public:
    virtual compat::RetainedString toString() {
        const auto keepAlive = this->weak_from_this().lock();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
protected:
    virtual void prepareSourceClassInfo();
public:
    PartAddRemoveMaskAction(const PartAddRemoveMaskAction&) = delete;
    PartAddRemoveMaskAction& operator=(const PartAddRemoveMaskAction&) = delete;
    PartAddRemoveMaskAction(PartAddRemoveMaskAction&&) = delete;
    PartAddRemoveMaskAction& operator=(PartAddRemoveMaskAction&&) = delete;
    std::shared_ptr<Part> target;
    MaskingMode mode = MaskingMode::Mask;
    std::size_t offset = 0;
    std::shared_ptr<Drawable> maskSrc;
    PartAddRemoveMaskAction(std::shared_ptr<Drawable> drawable, std::shared_ptr<Part> target, MaskingMode mode);
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
};
using PartAddMaskAction = PartAddRemoveMaskAction<true>;
using PartRemoveMaskAction = PartAddRemoveMaskAction<false>;
class PartChangeMaskModeAction : public Action, public std::enable_shared_from_this<PartChangeMaskModeAction> {
public:
    virtual compat::RetainedString toString() {
        const auto keepAlive = this->weak_from_this().lock();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
protected:
    virtual void prepareSourceClassInfo();
public:
    PartChangeMaskModeAction(const PartChangeMaskModeAction&) = delete;
    PartChangeMaskModeAction& operator=(const PartChangeMaskModeAction&) = delete;
    PartChangeMaskModeAction(PartChangeMaskModeAction&&) = delete;
    PartChangeMaskModeAction& operator=(PartChangeMaskModeAction&&) = delete;
    std::shared_ptr<Part> target;
    std::shared_ptr<Drawable> maskSrc;
    std::size_t idx = 0;
    MaskingMode oldMode = MaskingMode::Mask, newMode = MaskingMode::Mask;
    PartChangeMaskModeAction(std::shared_ptr<Part> target, std::shared_ptr<Drawable> maskSrc, MaskingMode newMode);
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
};
class PartMaskListChangeAction : public Action,
    public std::enable_shared_from_this<PartMaskListChangeAction> {
public:
    virtual compat::RetainedString toString() {
        const auto keepAlive = this->weak_from_this().lock();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
protected:
    virtual void prepareSourceClassInfo();
public:
    PartMaskListChangeAction(const PartMaskListChangeAction&) = delete;
    PartMaskListChangeAction& operator=(const PartMaskListChangeAction&) = delete;
    PartMaskListChangeAction(PartMaskListChangeAction&&) = delete;
    PartMaskListChangeAction& operator=(PartMaskListChangeAction&&) = delete;
    std::shared_ptr<Part> target;
    Masks oldMasks, newMasks;
    ActionText label;
    static std::shared_ptr<PartMaskListChangeAction> create(
        ActionText label, std::shared_ptr<Part> target, Masks oldMasks, Masks newMasks);
    template<class LabelProducer, class TargetProducer, class OldProducer, class NewProducer>
    static std::shared_ptr<PartMaskListChangeAction> createSourceDeferred(
            LabelProducer&& readLabel, TargetProducer&& readTarget,
            OldProducer&& readOld, NewProducer&& readNew) {
        auto result = allocateSourceReceiver();
        // Source new allocates the same receiver before evaluating each argument.
        ActionText name = std::invoke(std::forward<LabelProducer>(readLabel));
        std::shared_ptr<Part> target = std::invoke(std::forward<TargetProducer>(readTarget));
        Masks before = std::invoke(std::forward<OldProducer>(readOld));
        Masks after = std::invoke(std::forward<NewProducer>(readNew));
        result->initialize(std::move(name), std::move(target), std::move(before), std::move(after));
        return result;
    }
    void rollback() override;
    void redo() override;
    ActionText describe() override { return label; }
    ActionText describeUndo() override { return label; }
    ActionText getName() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
protected:
    struct FieldInitializationOnly {};
    explicit PartMaskListChangeAction(FieldInitializationOnly);
    static std::shared_ptr<PartMaskListChangeAction> allocateSourceReceiver();
    void initialize(ActionText label, std::shared_ptr<Part> target, Masks oldMasks, Masks newMasks);
};
template<bool Add = false> class DrawableAddRemoveWeldingAction : public Action,
    public std::enable_shared_from_this<DrawableAddRemoveWeldingAction<Add>> {
public:
    virtual compat::RetainedString toString() {
        const auto keepAlive = this->weak_from_this().lock();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
protected:
    virtual void prepareSourceClassInfo();
public:
    DrawableAddRemoveWeldingAction(const DrawableAddRemoveWeldingAction&) = delete;
    DrawableAddRemoveWeldingAction& operator=(const DrawableAddRemoveWeldingAction&) = delete;
    DrawableAddRemoveWeldingAction(DrawableAddRemoveWeldingAction&&) = delete;
    DrawableAddRemoveWeldingAction& operator=(DrawableAddRemoveWeldingAction&&) = delete;
    std::shared_ptr<Drawable> drawable;
    std::size_t offset = 0;
    std::shared_ptr<Drawable> target;
    Indices weldedVertexIndices;
    float weight = std::numeric_limits<float>::quiet_NaN();
    Indices counterVertexIndices;
    float counterWeight = std::numeric_limits<float>::quiet_NaN();
    DrawableAddRemoveWeldingAction(std::shared_ptr<Drawable> drawable, std::shared_ptr<Drawable> target,
        Indices indices = {}, float weight = -1);
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
private:
    void restoreLinks();
};
using DrawableAddWeldingAction = DrawableAddRemoveWeldingAction<true>;
using DrawableRemoveWeldingAction = DrawableAddRemoveWeldingAction<false>;
class DrawableChangeWeldingAction : public Action, public std::enable_shared_from_this<DrawableChangeWeldingAction> {
public:
    virtual compat::RetainedString toString() {
        const auto keepAlive = this->weak_from_this().lock();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
protected:
    virtual void prepareSourceClassInfo();
public:
    DrawableChangeWeldingAction(const DrawableChangeWeldingAction&) = delete;
    DrawableChangeWeldingAction& operator=(const DrawableChangeWeldingAction&) = delete;
    DrawableChangeWeldingAction(DrawableChangeWeldingAction&&) = delete;
    DrawableChangeWeldingAction& operator=(DrawableChangeWeldingAction&&) = delete;
    std::shared_ptr<Drawable> drawable, target;
    bool valid = false;
    float oldWeight = std::numeric_limits<float>::quiet_NaN(), newWeight = std::numeric_limits<float>::quiet_NaN();
    float oldCounterWeight = std::numeric_limits<float>::quiet_NaN(), newCounterWeight = std::numeric_limits<float>::quiet_NaN();
    Indices oldIndices, newIndices, oldCounterIndices, newCounterIndices;
    static std::shared_ptr<DrawableChangeWeldingAction> create(
        std::shared_ptr<Drawable> drawable, std::shared_ptr<Drawable> target,
        Indices indices, float weight, Indices counterIndices = {}, bool rematch = false);
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
protected:
    struct FieldInitializationOnly {};
    explicit DrawableChangeWeldingAction(FieldInitializationOnly);
    void initialize(std::shared_ptr<Drawable> drawable, std::shared_ptr<Drawable> target,
        Indices indices, float weight, Indices counterIndices = {}, bool rematch = false);
};
class NodeActiveAction : public Action, public std::enable_shared_from_this<NodeActiveAction> {
public:
    virtual compat::RetainedString toString() {
        const auto keepAlive = this->weak_from_this().lock();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
protected:
    virtual void prepareSourceClassInfo();
public:
    NodeActiveAction(const NodeActiveAction&) = delete;
    NodeActiveAction& operator=(const NodeActiveAction&) = delete;
    NodeActiveAction(NodeActiveAction&&) = delete;
    NodeActiveAction& operator=(NodeActiveAction&&) = delete;
    NodeRef self;
    bool newState = false;
    NodeActiveAction(NodeRef self, bool newState);
    void rollback() override {
        const auto keepAlive = weak_from_this().lock();
        detail::requireActionObject(self)->setEnabled(!newState);
    }
    void redo() override {
        const auto keepAlive = weak_from_this().lock();
        detail::requireActionObject(self)->setEnabled(newState);
    }
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr) override { return false; }
    bool canMerge(ActionPtr) override { return false; }
};
class NodeRootBaseSetAction : public Action, public std::enable_shared_from_this<NodeRootBaseSetAction> {
public:
    using TSelf = NodeRootBaseSetAction;
    virtual compat::RetainedString toString() {
        const auto keepAlive = this->weak_from_this().lock();
        prepareSourceClassInfo();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*this));
    }
protected:
    virtual void prepareSourceClassInfo();
public:
    NodeRootBaseSetAction(const NodeRootBaseSetAction&) = delete;
    NodeRootBaseSetAction& operator=(const NodeRootBaseSetAction&) = delete;
    NodeRootBaseSetAction(NodeRootBaseSetAction&&) = delete;
    NodeRootBaseSetAction& operator=(NodeRootBaseSetAction&&) = delete;
    NodeRef node;
    bool origState = false, state = false;
    NodeRootBaseSetAction(NodeRef node, bool state);
    void rollback() override;
    void redo() override;
    ActionText describe() override;
    ActionText describeUndo() override;
    ActionText getName() override;
    bool merge(ActionPtr other) override;
    bool canMerge(ActionPtr other) override;
};
void incMoveChildrenWithHistory(Nodes nodes, NodeRef to, std::size_t offset);
void incMoveChildWithHistory(NodeRef node, NodeRef to, std::size_t offset);
void incAddChildWithHistory(NodeRef node, NodeRef to, ActionText name = {});
std::shared_ptr<GroupAction> incDeleteMaskOfNode(NodeRef node, std::shared_ptr<GroupAction> group = {});
std::shared_ptr<GroupAction> incDeleteWeldedLinksOfNode(NodeRef node, std::shared_ptr<GroupAction> group = {});
std::shared_ptr<GroupAction> ngDeleteDepthRigReferencesOfNode(NodeRef node, std::shared_ptr<GroupAction> group = {});
void incDeleteChildWithHistory(NodeRef node);
void incDeleteChildrenWithHistory(Nodes nodes);
void incLockToRootNode(NodeRef node);
}

namespace nicxlive::core::d_runtime {
#define NG_NODE_ACTION_TYPE_ORIGIN(Type, ClassName, Qualified, Unqualified) \
    template<> struct SourceObjectTypeNames<::nijigenerate::actions::node::Type> { \
        static RetainedString className() { return RetainedString::fromStaticLiteral(ClassName); } \
        static RetainedString templateArgumentName() { \
            return RetainedString::fromStaticLiteral("(" Qualified ")"); \
        } \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::actions::node::Type> { \
        inline static constexpr bool isNativeObject = true; \
        static SourceTypeSpellingNames get() { \
            return {RetainedString::fromStaticLiteral("(" Qualified ")"), \
                RetainedString::fromStaticLiteral("(" Unqualified ")"), \
                RetainedString::fromStaticLiteral(Qualified), RetainedString::fromStaticLiteral(Unqualified)}; \
        } \
    }
NG_NODE_ACTION_TYPE_ORIGIN(NodeMoveAction,
    "nijigenerate.actions.node.NodeMoveAction",
    "nijigenerate.actions.node.NodeMoveAction", "NodeMoveAction");
NG_NODE_ACTION_TYPE_ORIGIN(NodeCentralizeAction,
    "nijigenerate.actions.node.NodeCentralizeAction",
    "nijigenerate.actions.node.NodeCentralizeAction", "NodeCentralizeAction");
NG_NODE_ACTION_TYPE_ORIGIN(NodeReplaceAction,
    "nijigenerate.actions.node.NodeReplaceAction",
    "nijigenerate.actions.node.NodeReplaceAction", "NodeReplaceAction");
NG_NODE_ACTION_TYPE_ORIGIN(PartChangeMaskModeAction,
    "nijigenerate.actions.node.PartChangeMaskModeAction",
    "nijigenerate.actions.node.PartChangeMaskModeAction", "PartChangeMaskModeAction");
NG_NODE_ACTION_TYPE_ORIGIN(PartMaskListChangeAction,
    "nijigenerate.actions.node.PartMaskListChangeAction",
    "nijigenerate.actions.node.PartMaskListChangeAction", "PartMaskListChangeAction");
NG_NODE_ACTION_TYPE_ORIGIN(DrawableChangeWeldingAction,
    "nijigenerate.actions.node.DrawableChangeWeldingAction",
    "nijigenerate.actions.node.DrawableChangeWeldingAction", "DrawableChangeWeldingAction");
NG_NODE_ACTION_TYPE_ORIGIN(NodeActiveAction,
    "nijigenerate.actions.node.NodeActiveAction",
    "nijigenerate.actions.node.NodeActiveAction", "NodeActiveAction");
NG_NODE_ACTION_TYPE_ORIGIN(NodeRootBaseSetAction,
    "nijigenerate.actions.node.NodeRootBaseSetAction",
    "nijigenerate.actions.node.NodeRootBaseSetAction", "NodeRootBaseSetAction");
NG_NODE_ACTION_TYPE_ORIGIN(PartAddRemoveMaskAction<true>,
    "nijigenerate.actions.node.PartAddRemoveMaskAction!true.PartAddRemoveMaskAction",
    "nijigenerate.actions.node.PartAddRemoveMaskAction!true", "PartAddRemoveMaskAction!true");
NG_NODE_ACTION_TYPE_ORIGIN(PartAddRemoveMaskAction<false>,
    "nijigenerate.actions.node.PartAddRemoveMaskAction!false.PartAddRemoveMaskAction",
    "nijigenerate.actions.node.PartAddRemoveMaskAction!false", "PartAddRemoveMaskAction!false");
NG_NODE_ACTION_TYPE_ORIGIN(DrawableAddRemoveWeldingAction<true>,
    "nijigenerate.actions.node.DrawableAddRemoveWeldingAction!true.DrawableAddRemoveWeldingAction",
    "nijigenerate.actions.node.DrawableAddRemoveWeldingAction!true", "DrawableAddRemoveWeldingAction!true");
NG_NODE_ACTION_TYPE_ORIGIN(DrawableAddRemoveWeldingAction<false>,
    "nijigenerate.actions.node.DrawableAddRemoveWeldingAction!false.DrawableAddRemoveWeldingAction",
    "nijigenerate.actions.node.DrawableAddRemoveWeldingAction!false", "DrawableAddRemoveWeldingAction!false");
#undef NG_NODE_ACTION_TYPE_ORIGIN
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::actions::node {
inline void NodeMoveAction::prepareSourceClassInfo() {
    detail::prepareNodeActionClass<NodeMoveAction>("nijigenerate.actions.node.NodeMoveAction");
}
inline void NodeCentralizeAction::prepareSourceClassInfo() {
    detail::prepareNodeActionClass<NodeCentralizeAction>("nijigenerate.actions.node.NodeCentralizeAction");
}
inline void NodeReplaceAction::prepareSourceClassInfo() {
    detail::prepareNodeActionClass<NodeReplaceAction>("nijigenerate.actions.node.NodeReplaceAction");
}
template<bool Add>
inline void PartAddRemoveMaskAction<Add>::prepareSourceClassInfo() {
    detail::prepareNodeActionClass<PartAddRemoveMaskAction>(Add
            ? "nijigenerate.actions.node.PartAddRemoveMaskAction!true.PartAddRemoveMaskAction"
            : "nijigenerate.actions.node.PartAddRemoveMaskAction!false.PartAddRemoveMaskAction");
}
inline void PartChangeMaskModeAction::prepareSourceClassInfo() {
    detail::prepareNodeActionClass<PartChangeMaskModeAction>("nijigenerate.actions.node.PartChangeMaskModeAction");
}
inline void PartMaskListChangeAction::prepareSourceClassInfo() {
    detail::prepareNodeActionClass<PartMaskListChangeAction>("nijigenerate.actions.node.PartMaskListChangeAction");
}
template<bool Add>
inline void DrawableAddRemoveWeldingAction<Add>::prepareSourceClassInfo() {
    detail::prepareNodeActionClass<DrawableAddRemoveWeldingAction>(Add
            ? "nijigenerate.actions.node.DrawableAddRemoveWeldingAction!true.DrawableAddRemoveWeldingAction"
            : "nijigenerate.actions.node.DrawableAddRemoveWeldingAction!false.DrawableAddRemoveWeldingAction");
}
inline void DrawableChangeWeldingAction::prepareSourceClassInfo() {
    detail::prepareNodeActionClass<DrawableChangeWeldingAction>("nijigenerate.actions.node.DrawableChangeWeldingAction");
}
inline void NodeActiveAction::prepareSourceClassInfo() {
    detail::prepareNodeActionClass<NodeActiveAction>("nijigenerate.actions.node.NodeActiveAction");
}
inline void NodeRootBaseSetAction::prepareSourceClassInfo() {
    detail::prepareNodeActionClass<NodeRootBaseSetAction>("nijigenerate.actions.node.NodeRootBaseSetAction");
}
}

namespace nijigenerate::core::translation {

#define NG_NODE_ACTION_FORMAT(Type) \
template<> struct FormatObjectTraits<std::shared_ptr<::nijigenerate::actions::node::Type>> \
    : detail::SourceNoArgObjectFormat<std::shared_ptr<::nijigenerate::actions::node::Type>> {};
NG_NODE_ACTION_FORMAT(NodeMoveAction)
NG_NODE_ACTION_FORMAT(NodeCentralizeAction)
NG_NODE_ACTION_FORMAT(NodeReplaceAction)
NG_NODE_ACTION_FORMAT(PartChangeMaskModeAction)
NG_NODE_ACTION_FORMAT(PartMaskListChangeAction)
NG_NODE_ACTION_FORMAT(DrawableChangeWeldingAction)
NG_NODE_ACTION_FORMAT(NodeActiveAction)
NG_NODE_ACTION_FORMAT(NodeRootBaseSetAction)
#undef NG_NODE_ACTION_FORMAT

template<bool Add> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::node::PartAddRemoveMaskAction<Add>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::node::PartAddRemoveMaskAction<Add>>> {};
template<bool Add> struct FormatObjectTraits<std::shared_ptr<
    ::nijigenerate::actions::node::DrawableAddRemoveWeldingAction<Add>>>
    : detail::SourceNoArgObjectFormat<std::shared_ptr<
        ::nijigenerate::actions::node::DrawableAddRemoveWeldingAction<Add>>> {};

} // namespace nijigenerate::core::translation
