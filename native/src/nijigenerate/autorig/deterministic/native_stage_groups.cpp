#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/commands/node/node_commands.hpp>
#include <nijigenerate/commands/automesh/dynamic.hpp>
#include <nijigenerate/viewport/vertex/automesh/grid.hpp>
#include <nodes/projectable.hpp>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
namespace {
bool sameTransform(const nicxlive::core::math::Transform& a, const nicxlive::core::math::Transform& b) {
    // Original Transform has structural equality, including its cached matrix.
    if (a.translation.x != b.translation.x || a.translation.y != b.translation.y || a.translation.z != b.translation.z ||
        a.rotation.x != b.rotation.x || a.rotation.y != b.rotation.y || a.rotation.z != b.rotation.z ||
        a.scale.x != b.scale.x || a.scale.y != b.scale.y || a.pixelSnap != b.pixelSnap) return false;
    for (std::size_t row = 0; row < 4; ++row)
        for (std::size_t column = 0; column < 4; ++column)
            if (a.trs.a.a[row][column] != b.trs.a.a[row][column]) return false;
    return true;
}
}
Json prepareSourceGroups(Json state, Task task) {
    using namespace commands::node::node;
    NativeIntegerMap<std::uint32_t, bool> excluded;
    core::settings::SettingsJsonArray removed;
    task->runOnMainThread([&] {
        auto puppet = project::incActivePuppet();
        compat::Slice<NodeRef> hidden;
        const auto collect = [&](auto&& self, NodeRef node) -> void {
            bool disabled = !node->getEnabled();
            if (auto drawable = std::dynamic_pointer_cast<nicxlive::core::nodes::Projectable>(node))
                disabled = disabled || drawable->opacity == 0;
            if (node != puppet->root && disabled) { hidden.append(node); return; }
            for (auto childRange = node->childrenList(); auto child : childRange) self(self,child);
        };
        const auto record = [&](auto&& self, NodeRef node) -> void {
            excluded[node->getUUID()]=true;
            for (auto childRange = node->childrenList(); auto child : childRange) self(self,child);
        };
        collect(collect,puppet->root);
        for (auto node : hidden) {
            record(record,node);
            removed.append(objectEvidence({{stringLiteralEvidence("uuid"), node->getUUID()}, {stringLiteralEvidence("name"), node->getName()}}));
        }
        if (!hidden.empty()) commandSource(
            [&] {
                return createNativeObject<DeleteNodeCommand>(true);
            },
            [&] {
                return editorContext(hidden);
            });
    });
    core::settings::SettingsJsonArray materials, groups;
    for (auto material : state.sourceIndexValue("materials").sourceArrayValue())
        if (!excluded.contains(uuid(material.sourceIndexValue("uuid")))) materials.append(material);
    for (auto group : state.sourceIndexValue("groups").sourceArrayValue())
        if (!excluded.contains(uuid(group.sourceIndexValue("uuid")))) groups.append(group);
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("materials")) = Json(materials); state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("groups")) = Json(groups);
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("excluded_hidden_sources")) = Json(removed);
    auto sourceGroups = state.sourceObjectRef().if_contains("groups");
    core::settings::SettingsJsonArray prepared;
    if (!sourceGroups) return state;
    // Hold the shared array descriptor, not a pointer into an object that may grow.
    const auto originalGroups = sourceGroups->as_array();
    for (auto group : originalGroups) {
        ngRigCheckpoint(task);
        bool hasMaterial = false, onlyEyes = true, onlyMouth = true;
        for (auto material : state.sourceIndexValue("materials").sourceArrayValue()) {
            if (!material.sourceIndexValue("active").as_bool()) continue;
            auto ancestors = material.sourceObjectRef().if_contains("ancestor_ids");
            if (!ancestors) continue;
            bool belongs = false;
            for (auto ancestor : ancestors->as_array()) if (ancestor == group.sourceIndexValue("uuid")) belongs = true;
            if (!belongs) continue;
            hasMaterial = true;
            auto feature = stringGet(material, "feature");
            bool eye = false;
            for (std::string_view candidate : {"sclera", "iris", "upper", "lower", "corner", "fold", "brow"})
                if (candidate == feature) eye = true;
            onlyEyes = onlyEyes && eye; onlyMouth = onlyMouth && feature.starts_with("mouth");
        }
        const auto desired = hasMaterial && (onlyEyes || onlyMouth) ?
            compat::RetainedString::fromStaticLiteral("DynamicComposite") :
            get(group, "clipping_surface", false).as_bool() ? compat::RetainedString::fromStaticLiteral("Composite") :
                compat::RetainedString::fromStaticLiteral("GridDeformer");
        task->runOnMainThread([&] {
            auto node = project::incActivePuppet()->findNodeById(uuid(group.sourceIndexValue("uuid")));
            enforceNative(node && compat::ngRequireObject(node->parentPtr())->getUUID() == uuid(group.sourceIndexValue("parent")),
                "Source group hierarchy changed",602);
            const auto before = node->localTransform;
            NativeVector<std::uint32_t> children;
            for (auto childRange = node->childrenList(); auto child : childRange) children.push_back(child->getUUID());
            if (node->typeId().view() != desired.view()) {
                if (node->typeId() != "Node" && desired.view() == "GridDeformer")
                    node = createSourceNode(
                        [&] {
                            return createNativeObject<ConvertToCommandT<true>>(compat::RetainedString::fromStaticLiteral("Node"));
                        },
                        [&] {
                            return editorContext({node});
                        });
                node = createSourceNode(
                    [&] {
                        return createNativeObject<ConvertToCommandT<true>>(desired);
                    },
                    [&] {
                        return editorContext({node});
                    });
            }
            enforceNative(compat::ngRequireObject(node)->getUUID() == uuid(group.sourceIndexValue("uuid")) &&
                compat::ngRequireObject(node->parentPtr())->getUUID() == uuid(group.sourceIndexValue("parent")) &&
                sameTransform(node->localTransform,before),"Source group conversion changed identity or transform",611);
            NativeVector<std::uint32_t> after;
            for (auto childRange = node->childrenList(); auto child : childRange) after.push_back(child->getUUID());
            enforceNative(children == after,"Source group conversion changed child order",614);
        });
        prepared.append(objectEvidence({{stringLiteralEvidence("uuid"), group.sourceIndexValue("uuid")},
            {stringLiteralEvidence("type"),core::settings::SettingsString(desired)}, {stringLiteralEvidence("active"), hasMaterial},
            {stringLiteralEvidence("feature"), hasMaterial && onlyEyes ? "eye" : hasMaterial && onlyMouth ? "mouth" : ""}}));
    }
    for (std::size_t i = prepared.size(); i > 0; --i) {
        auto group = prepared[i - 1];
        if (!group.sourceIndexValue("active").as_bool() || group.sourceIndexValue("type").as_string().view() != "GridDeformer") continue;
        using Grid = viewport::vertex::automesh::grid::GridAutoMeshProcessor;
        auto processor = createNativeObject<Grid>();
        processor->maskThreshold = 1; processor->margin = 0; processor->xSegments = 10; processor->ySegments = 10;
        processor->scaleX = {}; processor->scaleY = {};
        for (unsigned j = 0; j < 11; ++j) { processor->scaleX.append(j / 10.0f); processor->scaleY.append(j / 10.0f); }
        compat::SharedDelegate<bool()> canceled = [task] { return task->isCanceled(); };
        std::shared_ptr<commands::CommandResult> result;
        task->runOnMainThread([&] {
            auto node = project::incActivePuppet()->findNodeById(uuid(group.sourceIndexValue("uuid")));
            result = commandSource(
                [&] {
                    return createNativeObject<commands::automesh::dynamic::ApplyAutoMeshPT<Grid>>( processor, canceled);
                },
                [&] {
                    return editorContext({node});
                });
        });
        result = result->waitForCompletion(); enforceNativeLazy(result->succeeded,[&] { return result->message; },631);
    }
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("prepared_groups")) = Json(prepared);
    return state;
}
}
