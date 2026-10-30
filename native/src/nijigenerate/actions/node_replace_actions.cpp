#include "node_hierarchy_actions.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/assert_error.hpp>
#include <nodes/grid_deformer.hpp>
#include <nodes/path_deformer.hpp>
#include <nodes/projectable.hpp>
#include <resource.hpp>
#include <compat/object_metadata.hpp>
#include <cassert>
#include <bit>
#include <cmath>
#include <cstdint>
#include <new>
#include <source_location>
#include <stdexcept>
#include <unordered_set>

namespace nijigenerate::actions::node {
namespace {
using nicxlive::core::nodes::NotifyReason;
using nicxlive::core::nodes::Deformable;
using nicxlive::core::nodes::GridDeformer;
using nicxlive::core::nodes::PathDeformer;
using nicxlive::core::nodes::Projectable;
using nicxlive::core::math::Vec2;
using nicxlive::core::math::Vec3;
using nicxlive::core::math::Vec2Array;
template<class... T> ActionText text(std::string_view key, const T&... values) {
    const auto pattern = core::translation::ngTranslate(key);
    const std::array<core::translation::TypedFormatArgument, sizeof...(T)> args{
        core::translation::ngFormatArgument(values)...};
    auto formatted = core::translation::ngFormatTyped(pattern, args);
    return detail::nodeActionStringAllocation([&] {
        return compat::RetainedString(std::move(formatted));
    });
}
template<class T> compat::Slice<std::shared_ptr<T>> findNodes(NodeRef node) {
    compat::Slice<std::shared_ptr<T>> result;
    if (auto cast = std::dynamic_pointer_cast<T>(node)) result.append(cast);
    const auto children = detail::requireActionObject(node)->children;
    for (auto child : children) result.append(findNodes<T>(child));
    return result;
}
template<class T> auto findPuppetNodes(
    std::shared_ptr<nicxlive::core::Puppet> receiver, NodeRef node) {
    // Keep the evaluated receiver alive and return the canonical retained array.
    return detail::requireActionObject(receiver)->findNodesType<T>(node);
}
template<class T> auto findCurrentPuppetNodes() {
    auto receiver = project::incActivePuppet();
    auto root = detail::requireActionObject(project::incActivePuppet())->root;
    return findPuppetNodes<T>(receiver, root);
}
std::vector<Vec2> collectVertices(NodeRef node) {
    std::vector<Vec2> result;
    if (auto deformable = std::dynamic_pointer_cast<Deformable>(node)) {
        result = deformable->getVertices().toArray();
    } else if (auto drawable = std::dynamic_pointer_cast<Drawable>(node)) {
        result = drawable->getMesh().vertices.toArray();
    }
    return result;
}
compat::Slice<std::size_t> buildRemap(const std::vector<Vec2>& oldVertices, const std::vector<Vec2>& newVertices) {
    if (oldVertices.empty() || oldVertices.size() != newVertices.size()) return {};
    compat::Slice<std::size_t> remap;
    remap.resize(oldVertices.size());
    nicxlive::core::common::SharedArray<bool> used;
    used.resize(newVertices.size());
    constexpr float tolerance = 1e-4f;
    for (std::size_t oldIndex = 0; oldIndex < oldVertices.size(); ++oldIndex) {
        std::size_t match = std::numeric_limits<std::size_t>::max();
        for (std::size_t newIndex = 0; newIndex < newVertices.size(); ++newIndex) {
            if (used[newIndex]) continue;
            const Vec2 delta{oldVertices[oldIndex].x - newVertices[newIndex].x,
                oldVertices[oldIndex].y - newVertices[newIndex].y};
            const float xx = delta.x * delta.x, yy = delta.y * delta.y;
#if defined(_WIN32)
            using SourceReal = double;
#else
            using SourceReal = long double;
#endif
            SourceReal lengthSquared = 0;
            lengthSquared += xx; lengthSquared += yy;
            if (lengthSquared <= tolerance * tolerance) { match = newIndex; break; }
        }
        if (match == std::numeric_limits<std::size_t>::max()) return {};
        remap[oldIndex] = match; used[match] = true;
    }
    return remap;
}
}
void NodeReplaceAction::updateParameterBindings(bool recordActions) {
    const auto keepAlive = weak_from_this().lock();
    auto parent = detail::requireActionObject(srcNode)->parentPtr();
    auto src = toNode, to = srcNode;
    if (detail::requireActionObject(toNode)->parentPtr()) {
        parent = detail::requireActionObject(toNode)->parentPtr();
        src = srcNode;
        to = toNode;
    }
#ifndef NDEBUG
    if (!detail::requireActionObject(parent)->puppetRef())
        throw compat::ngStaticError<compat::AssertError>(
            compat::RetainedString::fromStaticLiteral("Assertion failure"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/node.d"), 324);
#endif
    auto parameters = detail::requireActionObject(
        detail::requireActionObject(parent)->puppetRef())->parameters;
    if (recordActions) bindingReorderActions.resize(0);
    for (auto param : parameters) {
        auto bindings = detail::requireActionObject(param)->bindings;
        for (auto binding : bindings) {
            if (!nicxlive::core::ngResourceObjectEquals(
                    detail::requireActionObject(binding)->getTarget().getResource(), src)) continue;
            const auto oldVertices = collectVertices(src);
            binding->setResourceTarget(to, binding->getTarget().name);
            auto grid = std::dynamic_pointer_cast<GridDeformer>(to);
            if (!grid) continue;
            auto deform = std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(binding);
            if (!deform) continue;
            const auto remap = buildRemap(oldVertices, collectVertices(grid));
            if (remap.empty()) continue;
            bool needsReInterpolate = false;
            const auto xCount = deform->values.size();
            for (std::size_t x = 0; x < xCount; ++x) {
                const auto yCount = detail::actionArrayValue(deform->values, x, std::size_t{377}).size();
                for (std::size_t y = 0; y < yCount; ++y) {
                    auto offsets = detail::actionArrayValue(
                        detail::actionArrayValue(deform->values, x, std::size_t{378}),
                        y, std::size_t{378}).vertexOffsets.dup();
                    if (offsets.size() != remap.size()) {
                        offsets.resize(remap.size());
                        const auto offsetCount = offsets.size();
                        for (std::size_t i = 0; i < offsetCount; ++i) offsets[i] = Vec2{0, 0};
                        detail::actionArrayValue(
                            detail::actionArrayValue(deform->values, x, std::size_t{384}),
                            y, std::size_t{384}).vertexOffsets = offsets;
                        using BaseBinding =
                            nicxlive::core::param::ParameterBindingImpl<nicxlive::core::param::DeformSlot>;
                        detail::actionArrayValue(
                            detail::actionArrayValue(deform->BaseBinding::getIsSet(), x, std::size_t{385}),
                            y, std::size_t{385}) = false;
                    } else {
                        Vec2Array reordered(offsets.size());
                        for (std::size_t oldIndex = 0; oldIndex < remap.size(); ++oldIndex)
                            reordered[remap[oldIndex]] = offsets[oldIndex].toVector();
                        detail::actionArrayValue(
                            detail::actionArrayValue(deform->values, x, std::size_t{391}),
                            y, std::size_t{391}).vertexOffsets = reordered;
                    }
                    if (recordActions) {
                        auto action = parameter::ngCreateParameterBindingsAction<
                            parameter::ParameterChangeBindingsValueAction>(
                            compat::RetainedString::fromStaticLiteral("Reorder Deformation"), param,
                            parameter::Bindings{deform},
                            std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(x)),
                            std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(y)));
                        bindingReorderActions.append(action); action->updateNewState();
                    }
                    needsReInterpolate = true;
                }
            }
            if (needsReInterpolate) deform->reInterpolate();
        }
    }
}
void NodeReplaceAction::captureDepthRigBindings() {
    const auto keepAlive = weak_from_this().lock();
    auto puppet = project::incActivePuppet(); if (!puppet) return;
    for (auto root : puppet->findNodesType<ext::nodes::ExDepthRigRoot>(puppet->root)) {
        if (root->findBindingIndex(detail::requireActionObject(srcNode)->getUUID()) < 0) continue;
        auto before = depthbone::ngCopyDepthRigBindings(root->bindings);
        depthbone::Bindings after;
        auto copied = depthbone::ngCopyDepthRigBindings(before);
        for (auto binding : copied) {
            if (binding.targetUuid == detail::requireActionObject(srcNode)->getUUID()) {
                if (std::dynamic_pointer_cast<GridDeformer>(toNode)) binding.targetKind = ext::nodes::ExDepthTargetKind::Grid;
                else if (std::dynamic_pointer_cast<PathDeformer>(toNode)) binding.targetKind = ext::nodes::ExDepthTargetKind::Path;
                else continue;
                binding.targetUuid = detail::requireActionObject(toNode)->getUUID();
            }
            after.append(binding);
        }
        depthRigBindingReplacements.append(DepthRigBindingReplacement{root, before, after});
    }
}
void NodeReplaceAction::applyDepthRigBindings(bool forward) {
    const auto keepAlive = weak_from_this().lock();
    auto replacements = depthRigBindingReplacements;
    for (auto& change : replacements) {
        {
            // The selected Source lowering captures this field receiver before the copy.
            const auto receiver = change.root;
            const auto bindings = depthbone::ngCopyDepthRigBindings(forward ? change.after : change.before);
            detail::requireActionObject(receiver)->bindings = bindings;
        }
        detail::requireActionObject(change.root)->notifyChange(change.root, NotifyReason::AttributeChanged);
        depthboneinvalidation::ngNotifyDepthBoneRigChanged(change.root,
            compat::RetainedString::fromStaticLiteral("Node Type Conversion"));
    }
}
float NodeReplaceAction::nonZeroScale(float value) { return std::abs(value) < 0.0001f ? 1.0f : value; }
Transform NodeReplaceAction::rootTransformFor() {
    auto puppet = project::incActivePuppet();
    return puppet && puppet->root ? puppet->root->localTransform : Transform(Vec3{0, 0, 0});
}
void NodeReplaceAction::restoreWorldTransformUnder(NodeRef child, NodeRef parent, Transform world) {
    const auto parentTransform = detail::requireActionObject(child)->lockToRootValue() ? rootTransformFor() :
        (parent ? parent->transform() : Transform(Vec3{0, 0, 0}));
    const auto translation = Node::getRelativePosition(parentTransform.matrix(), world.matrix());
    const Vec3 rotation{world.rotation.x - parentTransform.rotation.x,
        world.rotation.y - parentTransform.rotation.y, world.rotation.z - parentTransform.rotation.z};
    const Vec2 scale{world.scale.x / nonZeroScale(parentTransform.scale.x), world.scale.y / nonZeroScale(parentTransform.scale.y)};
    const Vec3 translationOffset{
        child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.t.x")),
        child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.t.y")),
        child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.t.z"))};
    child->localTransform.translation = Vec3{translation.x - translationOffset.x,
        translation.y - translationOffset.y, translation.z - translationOffset.z};
    const Vec3 rotationOffset{
        child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.r.x")),
        child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.r.y")),
        child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.r.z"))};
    child->localTransform.rotation = Vec3{rotation.x - rotationOffset.x,
        rotation.y - rotationOffset.y, rotation.z - rotationOffset.z};
    child->localTransform.scale = Vec2{
        scale.x / nonZeroScale(child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.s.x"))),
        scale.y / nonZeroScale(child->getValueSource(compat::RetainedString::fromStaticLiteral("transform.s.y")))};
    child->localTransform.update(); child->transformChanged();
}
void NodeReplaceAction::applyReorderOps(bool forward) {
    const auto keepAlive = weak_from_this().lock();
    const auto actions = bindingReorderActions;
    for (auto action : actions) {
        if (forward) detail::requireActionObject(action)->redo();
        else detail::requireActionObject(action)->rollback();
    }
}
NodeReplaceAction::NodeReplaceAction(FieldInitializationOnly) {}

std::shared_ptr<NodeReplaceAction> NodeReplaceAction::create(NodeRef src, NodeRef to, bool copy) {
    auto result = detail::createNodeAction<NodeReplaceAction>([](auto* storage) {
        ::new (static_cast<void*>(storage)) NodeReplaceAction(FieldInitializationOnly{});
    });
    result->initialize(src, to, copy);
    return result;
}

void NodeReplaceAction::initialize(NodeRef src, NodeRef to, bool copy) {
    prepareSourceClassInfo();
    srcNode = src;
    toNode = to;
    if (std::dynamic_pointer_cast<Projectable>(srcNode) && !std::dynamic_pointer_cast<Projectable>(toNode) &&
        std::dynamic_pointer_cast<Part>(toNode)) copy = false;
    deepCopy = copy;
    if (!detail::requireActionObject(srcNode)->childrenList().empty()) {
        const auto sourceChildren = detail::requireActionObject(srcNode)->childrenList();
        children = Nodes(sourceChildren.begin(), sourceChildren.end());
    }
    const auto sourceName = detail::requireActionObject(src)->getName();
    descrName = detail::nodeActionStringAllocation([&] { return compat::RetainedString(sourceName); });
    captureDepthRigBindings();
    if (!detail::requireActionObject(toNode)->parentPtr()) redo(); else applyDepthRigBindings(true);
    updateParameterBindings(true);
}
void NodeReplaceAction::rollback() {
    const auto keepAlive = weak_from_this().lock();
    const auto parent = detail::requireActionObject(toNode)->parentPtr();
#ifndef NDEBUG
    if (!parent) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Assertion failure"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/node.d"), 549);
#endif
    std::uint64_t offset = std::numeric_limits<std::uint64_t>::max();
    {
        const auto siblings = detail::requireActionObject(parent)->childrenList();
        const auto needle = toNode;
        for (std::size_t i = 0; i < siblings.size(); ++i) {
            if (nicxlive::core::ngResourceObjectEquals(siblings[i], needle)) {
                offset = static_cast<std::uint64_t>(i);
                break;
            }
        }
    }
    const auto transform = detail::requireActionObject(toNode)->localTransform;
    compat::Slice<Transform> worlds;
    if (!children.isNull()) {
        const auto values = children;
        worlds.resize(values.size());
        for (std::size_t i = 0; i < values.size(); ++i)
            worlds[i] = detail::requireActionObject(values[i])->transform();
    }
    detail::requireActionObject(toNode)->reparent({}, 0);
    detail::requireActionObject(srcNode)->reparent(parent, offset, true);
    detail::requireActionObject(srcNode)->localTransform = transform;
    detail::requireActionObject(srcNode)->transformChanged();
    updateParameterBindings(false);
    applyReorderOps(false);
    if (deepCopy && !children.isNull()) {
        const auto values = children;
        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto child = values[i];
            const auto nextParent = srcNode;
            detail::requireActionObject(child)->reparent(nextParent, i, true);
            const auto restoreParent = srcNode;
            const auto world = detail::actionArrayValue(worlds, i, std::size_t{562});
            restoreWorldTransformUnder(child, restoreParent, world);
            detail::requireActionObject(child)->notifyChange(child, NotifyReason::StructureChanged);
        }
    } else if (!children.isNull()) {
        const auto values = children;
        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto child = values[i];
            const auto restoreParent = detail::requireActionObject(child)->parentPtr();
            const auto world = detail::actionArrayValue(worlds, i, std::size_t{567});
            restoreWorldTransformUnder(child, restoreParent, world);
            detail::requireActionObject(child)->notifyChange(child, NotifyReason::StructureChanged);
        }
        const auto receiver = detail::requireActionObject(srcNode);
        const auto target = srcNode;
        receiver->notifyChange(target, NotifyReason::StructureChanged);
    } else {
        const auto receiver = detail::requireActionObject(srcNode);
        const auto target = srcNode;
        receiver->notifyChange(target, NotifyReason::StructureChanged);
    }
    applyDepthRigBindings(false);
    if (project::incNodeInSelection(toNode)) {
        project::incRemoveSelectNode(toNode);
        project::incAddSelectNode(srcNode);
    }
}
void NodeReplaceAction::redo() {
    const auto keepAlive = weak_from_this().lock();
    const auto parent = detail::requireActionObject(srcNode)->parentPtr();
#ifndef NDEBUG
    if (!parent) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Assertion failure"),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/actions/node.d"), 586);
#endif
    std::uint64_t offset = std::numeric_limits<std::uint64_t>::max();
    {
        const auto siblings = detail::requireActionObject(parent)->childrenList();
        const auto needle = srcNode;
        for (std::size_t i = 0; i < siblings.size(); ++i) {
            if (nicxlive::core::ngResourceObjectEquals(siblings[i], needle)) {
                offset = static_cast<std::uint64_t>(i);
                break;
            }
        }
    }
    const auto transform = detail::requireActionObject(srcNode)->localTransform;
    compat::Slice<Transform> worlds;
    if (!children.isNull()) {
        const auto values = children;
        worlds.resize(values.size());
        for (std::size_t i = 0; i < values.size(); ++i)
            worlds[i] = detail::requireActionObject(values[i])->transform();
    }
    detail::requireActionObject(srcNode)->reparent({}, 0);
    detail::requireActionObject(toNode)->reparent(parent, offset, true);
    detail::requireActionObject(toNode)->localTransform = transform;
    detail::requireActionObject(toNode)->transformChanged();
    updateParameterBindings(false);
    applyReorderOps(true);
    if (deepCopy && !children.isNull()) {
        const auto values = children;
        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto child = values[i];
            const auto nextParent = toNode;
            detail::requireActionObject(child)->reparent(nextParent, i, true);
            const auto restoreParent = toNode;
            const auto world = detail::actionArrayValue(worlds, i, std::size_t{599});
            restoreWorldTransformUnder(child, restoreParent, world);
            detail::requireActionObject(child)->notifyChange(child, NotifyReason::StructureChanged);
        }
    } else if (!children.isNull()) {
        const auto values = children;
        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto child = values[i];
            const auto restoreParent = detail::requireActionObject(child)->parentPtr();
            const auto world = detail::actionArrayValue(worlds, i, std::size_t{604});
            restoreWorldTransformUnder(child, restoreParent, world);
            detail::requireActionObject(child)->notifyChange(child, NotifyReason::StructureChanged);
        }
        const auto receiver = detail::requireActionObject(toNode);
        const auto target = toNode;
        receiver->notifyChange(target, NotifyReason::StructureChanged);
    } else {
        const auto receiver = detail::requireActionObject(toNode);
        const auto target = toNode;
        receiver->notifyChange(target, NotifyReason::StructureChanged);
    }
    applyDepthRigBindings(true);
    if (project::incNodeInSelection(srcNode)) {
        project::incRemoveSelectNode(srcNode);
        project::incAddSelectNode(toNode);
    }
}
ActionText NodeReplaceAction::describe() {
    const auto keepAlive = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Change type of %s to %s"));
    const auto name = descrName;
    const auto type = detail::requireActionObject(toNode)->typeId();
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(name), core::translation::ngFormatArgument(type)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
ActionText NodeReplaceAction::describeUndo() {
    const auto keepAlive = weak_from_this().lock();
    const auto pattern = core::translation::ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Revert type of %s to %s"));
    const auto name = descrName;
    const auto type = detail::requireActionObject(srcNode)->typeId();
    const std::array<core::translation::TypedFormatArgument, 2> arguments{
        core::translation::ngFormatArgument(name), core::translation::ngFormatArgument(type)};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), arguments);
    return detail::nodeActionStringAllocation([&] {
        return formatted.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(formatted));
    });
}
ActionText NodeReplaceAction::getName() { return compat::RetainedString::fromStaticLiteral("this"); }

void incMoveChildrenWithHistory(Nodes nodes, NodeRef to, std::size_t offset) {
    core::actionstack::incActionPush(detail::createNodeAction<NodeMoveAction>([&](auto* storage) {
        std::construct_at(storage, nodes, to, offset);
    }));
}
void incMoveChildWithHistory(NodeRef node, NodeRef to, std::size_t offset) { incMoveChildrenWithHistory(Nodes{node}, to, offset); }
void incAddChildWithHistory(NodeRef node, NodeRef to, ActionText name) {
    if (!to) to = detail::requireActionObject(project::incActivePuppet())->root;
    auto action = detail::createNodeAction<NodeMoveAction>([&](auto* storage) {
        std::construct_at(storage, Nodes{node}, to);
    });
    core::actionstack::incActionPush(action);
    const auto child = detail::requireActionObject(node);
    child->insertInto(to, Node::OFFSET_START);
    child->localTransform.clear();
    {
        auto& snapshots = action->newTransform;
        const auto key = child->getUUID();
        const auto& value = child->localTransform;
        auto destination = snapshots[key];
        destination = value;
    }
    {
        auto& snapshots = action->newRelativeSort;
        const auto key = child->getUUID();
        const auto value = child->relZSort();
        auto destination = snapshots[key];
        destination = value;
    }
    if (name.isNull()) {
        const auto prefix = core::translation::ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Unnamed "));
        const auto type = child->typeId();
        const auto suffix = core::translation::ngTranslateRetained(compat::RetainedString(type));
        const auto label = [&] {
            try {
                std::string combined(prefix.view());
                combined.append(suffix.view());
                return combined.empty() ? compat::RetainedString{} : compat::RetainedString(std::move(combined));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        }();
        child->setName(ext::nodes::ngRetainNodeName(label));
    } else child->setName(ext::nodes::ngRetainNodeName(name));
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
}
std::shared_ptr<GroupAction> incDeleteMaskOfNode(NodeRef node, std::shared_ptr<GroupAction> group) {
    auto drawables = findPuppetNodes<Drawable>(project::incActivePuppet(), node);
    auto parts = findCurrentPuppetNodes<Part>();
    for (auto drawable : drawables) for (auto target : parts) {
        const auto idx = detail::requireActionObject(target)->getMaskIdx(drawable);
        if (idx >= 0) {
            if (!group) group = detail::createNodeAction<GroupAction>([](auto* storage) {
                std::construct_at(storage);
            });
            group->addAction(detail::createNodeAction<PartRemoveMaskAction>([&](auto* storage) {
                std::construct_at(storage, drawable, target,
                    detail::actionArrayValue(detail::requireActionObject(target)->masks,
                        static_cast<std::size_t>(idx), std::size_t{1164}).mode);
            }));
        }
    }
    return group;
}
std::shared_ptr<GroupAction> incDeleteWeldedLinksOfNode(NodeRef node, std::shared_ptr<GroupAction> group) {
    auto drawables = findPuppetNodes<Drawable>(project::incActivePuppet(), node);
    auto parts = findCurrentPuppetNodes<Part>();
    for (auto drawable : drawables) for (auto target : parts) {
        if (detail::requireActionObject(target)->isWeldedBy(drawable)) {
            if (!group) group = detail::createNodeAction<GroupAction>([](auto* storage) {
                std::construct_at(storage);
            });
            group->addAction(detail::createNodeAction<DrawableRemoveWeldingAction>([&](auto* storage) {
                std::construct_at(storage, drawable, target);
            }));
        }
    }
    return group;
}
std::shared_ptr<GroupAction> ngDeleteDepthRigReferencesOfNode(NodeRef node, std::shared_ptr<GroupAction> group) {
    if (!node || !project::incActivePuppet()) return group;
    struct UuidHash {
        std::size_t operator()(std::uint64_t value) const noexcept { return static_cast<std::size_t>(value); }
    };
    nicxlive::core::common::ResourceKeyMap<std::uint64_t, bool, UuidHash,
        std::equal_to<std::uint64_t>> removed;
    // The source inserts each UUID before evaluating and recursing through its children.
    const auto collectRemovedUuids = [&removed](auto&& recurse, NodeRef current) -> void {
        removed[detail::requireActionObject(current)->getUUID()] = true;
        const auto children = detail::requireActionObject(current)->childrenList();
        for (auto child : children) recurse(recurse, child);
    };
    collectRemovedUuids(collectRemovedUuids, node);
    for (auto root : findCurrentPuppetNodes<ext::nodes::ExDepthRigRoot>()) {
        if (removed.contains(detail::requireActionObject(root)->getUUID())) continue;
        auto oldBindings = depthbone::ngCopyDepthRigBindings(root->bindings);
        depthbone::Bindings newBindings; bool changed = false;
        auto bindings = root->bindings;
        for (auto binding : bindings) {
            if (removed.contains(binding.targetUuid)) { changed = true; continue; }
            compat::Slice<std::uint64_t> kept; bool removedSource = false;
            for (auto uuid : binding.sourceBoneUuids) {
                if (removed.contains(uuid)) { changed = true; removedSource = true; continue; }
                kept.append(uuid);
            }
            if (removedSource && kept.empty()) continue;
            if (removedSource) {
                binding.sourceBoneUuids = kept;
                for (const auto& entry : removed) binding.influenceRule.multipliersByBoneUuid.remove(entry.first);
                binding.normalizeSourceSettings();
            }
            newBindings.append(binding);
        }
        if (changed) {
            root->bindings = newBindings;
            if (!group) group = detail::createNodeAction<GroupAction>([](auto* storage) {
                std::construct_at(storage);
            });
            group->addAction(detail::createNodeAction<depthbone::DepthBoneSourceListChangeAction>([&](auto* storage) {
                const auto label = core::translation::ngTranslateRetained(
                    compat::RetainedString::fromStaticLiteral("Remove deleted Depth Rig bindings"));
                std::construct_at(storage, label, root, oldBindings, root->bindings);
            }));
        }
    }
    return group;
}
void incDeleteChildWithHistory(NodeRef node) {
    auto group = incDeleteMaskOfNode(node); group = incDeleteWeldedLinksOfNode(node, group);
    group = ngDeleteDepthRigReferencesOfNode(node, group);
    auto action = detail::createNodeAction<NodeMoveAction>([&](auto* storage) {
        std::construct_at(storage, Nodes{node}, NodeRef{});
    });
    if (group) { group->addAction(action); core::actionstack::incActionPush(group); }
    else core::actionstack::incActionPush(action);
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
}
void incDeleteChildrenWithHistory(Nodes nodes) {
    std::shared_ptr<GroupAction> group;
    for (auto node : nodes) { group = incDeleteMaskOfNode(node, group); group = incDeleteWeldedLinksOfNode(node, group);
        group = ngDeleteDepthRigReferencesOfNode(node, group); }
    auto action = detail::createNodeAction<NodeMoveAction>([&](auto* storage) {
        std::construct_at(storage, nodes, NodeRef{});
    });
    if (group) { group->addAction(action); core::actionstack::incActionPush(group); }
    else core::actionstack::incActionPush(action);
    detail::requireActionObject(project::incActivePuppet())->rescanNodes();
}
void incLockToRootNode(NodeRef node) {
    core::actionstack::incActionPush(detail::createNodeAction<NodeRootBaseSetAction>([&](auto* storage) {
        const auto locked = detail::requireActionObject(node)->lockToRootValue();
        std::construct_at(storage, node, !locked);
    }));
}
}
