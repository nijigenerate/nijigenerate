#include "node_hierarchy_actions.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/detail/plain_array_remove.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/path_deformer.hpp>
#include <nodes/projectable.hpp>
#include <resource.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_set>

namespace nijigenerate::actions::node {
namespace {
using nicxlive::core::nodes::NotifyReason;
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Vec2Array;
template<class Value, class Hash, class Equal>
Value nodeMoveMapValue(nicxlive::core::common::ResourceKeyMap<std::uint32_t, Value, Hash, Equal>& values,
    std::uint32_t key, std::size_t sourceLine) {
    const auto found = values.find(key);
    if (found == values.end()) throw compat::ngStaticError<compat::RangeError>(
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/node.d"), sourceLine);
    return found->second;
}
template<class... T> ActionText text(std::string_view key, const T&... values) {
    const auto pattern = core::translation::ngTranslate(key);
    const std::array<core::translation::TypedFormatArgument, sizeof...(T)> args{
        core::translation::ngFormatArgument(values)...};
    auto formatted = core::translation::ngFormatTyped(pattern, args);
    return detail::nodeActionStringAllocation([&] {
        return compat::RetainedString(std::move(formatted));
    });
}
ActionText nodeMoveFormattedText(std::string value) {
    if (value.empty()) return {};
    return detail::nodeActionStringAllocation([&] {
        return compat::RetainedString(std::move(value));
    });
}
void notify(NodeRef node, compat::RetainedStringSlice reason) {
    depthboneinvalidation::ngNotifyDepthBoneTargetChanged(node,
        depthboneinvalidation::DepthBoneMutationKind::TargetTransform, reason);
}
void notifyStructure(NodeRef receiver, NodeRef target) {
    detail::requireActionObject(receiver)->notifyChange(target, NotifyReason::StructureChanged);
}
void rescan() { project::incActivePuppet()->rescanNodes(); }
template<class T> compat::Slice<std::shared_ptr<T>> findNodes(NodeRef node) {
    compat::Slice<std::shared_ptr<T>> result;
    if (auto cast = std::dynamic_pointer_cast<T>(node)) result.append(cast);
    const auto children = node->children;
    for (auto child : children) result.append(findNodes<T>(child));
    return result;
}
std::ptrdiff_t linkIndex(const std::shared_ptr<Drawable>& drawable, const std::shared_ptr<Drawable>& target) {
    for (std::size_t i = 0; i < drawable->weldedLinks.size(); ++i)
        if (drawable->weldedLinks[i].target.lock() == target) return static_cast<std::ptrdiff_t>(i);
    return -1;
}
std::vector<std::ptrdiff_t> vectorOf(const Indices& values) {
    if (values.empty()) return {};
    return {values.begin(), values.end()};
}
void addWeld(const std::shared_ptr<Drawable>& drawable, const std::shared_ptr<Drawable>& target,
    const Indices& indices, float weight) { drawable->addWeldedTarget(target, vectorOf(indices), weight); }
}
NodeMoveAction::NodeMoveAction(Nodes values, NodeRef to, std::size_t offset)
    : parentOffset(offset), nodes(values), newParent(to) {
    prepareSourceClassInfo();
    for (auto node : values) if (!detail::requireActionObject(node)->canReparent(to)) {
        const auto pattern = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
            "%s can not be reparented in to %s due to a circular dependency."));
        const std::array<core::translation::TypedFormatArgument, 2> args{
            core::translation::ngFormatArgument(detail::requireActionObject(node)->getName()),
            core::translation::ngFormatArgument(detail::requireActionObject(to)->getName())};
        auto formatted = core::translation::ngFormatTyped(pattern.view(), args);
        const auto message = detail::nodeActionStringAllocation([&] {
            return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
        });
        throw compat::ThrowableException(message,
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/node.d"), 120);
    }
    for (auto& node : values) {
        const auto transform = detail::requireActionObject(node)->transformNoLock();
        if (detail::requireActionObject(node)->parentPtr()) {
            {
                auto& snapshots = originalTransform;
                const auto key = detail::requireActionObject(node)->getUUID();
                const auto& value = detail::requireActionObject(node)->localTransform;
                auto destination = snapshots[key];
                destination = value;
            }
            {
                auto& snapshots = prevParents;
                const auto key = detail::requireActionObject(node)->getUUID();
                const auto value = detail::requireActionObject(node)->parentPtr();
                auto destination = snapshots[key];
                destination = value;
            }
            {
                auto& snapshots = prevOffsets;
                const auto key = detail::requireActionObject(node)->getUUID();
                const auto value = detail::requireActionObject(node)->getIndexInParent();
                auto destination = snapshots[key];
                destination = value;
            }
            {
                auto& snapshots = zSort;
                const auto key = detail::requireActionObject(node)->getUUID();
                const auto value = detail::requireActionObject(node)->zSort();
                auto destination = snapshots[key];
                destination = value;
            }
            {
                auto& snapshots = originalRelativeSort;
                const auto key = detail::requireActionObject(node)->getUUID();
                const auto value = detail::requireActionObject(node)->relZSort();
                auto destination = snapshots[key];
                destination = value;
            }
        }
        if (to) {
            detail::requireActionObject(node)->reparent(to, offset, true);
            if (zSort.contains(detail::requireActionObject(node)->getUUID())) {
                const auto sortReceiver = detail::requireActionObject(node);
                const auto oldSort = nodeMoveMapValue(zSort, detail::requireActionObject(node)->getUUID(), 140);
                const auto parentSort = to->zSort();
                sortReceiver->zSort(oldSort - parentSort);
            }
            if (!detail::requireActionObject(node)->lockToRootValue()) {
                const auto receiver = detail::requireActionObject(node);
                auto& destination = receiver->localTransform.translation;
                const auto parentTransform = to->transform();
                destination = Node::getRelativePosition(parentTransform.matrix(), transform.matrix());
                detail::requireActionObject(node)->localTransform.update();
            }
            detail::requireActionObject(node)->transformChanged();
            notifyStructure(detail::requireActionObject(node), node);
        } else detail::requireActionObject(node)->setParentSource({});
        if (prevParents.contains(detail::requireActionObject(node)->getUUID())
                && nodeMoveMapValue(prevParents, detail::requireActionObject(node)->getUUID(), 152)) {
            const auto previous = detail::requireActionObject(
                nodeMoveMapValue(prevParents, detail::requireActionObject(node)->getUUID(), 152));
            notifyStructure(previous, node);
        }
        {
            auto& snapshots = newTransform;
            const auto key = detail::requireActionObject(node)->getUUID();
            const auto& value = detail::requireActionObject(node)->localTransform;
            auto destination = snapshots[key];
            destination = value;
        }
        {
            auto& snapshots = newRelativeSort;
            const auto key = detail::requireActionObject(node)->getUUID();
            const auto value = detail::requireActionObject(node)->relZSort();
            auto destination = snapshots[key];
            destination = value;
        }
    }
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
    // The original constructor's nodes parameter shadows the public field.
    for (auto node : values) notify(node, compat::RetainedString::fromStaticLiteral("Node Hierarchy"));
    descrName = values.size() == 1
        ? ext::nodes::ngRetainNodeName(detail::requireActionObject(
            detail::actionArrayValue(values, 0, std::size_t{161}))->getName())
        : core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral("nodes"));
}
void NodeMoveAction::rollback() {
    auto owner = weak_from_this().lock();
    auto values = nodes;
    for (auto& node : values) {
        if (prevParents.contains(detail::requireActionObject(node)->getUUID())
                && nodeMoveMapValue(prevParents, detail::requireActionObject(node)->getUUID(), 170)) {
            if (!detail::requireActionObject(node)->lockToRootValue()) {
                const auto receiver = detail::requireActionObject(node);
                const auto parent = nodeMoveMapValue(prevParents, detail::requireActionObject(node)->getUUID(), 171);
                receiver->setRelativeTo(parent);
            }
            {
                const auto receiver = detail::requireActionObject(node);
                const auto parent = nodeMoveMapValue(prevParents, detail::requireActionObject(node)->getUUID(), 172);
                const auto offset = nodeMoveMapValue(prevOffsets, detail::requireActionObject(node)->getUUID(), 172);
                receiver->reparent(parent, offset, true);
            }
            if (zSort.contains(detail::requireActionObject(node)->getUUID()))
                detail::requireActionObject(node)->zSort(
                    nodeMoveMapValue(originalRelativeSort, detail::requireActionObject(node)->getUUID(), 174));
            {
                const auto receiver = detail::requireActionObject(node);
                auto& destination = receiver->localTransform;
                const auto value = nodeMoveMapValue(originalTransform,
                    detail::requireActionObject(node)->getUUID(), 176);
                destination = value;
            }
            detail::requireActionObject(node)->transformChanged();
            notifyStructure(detail::requireActionObject(node), node);
            if (newParent) notifyStructure(detail::requireActionObject(newParent), node);
        } else {
            detail::requireActionObject(node)->reparent({}, 0);
            if (newParent) notifyStructure(detail::requireActionObject(newParent), node);
        }
    }
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
    const auto notificationNodes = nodes;
    for (auto node : notificationNodes)
        notify(node, compat::RetainedString::fromStaticLiteral("Node Hierarchy"));
}
void NodeMoveAction::redo() {
    auto owner = weak_from_this().lock();
    auto values = nodes;
    for (auto node : values) {
        std::size_t previousParentOrigin;
        if (newParent) {
            if (!detail::requireActionObject(node)->lockToRootValue()) {
                const auto receiver = detail::requireActionObject(node);
                const auto parent = newParent;
                receiver->setRelativeTo(parent);
            }
            {
                const auto receiver = detail::requireActionObject(node);
                const auto parent = newParent;
                const auto offset = parentOffset;
                receiver->reparent(parent, offset, true);
            }
            detail::requireActionObject(node)->zSort(
                nodeMoveMapValue(newRelativeSort, detail::requireActionObject(node)->getUUID(), 201));
            {
                const auto receiver = detail::requireActionObject(node);
                auto& destination = receiver->localTransform;
                const auto value = nodeMoveMapValue(newTransform,
                    detail::requireActionObject(node)->getUUID(), 202);
                destination = value;
            }
            detail::requireActionObject(node)->transformChanged();
            notifyStructure(detail::requireActionObject(node), node);
            previousParentOrigin = 205;
        } else {
            detail::requireActionObject(node)->reparent({}, 0);
            previousParentOrigin = 208;
        }
        if (prevParents.contains(detail::requireActionObject(node)->getUUID())
                && nodeMoveMapValue(prevParents, detail::requireActionObject(node)->getUUID(), previousParentOrigin)) {
            const auto previous = detail::requireActionObject(
                nodeMoveMapValue(prevParents, detail::requireActionObject(node)->getUUID(), previousParentOrigin));
            notifyStructure(previous, node);
        }
    }
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
    const auto notificationNodes = nodes;
    for (auto node : notificationNodes)
        notify(node, compat::RetainedString::fromStaticLiteral("Node Hierarchy"));
}
ActionText NodeMoveAction::describe() {
    auto owner = weak_from_this().lock();
    if (prevParents.empty()) {
        const auto pattern = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Created %s"));
        const std::array args{core::translation::ngFormatArgument(descrName)};
        return nodeMoveFormattedText(core::translation::ngFormatTyped(pattern.view(), args));
    }
    if (!newParent) {
        const auto pattern = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Deleted %s"));
        const std::array args{core::translation::ngFormatArgument(descrName)};
        return nodeMoveFormattedText(core::translation::ngFormatTyped(pattern.view(), args));
    }
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Moved %s to %s"));
    const std::array args{core::translation::ngFormatArgument(descrName),
        core::translation::ngFormatArgument(detail::requireActionObject(newParent)->getName())};
    return nodeMoveFormattedText(core::translation::ngFormatTyped(pattern.view(), args));
}
ActionText NodeMoveAction::describeUndo() {
    auto owner = weak_from_this().lock();
    if (prevParents.empty()) {
        const auto pattern = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Created %s"));
        const std::array args{core::translation::ngFormatArgument(descrName)};
        return nodeMoveFormattedText(core::translation::ngFormatTyped(pattern.view(), args));
    }
    bool fromSingleParent = false;
    if (nodes.size() == 1 && prevParents.size() == 1) {
        // AA.values creates a real array even when only its first element is read.
        const auto previousValues = prevParents.values();
        fromSingleParent = static_cast<bool>(detail::actionArrayValue(previousValues, 0, std::size_t{230}));
    }
    if (fromSingleParent) {
        const auto pattern = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Moved %s from %s"));
        const auto label = core::translation::ngFormatArgument(descrName);
        const auto key = detail::requireActionObject(
            detail::actionArrayValue(nodes, 0, std::size_t{230}))->getUUID();
        const auto parent = detail::requireActionObject(nodeMoveMapValue(prevParents, key, 230));
        const std::array args{label, core::translation::ngFormatArgument(parent->getName())};
        return nodeMoveFormattedText(core::translation::ngFormatTyped(pattern.view(), args));
    }
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Moved %s from origin"));
    const std::array args{core::translation::ngFormatArgument(descrName)};
    return nodeMoveFormattedText(core::translation::ngFormatTyped(pattern.view(), args));
}
ActionText NodeMoveAction::getName() {
    // this.stringof represents the expression, not the action's type name.
    return compat::RetainedString::fromStaticLiteral("this");
}
NodeCentralizeAction::NodeCentralizeAction(NodeRef value) : root(value) {
    prepareSourceClassInfo();
    collect(value);
    {
        const auto nodes = affectedNodes;
        for (auto node : nodes) {
            auto& snapshots = oldTransforms;
            const auto uuid = detail::requireActionObject(node)->getUUID();
            const auto& transform = detail::requireActionObject(node)->localTransform;
            auto destination = snapshots[uuid];
            destination = transform;
        }
    }
    detail::requireActionObject(value)->centralize();
    {
        const auto nodes = affectedNodes;
        for (auto node : nodes) {
            auto& snapshots = newTransforms;
            const auto uuid = detail::requireActionObject(node)->getUUID();
            const auto& transform = detail::requireActionObject(node)->localTransform;
            auto destination = snapshots[uuid];
            destination = transform;
        }
    }
    {
        const auto nodes = affectedNodes;
        for (auto node : nodes) detail::notifyDepthBoneNodeTransform(node,
            compat::RetainedString::fromStaticLiteral("Centralize Node"));
    }
}
void NodeCentralizeAction::collect(NodeRef node) {
    affectedNodes.append(node);
    const auto children = detail::requireActionObject(node)->childrenList();
    for (auto child : children) collect(child);
}
void NodeCentralizeAction::applyTransforms(TransformMap values) {
    const auto keepAlive = weak_from_this().lock();
    const auto nodes = affectedNodes;
    for (auto node : nodes) {
        const auto uuid = detail::requireActionObject(node)->getUUID();
        const auto found = values.find(uuid);
        if (found == values.end()) continue;
        node->localTransform = found->second;
        node->transformChanged();
        node->notifyChange(node, NotifyReason::Transformed);
        detail::notifyDepthBoneNodeTransform(node,
            compat::RetainedString::fromStaticLiteral("Centralize Node"));
    }
}
void NodeCentralizeAction::rollback() { applyTransforms(oldTransforms); }
void NodeCentralizeAction::redo() { applyTransforms(newTransforms); }
ActionText NodeCentralizeAction::describe() {
    const auto keepAlive = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Centralized %s"));
    const auto name = detail::requireActionObject(root)->getName();
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(name)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
ActionText NodeCentralizeAction::describeUndo() {
    const auto keepAlive = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Uncentralized %s"));
    const auto name = detail::requireActionObject(root)->getName();
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(name)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
ActionText NodeCentralizeAction::getName() { return compat::RetainedString::fromStaticLiteral("centralize"); }

template<bool Add> PartAddRemoveMaskAction<Add>::PartAddRemoveMaskAction(std::shared_ptr<Drawable> drawable,
    std::shared_ptr<Part> value, MaskingMode suppliedMode) {
    prepareSourceClassInfo();
    maskSrc = drawable;
    target = value;
    // The original constructor uses suppliedMode for insertion but leaves the member at its default.
    if constexpr (Add) {
        offset = detail::requireActionObject(value)->masks.size();
        const auto uuid = detail::requireActionObject(maskSrc)->getUUID();
        detail::requireActionObject(value)->masks.push_back(MaskBinding{uuid, drawable, suppliedMode});
    } else {
        const auto masks = detail::requireActionObject(value)->masks;
        for (std::size_t i = 0; i < masks.size(); ++i) {
            const auto masker = masks[i];
            if (nicxlive::core::ngResourceObjectEquals(masker.maskSrc.lock(), maskSrc)) {
                offset = i;
                detail::requireActionObject(value)->masks = compat::detail::ngRemovePlainArrayStable(
                    detail::requireActionObject(value)->masks, i);
                break;
            }
        }
    }
    detail::requireActionObject(value)->notifyChange(value, NotifyReason::AttributeChanged);
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
}
template<bool Add> void PartAddRemoveMaskAction<Add>::rollback() {
    const auto keepAlive = this->weak_from_this().lock();
    if constexpr (Add) {
        {
            const auto receiver = detail::requireActionObject(target);
            const auto notificationTarget = target;
            receiver->notifyChange(notificationTarget, NotifyReason::AttributeChanged);
        }
        const auto removalTarget = detail::requireActionObject(target);
        removalTarget->masks = compat::detail::ngRemovePlainArrayStable(removalTarget->masks, offset);
    } else {
        const auto insertionTarget = detail::requireActionObject(target);
        const auto position = offset;
        const auto uuid = detail::requireActionObject(maskSrc)->getUUID();
        const auto currentMode = mode;
        const auto currentMask = maskSrc;
        insertionTarget->masks.insertValue(position, MaskBinding{uuid, currentMask, currentMode});
        const auto receiver = detail::requireActionObject(target);
        const auto notificationTarget = target;
        receiver->notifyChange(notificationTarget, NotifyReason::AttributeChanged);
    }
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
}
template<bool Add> void PartAddRemoveMaskAction<Add>::redo() {
    const auto keepAlive = this->weak_from_this().lock();
    if constexpr (Add) {
        const auto insertionTarget = detail::requireActionObject(target);
        const auto position = offset;
        const auto uuid = detail::requireActionObject(maskSrc)->getUUID();
        const auto currentMode = mode;
        const auto currentMask = maskSrc;
        insertionTarget->masks.insertValue(position, MaskBinding{uuid, currentMask, currentMode});
        const auto receiver = detail::requireActionObject(target);
        const auto notificationTarget = target;
        receiver->notifyChange(notificationTarget, NotifyReason::AttributeChanged);
    } else {
        {
            const auto receiver = detail::requireActionObject(target);
            const auto notificationTarget = target;
            receiver->notifyChange(notificationTarget, NotifyReason::AttributeChanged);
        }
        const auto removalTarget = detail::requireActionObject(target);
        removalTarget->masks = compat::detail::ngRemovePlainArrayStable(removalTarget->masks, offset);
    }
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
}
template<bool Add> ActionText PartAddRemoveMaskAction<Add>::describe() {
    const auto keepAlive = this->weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(Add
        ? compat::RetainedString::fromStaticLiteral("%s is added to mask of %s")
        : compat::RetainedString::fromStaticLiteral("%s is deleted from mask of %s"));
    const auto maskName = detail::requireActionObject(maskSrc)->getName();
    const auto targetName = detail::requireActionObject(target)->getName();
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(maskName), core::translation::ngFormatArgument(targetName)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
template<bool Add> ActionText PartAddRemoveMaskAction<Add>::describeUndo() {
    const auto keepAlive = this->weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(Add
        ? compat::RetainedString::fromStaticLiteral("%s is deleted from mask of %s")
        : compat::RetainedString::fromStaticLiteral("%s is added to mask of %s"));
    const auto maskName = detail::requireActionObject(maskSrc)->getName();
    const auto targetName = detail::requireActionObject(target)->getName();
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(maskName), core::translation::ngFormatArgument(targetName)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
template<bool Add> ActionText PartAddRemoveMaskAction<Add>::getName() {
    return compat::RetainedString::fromStaticLiteral("this");
}
template class PartAddRemoveMaskAction<true>;
template class PartAddRemoveMaskAction<false>;
PartChangeMaskModeAction::PartChangeMaskModeAction(std::shared_ptr<Part> value,
    std::shared_ptr<Drawable> source, MaskingMode mode) {
    prepareSourceClassInfo();
    target = value;
    maskSrc = source;
    newMode = mode;
    const auto found = detail::requireActionObject(value)->getMaskIdx(source);
    if (found < 0) throw compat::ThrowableException(compat::RetainedString(std::string_view("Mask entry not found")),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/node.d"), 756);
    idx = static_cast<std::size_t>(found);
    oldMode = detail::actionArrayValue(detail::requireActionObject(value)->masks, idx, std::size_t{758}).mode;
    detail::actionArrayValue(detail::requireActionObject(value)->masks, idx, std::size_t{760}).mode = mode;
    detail::requireActionObject(value)->notifyChange(value, NotifyReason::AttributeChanged);
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
}
void PartChangeMaskModeAction::rollback() {
    const auto keepAlive = weak_from_this().lock();
    detail::actionArrayValue(detail::requireActionObject(target)->masks, idx, std::size_t{766}).mode = oldMode;
    const auto receiver = detail::requireActionObject(target);
    const auto notificationTarget = target;
    receiver->notifyChange(notificationTarget, NotifyReason::AttributeChanged);
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
}
void PartChangeMaskModeAction::redo() {
    const auto keepAlive = weak_from_this().lock();
    detail::actionArrayValue(detail::requireActionObject(target)->masks, idx, std::size_t{772}).mode = newMode;
    const auto receiver = detail::requireActionObject(target);
    const auto notificationTarget = target;
    receiver->notifyChange(notificationTarget, NotifyReason::AttributeChanged);
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
}
ActionText PartChangeMaskModeAction::describe() {
    const auto keepAlive = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("%s mask mode changed"));
    const auto name = detail::requireActionObject(maskSrc)->getName();
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(name)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
ActionText PartChangeMaskModeAction::describeUndo() {
    const auto keepAlive = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("%s mask mode restored"));
    const auto name = detail::requireActionObject(maskSrc)->getName();
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(name)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
ActionText PartChangeMaskModeAction::getName() { return compat::RetainedString::fromStaticLiteral("this"); }
PartMaskListChangeAction::PartMaskListChangeAction(FieldInitializationOnly) { prepareSourceClassInfo(); }

std::shared_ptr<PartMaskListChangeAction> PartMaskListChangeAction::allocateSourceReceiver() {
    return detail::createNodeAction<PartMaskListChangeAction>([](auto* storage) {
        ::new (static_cast<void*>(storage)) PartMaskListChangeAction(FieldInitializationOnly{});
    });
}

std::shared_ptr<PartMaskListChangeAction> PartMaskListChangeAction::create(
        ActionText name, std::shared_ptr<Part> value, Masks old, Masks next) {
    auto result = allocateSourceReceiver();
    result->initialize(name, value, old, next);
    return result;
}

void PartMaskListChangeAction::initialize(ActionText name,
        std::shared_ptr<Part> value, Masks old, Masks next) {
    label = name;
    target = value;
    oldMasks = old.duplicate();
    newMasks = next.duplicate();
    redo();
}
void PartMaskListChangeAction::rollback() {
    const auto keepAlive = weak_from_this().lock();
    {
        const auto receiver = target;
        auto masks = oldMasks.duplicate();
        detail::requireActionObject(receiver)->masks = std::move(masks);
    }
    {
        const auto receiver = detail::requireActionObject(target);
        const auto argument = target;
        receiver->notifyChange(argument, NotifyReason::StructureChanged);
    }
    const auto puppet = project::incActivePuppet();
    if (!puppet) detail::failActionObjectReference();
    puppet->rescanNodes();
}
void PartMaskListChangeAction::redo() {
    const auto keepAlive = weak_from_this().lock();
    {
        const auto receiver = target;
        auto masks = newMasks.duplicate();
        detail::requireActionObject(receiver)->masks = std::move(masks);
    }
    {
        const auto receiver = detail::requireActionObject(target);
        const auto argument = target;
        receiver->notifyChange(argument, NotifyReason::StructureChanged);
    }
    const auto puppet = project::incActivePuppet();
    if (!puppet) detail::failActionObjectReference();
    puppet->rescanNodes();
}
ActionText PartMaskListChangeAction::getName() {
    return compat::RetainedString::fromStaticLiteral("this");
}

template<bool Add> DrawableAddRemoveWeldingAction<Add>::DrawableAddRemoveWeldingAction(
    std::shared_ptr<Drawable> source, std::shared_ptr<Drawable> destination, Indices indices, float suppliedWeight) {
    prepareSourceClassInfo();
    drawable = source;
    target = destination;
    if (indices.isNull() || suppliedWeight < 0) {
        std::ptrdiff_t index = -1;
        {
            const auto links = detail::requireActionObject(source)->weldedLinks;
            for (std::size_t i = 0; i < links.size(); ++i) {
                const auto candidate = links[i];
                if (nicxlive::core::ngResourceObjectEquals(candidate.target.lock(), destination)) {
                    index = static_cast<std::ptrdiff_t>(i);
                    break;
                }
            }
        }
        if (index < 0) throw compat::ThrowableException(compat::RetainedString::fromStaticLiteral(
            "DrawableAddRemoveWeldingAction is created without any specific parameters."),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/node.d"), 865);
        const auto link = detail::actionArrayValue(detail::requireActionObject(source)->weldedLinks,
            static_cast<std::size_t>(index), std::size_t{857});
        if (indices.isNull()) indices = link.indices;
        if (suppliedWeight < 0) suppliedWeight = link.weight;
    }
    weight = suppliedWeight;
    weldedVertexIndices = indices.duplicate();
    std::ptrdiff_t counter = -1;
    {
        const auto links = detail::requireActionObject(destination)->weldedLinks;
        for (std::size_t i = 0; i < links.size(); ++i) {
            const auto candidate = links[i];
            if (nicxlive::core::ngResourceObjectEquals(candidate.target.lock(), source)) {
                counter = static_cast<std::ptrdiff_t>(i);
                break;
            }
        }
    }
    if (counter >= 0) {
        {
            const auto saved = detail::actionArrayValue(detail::requireActionObject(destination)->weldedLinks,
                static_cast<std::size_t>(counter), std::size_t{873}).indices;
            counterVertexIndices = saved.duplicate();
        }
        counterWeight = detail::actionArrayValue(detail::requireActionObject(destination)->weldedLinks,
            static_cast<std::size_t>(counter), std::size_t{874}).weight;
    }
    if constexpr (Add) {
        offset = detail::requireActionObject(source)->weldedLinks.size();
        {
            const auto receiver = detail::requireActionObject(source);
            receiver->addWeldedTarget(destination, indices, suppliedWeight);
        }
        counter = -1;
        {
            const auto links = detail::requireActionObject(destination)->weldedLinks;
            for (std::size_t i = 0; i < links.size(); ++i) {
                const auto candidate = links[i];
                if (nicxlive::core::ngResourceObjectEquals(candidate.target.lock(), source)) {
                    counter = static_cast<std::ptrdiff_t>(i);
                    break;
                }
            }
        }
        {
            const auto saved = detail::actionArrayValue(detail::requireActionObject(destination)->weldedLinks,
                static_cast<std::size_t>(counter), std::size_t{881}).indices;
            counterVertexIndices = saved.duplicate();
        }
        counterWeight = detail::actionArrayValue(detail::requireActionObject(destination)->weldedLinks,
            static_cast<std::size_t>(counter), std::size_t{882}).weight;
    } else detail::requireActionObject(source)->removeWeldedTarget(destination);
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
}
template<bool Add> void DrawableAddRemoveWeldingAction<Add>::restoreLinks() {
    const auto keepAlive = this->weak_from_this().lock();
    {
        const auto receiver = detail::requireActionObject(drawable);
        const auto destination = target;
        const auto indices = weldedVertexIndices;
        const auto suppliedWeight = weight;
        receiver->addWeldedTarget(destination, indices, suppliedWeight);
    }
    std::ptrdiff_t own = -1;
    {
        const auto links = detail::requireActionObject(drawable)->weldedLinks;
        for (std::size_t i = 0; i < links.size(); ++i) {
            const auto candidate = links[i];
            if (nicxlive::core::ngResourceObjectEquals(candidate.target.lock(), target)) {
                own = static_cast<std::ptrdiff_t>(i);
                break;
            }
        }
    }
    std::ptrdiff_t counter = -1;
    {
        const auto links = detail::requireActionObject(target)->weldedLinks;
        for (std::size_t i = 0; i < links.size(); ++i) {
            const auto candidate = links[i];
            if (nicxlive::core::ngResourceObjectEquals(candidate.target.lock(), drawable)) {
                counter = static_cast<std::ptrdiff_t>(i);
                break;
            }
        }
    }
    detail::actionArrayValue(detail::requireActionObject(drawable)->weldedLinks,
        static_cast<std::size_t>(own), std::size_t{918}).weight = weight;
    {
        const auto receiver = detail::requireActionObject(target);
        auto links = receiver->weldedLinks;
        auto& destination = detail::actionArrayValue(links,
            static_cast<std::size_t>(counter), std::size_t{919}).indices;
        const auto saved = counterVertexIndices;
        destination = saved.duplicate();
    }
    detail::actionArrayValue(detail::requireActionObject(target)->weldedLinks,
        static_cast<std::size_t>(counter), std::size_t{920}).weight = counterWeight;
}
template<bool Add> void DrawableAddRemoveWeldingAction<Add>::rollback() {
    const auto keepAlive = this->weak_from_this().lock();
    if constexpr (Add) {
        const auto receiver = detail::requireActionObject(drawable);
        const auto destination = target;
        receiver->removeWeldedTarget(destination);
    } else restoreLinks();
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
}
template<bool Add> void DrawableAddRemoveWeldingAction<Add>::redo() {
    const auto keepAlive = this->weak_from_this().lock();
    if constexpr (Add) restoreLinks();
    else {
        const auto receiver = detail::requireActionObject(drawable);
        const auto destination = target;
        receiver->removeWeldedTarget(destination);
    }
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
}
template<bool Add> ActionText DrawableAddRemoveWeldingAction<Add>::describe() {
    const auto keepAlive = this->weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(Add
        ? compat::RetainedString::fromStaticLiteral("%s is added to welded targets of %s")
        : compat::RetainedString::fromStaticLiteral("%s is deleted from welded targets of %s"));
    const auto targetName = detail::requireActionObject(target)->getName();
    const auto drawableName = detail::requireActionObject(drawable)->getName();
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(targetName), core::translation::ngFormatArgument(drawableName)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
template<bool Add> ActionText DrawableAddRemoveWeldingAction<Add>::describeUndo() {
    const auto keepAlive = this->weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(Add
        ? compat::RetainedString::fromStaticLiteral("%s is deleted from welded targets of %s")
        : compat::RetainedString::fromStaticLiteral("%s is added to welded targets of %s"));
    const auto targetName = detail::requireActionObject(target)->getName();
    const auto drawableName = detail::requireActionObject(drawable)->getName();
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(targetName), core::translation::ngFormatArgument(drawableName)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
template<bool Add> ActionText DrawableAddRemoveWeldingAction<Add>::getName() {
    return compat::RetainedString::fromStaticLiteral("this");
}
template class DrawableAddRemoveWeldingAction<true>;
template class DrawableAddRemoveWeldingAction<false>;
DrawableChangeWeldingAction::DrawableChangeWeldingAction(FieldInitializationOnly) {}

std::shared_ptr<DrawableChangeWeldingAction> DrawableChangeWeldingAction::create(
    std::shared_ptr<Drawable> source, std::shared_ptr<Drawable> destination,
    Indices indices, float weight, Indices counterIndices, bool rematch) {
    auto result = detail::createNodeAction<DrawableChangeWeldingAction>([](auto* storage) {
        ::new (static_cast<void*>(storage)) DrawableChangeWeldingAction(FieldInitializationOnly{});
    });
    result->initialize(source, destination, indices, weight, counterIndices, rematch);
    return result;
}

void DrawableChangeWeldingAction::initialize(std::shared_ptr<Drawable> source,
    std::shared_ptr<Drawable> destination, Indices indices, float weight, Indices counterIndices, bool rematch) {
    prepareSourceClassInfo();
    drawable = source;
    target = destination;
    std::ptrdiff_t own = -1;
    {
        const auto links = detail::requireActionObject(source)->weldedLinks;
        for (std::size_t i = 0; i < links.size(); ++i) {
            const auto candidate = links[i];
            if (nicxlive::core::ngResourceObjectEquals(candidate.target.lock(), destination)) {
                own = static_cast<std::ptrdiff_t>(i);
                break;
            }
        }
    }
    std::ptrdiff_t counter = -1;
    {
        const auto links = detail::requireActionObject(destination)->weldedLinks;
        for (std::size_t i = 0; i < links.size(); ++i) {
            const auto candidate = links[i];
            if (nicxlive::core::ngResourceObjectEquals(candidate.target.lock(), source)) {
                counter = static_cast<std::ptrdiff_t>(i);
                break;
            }
        }
    }
    if (own >= 0 && counter >= 0) {
        const auto link = [&] {
            auto links = detail::requireActionObject(source)->weldedLinks;
            const auto index = static_cast<std::size_t>(own);
            detail::actionArrayValue(links, index, std::size_t{984});
            return links.retainPointer(index);
        }();
        const auto counterLink = [&] {
            auto links = detail::requireActionObject(destination)->weldedLinks;
            const auto index = static_cast<std::size_t>(counter);
            detail::actionArrayValue(links, index, std::size_t{985});
            return links.retainPointer(index);
        }();
        Indices result;
        if (!counterIndices.isNull()) result = counterIndices.duplicate();
        else if (rematch) result = detail::requireActionObject(source)->counterWeldingIndices(
            destination, indices);
        else result = detail::requireActionObject(source)->updatedCounterWeldingIndices(
            destination, indices);
        oldWeight = link->weight;
        newWeight = weight;
        oldCounterWeight = counterLink->weight;
        newCounterWeight = 1 - weight;
        {
            const auto saved = link->indices;
            oldIndices = saved.duplicate();
        }
        {
            const auto saved = counterLink->indices;
            oldCounterIndices = saved.duplicate();
        }
        newIndices = indices.duplicate();
        newCounterIndices = result;
        valid = true;
        redo();
    }
}
void DrawableChangeWeldingAction::rollback() {
    const auto keepAlive = weak_from_this().lock();
    std::ptrdiff_t own = -1;
    {
        const auto links = detail::requireActionObject(drawable)->weldedLinks;
        for (std::size_t i = 0; i < links.size(); ++i) {
            const auto candidate = links[i];
            if (nicxlive::core::ngResourceObjectEquals(candidate.target.lock(), target)) {
                own = static_cast<std::ptrdiff_t>(i);
                break;
            }
        }
    }
    std::ptrdiff_t counter = -1;
    {
        const auto links = detail::requireActionObject(target)->weldedLinks;
        for (std::size_t i = 0; i < links.size(); ++i) {
            const auto candidate = links[i];
            if (nicxlive::core::ngResourceObjectEquals(candidate.target.lock(), drawable)) {
                counter = static_cast<std::ptrdiff_t>(i);
                break;
            }
        }
    }
    if (valid && own >= 0 && counter >= 0) {
        const auto link = [&] {
            auto links = detail::requireActionObject(drawable)->weldedLinks;
            const auto index = static_cast<std::size_t>(own);
            detail::actionArrayValue(links, index, std::size_t{1011});
            return links.retainPointer(index);
        }();
        const auto other = [&] {
            auto links = detail::requireActionObject(target)->weldedLinks;
            const auto index = static_cast<std::size_t>(counter);
            detail::actionArrayValue(links, index, std::size_t{1012});
            return links.retainPointer(index);
        }();
        link->weight = oldWeight;
        {
            const auto saved = oldIndices;
            link->indices = saved.duplicate();
        }
        other->weight = oldCounterWeight;
        {
            const auto saved = oldCounterIndices;
            other->indices = saved.duplicate();
        }
    }
}
void DrawableChangeWeldingAction::redo() {
    const auto keepAlive = weak_from_this().lock();
    std::ptrdiff_t own = -1;
    {
        const auto links = detail::requireActionObject(drawable)->weldedLinks;
        for (std::size_t i = 0; i < links.size(); ++i) {
            const auto candidate = links[i];
            if (nicxlive::core::ngResourceObjectEquals(candidate.target.lock(), target)) {
                own = static_cast<std::ptrdiff_t>(i);
                break;
            }
        }
    }
    std::ptrdiff_t counter = -1;
    {
        const auto links = detail::requireActionObject(target)->weldedLinks;
        for (std::size_t i = 0; i < links.size(); ++i) {
            const auto candidate = links[i];
            if (nicxlive::core::ngResourceObjectEquals(candidate.target.lock(), drawable)) {
                counter = static_cast<std::ptrdiff_t>(i);
                break;
            }
        }
    }
    if (valid && own >= 0 && counter >= 0) {
        const auto link = [&] {
            auto links = detail::requireActionObject(drawable)->weldedLinks;
            const auto index = static_cast<std::size_t>(own);
            detail::actionArrayValue(links, index, std::size_t{1027});
            return links.retainPointer(index);
        }();
        const auto other = [&] {
            auto links = detail::requireActionObject(target)->weldedLinks;
            const auto index = static_cast<std::size_t>(counter);
            detail::actionArrayValue(links, index, std::size_t{1028});
            return links.retainPointer(index);
        }();
        link->weight = newWeight;
        {
            const auto saved = newIndices;
            link->indices = saved.duplicate();
        }
        other->weight = newCounterWeight;
        {
            const auto saved = newCounterIndices;
            other->indices = saved.duplicate();
        }
    }
}
ActionText DrawableChangeWeldingAction::describe() {
    const auto keepAlive = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("links of %s and %s are changed."));
    const auto drawableName = detail::requireActionObject(drawable)->getName();
    const auto targetName = detail::requireActionObject(target)->getName();
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(drawableName), core::translation::ngFormatArgument(targetName)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
ActionText DrawableChangeWeldingAction::describeUndo() {
    const auto keepAlive = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("links of %s and %s are restored."));
    const auto drawableName = detail::requireActionObject(drawable)->getName();
    const auto targetName = detail::requireActionObject(target)->getName();
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(drawableName), core::translation::ngFormatArgument(targetName)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
ActionText DrawableChangeWeldingAction::getName() { return compat::RetainedString::fromStaticLiteral("this"); }
NodeActiveAction::NodeActiveAction(NodeRef value, bool next) : self(value), newState(next) {
    prepareSourceClassInfo();
}
ActionText NodeActiveAction::describe() {
    const auto keepAlive = weak_from_this().lock();
    const auto status = core::translation::ngTranslateRetained(newState
        ? compat::RetainedString::fromStaticLiteral("Enabled")
        : compat::RetainedString::fromStaticLiteral("Disabled"));
    const auto name = detail::requireActionObject(self)->getName();
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(status), core::translation::ngFormatArgument(name)};
    auto formatted = core::translation::ngFormatTyped("%s %s", arguments);
    return detail::nodeActionStringAllocation([&] {
        return compat::RetainedString(std::move(formatted));
    });
}
ActionText NodeActiveAction::describeUndo() {
    const auto keepAlive = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("%s was %s"));
    const auto name = detail::requireActionObject(self)->getName();
    const auto status = core::translation::ngTranslateRetained(!newState
        ? compat::RetainedString::fromStaticLiteral("Enabled")
        : compat::RetainedString::fromStaticLiteral("Disabled"));
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(name), core::translation::ngFormatArgument(status)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
ActionText NodeActiveAction::getName() { return compat::RetainedString::fromStaticLiteral("this"); }
NodeRootBaseSetAction::NodeRootBaseSetAction(NodeRef value, bool next) {
    prepareSourceClassInfo();
    node = value;
    origState = detail::requireActionObject(value)->lockToRootValue();
    state = next;
    detail::requireActionObject(value)->setLockToRoot(state);
}
void NodeRootBaseSetAction::rollback() {
    const auto keepAlive = weak_from_this().lock();
    detail::requireActionObject(node)->setLockToRoot(origState);
    const auto receiver = detail::requireActionObject(node);
    const auto target = node;
    receiver->notifyChange(target, NotifyReason::StructureChanged);
}
void NodeRootBaseSetAction::redo() {
    const auto keepAlive = weak_from_this().lock();
    detail::requireActionObject(node)->setLockToRoot(state);
    const auto receiver = detail::requireActionObject(node);
    const auto target = node;
    receiver->notifyChange(target, NotifyReason::StructureChanged);
}
ActionText NodeRootBaseSetAction::describe() {
    const auto keepAlive = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(origState
        ? compat::RetainedString::fromStaticLiteral("%s locked to root node")
        : compat::RetainedString::fromStaticLiteral("%s unlocked from root node"));
    const auto name = detail::requireActionObject(node)->getName();
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(name)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
ActionText NodeRootBaseSetAction::describeUndo() {
    const auto keepAlive = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(state
        ? compat::RetainedString::fromStaticLiteral("%s locked to root node")
        : compat::RetainedString::fromStaticLiteral("%s unlocked from root node"));
    const auto name = detail::requireActionObject(node)->getName();
    const std::array<core::translation::TypedFormatArgument, 1> arguments{
        core::translation::ngFormatArgument(name)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
ActionText NodeRootBaseSetAction::getName() { return compat::RetainedString::fromStaticLiteral("this"); }
bool NodeRootBaseSetAction::merge(ActionPtr other) {
    const auto keepAlive = weak_from_this().lock();
    if (!canMerge(other)) return false;
    detail::requireActionObject(node)->setLockToRoot(!state);
    state = !state;
    return true;
}
bool NodeRootBaseSetAction::canMerge(ActionPtr other) {
    const auto keepAlive = weak_from_this().lock();
    const auto otherChange = std::dynamic_pointer_cast<TSelf>(other);
    if (!otherChange) return false;
    const auto otherNode = otherChange->node;
    const auto currentNode = node;
    return nicxlive::core::ngResourceObjectEquals(otherNode, currentNode);
}
}
