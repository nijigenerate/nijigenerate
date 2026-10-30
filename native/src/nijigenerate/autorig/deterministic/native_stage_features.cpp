#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/commands/node/node_commands.hpp>
#include <nijigenerate/commands/automesh/dynamic.hpp>
#include <nijigenerate/viewport/vertex/automesh/grid.hpp>
#include <nodes/projectable.hpp>
#include <limits>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
Json prepareFeatureComposites(Json state, Json program, Task task) {
    framework::Table<compat::Slice<std::uint32_t>> groups;
    NativeIntegerMap<std::uint64_t, framework::Text> sides;
    for (auto carrier : program.sourceIndexValue("carriers").sourceArrayValue()) {
        const auto key = ngRigUnsigned(carrier.sourceIndexValue("part"));
        const auto value = carrier.sourceIndexValue("side").as_string().retainedString();
        sides[key]=value;
    }
    for (auto material : state.sourceIndexValue("materials").sourceArrayValue()) {
        if (material.sourceIndexValue("static").as_bool()) continue;
        const auto feature=ngRigString(material,stringLiteralEvidence("feature"),stringLiteralEvidence(""));
        if (feature.view().starts_with("mouth"))
            groups[compat::RetainedString::fromStaticLiteral("Mouth")].append(uuid(material.sourceIndexValue("uuid")));
        else if (feature.view()=="sclera"||feature.view()=="iris"||feature.view()=="upper"||feature.view()=="lower"||
            feature.view()=="corner"||feature.view()=="fold")
            groups[nativeText({"Eye::",readNativeMap(sides,ngRigUnsigned(
                material.sourceIndexValue("uuid")),748).view()})].append(uuid(material.sourceIndexValue("uuid")));
    }
    core::settings::SettingsJsonArray composites;
    NativeIntegerMap<std::uint32_t, bool> meshed;
    for (auto [name, ids] : groups) {
        compat::Slice<std::uint32_t> targets;
        task->runOnMainThread([&] {
            auto puppet = project::incActivePuppet();
            auto first = puppet->findNodeById(ids[0]);
            NodeRef commonComposite;
            for (auto candidate = first->parentPtr(); candidate && candidate != puppet->root; candidate = candidate->parentPtr()) {
                if (candidate->typeId() != "DynamicComposite") continue;
                bool featureGroup = false;
                for (auto source : state.sourceIndexValue("prepared_groups").sourceArrayValue())
                    if (uuid(source.sourceIndexValue("uuid")) == candidate->getUUID() &&
                        source.sourceIndexValue("feature").as_string().view() == (name.view()=="Mouth" ? "mouth" : "eye")) featureGroup = true;
                if (!featureGroup) continue;
                bool common = true;
                for (auto id : ids) {
                    bool contained = false;
                    for (auto cursor = puppet->findNodeById(id); cursor; cursor = cursor->parentPtr())
                        if (cursor == candidate) contained = true;
                    common = common && contained;
                }
                if (common) { commonComposite = candidate; break; }
            }
            if (commonComposite) targets.append(commonComposite->getUUID());
            else {
                NativeIntegerMap<std::uint32_t, compat::Slice<std::uint32_t>> partitions;
                for (auto id : ids) partitions[puppet->findNodeById(id)->parentPtr()->getUUID()].append(id);
                for (auto [parentId, parts] : partitions) {
                    auto parent = puppet->findNodeById(parentId);
                    using namespace commands::node::node;
                    auto composite = createSourceNode(
                        [&] {
                            return createNativeObject<AddNodeCommandT<true>>(compat::RetainedString::fromStaticLiteral("DynamicComposite"),
                                compat::RetainedString::fromStaticLiteral("::Mechanism"));
                        },
                        [&] {
                            return editorContext({parent});
                        });
                    composite->setName(nativeText({name.view(),"::Composite"}).toNullableString());
                    compat::Slice<NodeRef> selected;
                    for (auto id : parts) selected.append(puppet->findNodeById(id));
                    commandSource(
                        [&] {
                            return createNativeObject<MoveNodeCommand>(composite, 0, true);
                        },
                        [&] {
                            return editorContext(selected);
                        });
                    targets.append(composite->getUUID());
                }
            }
            puppet->root->build(); puppet->update();
        });
        const double infinity = std::numeric_limits<double>::infinity();
        std::array<double, 4> bounds{infinity, infinity, -infinity, -infinity};
        for (auto material : state.sourceIndexValue("materials").sourceArrayValue()) {
            if (std::find(ids.begin(), ids.end(), uuid(material.sourceIndexValue("uuid"))) == ids.end()) continue;
            for (auto p : ngRigPoints(material.sourceIndexValue("cloud"))) {
                bounds[0] = dmin(bounds[0], p[0]); bounds[1] = dmin(bounds[1], p[1]);
                bounds[2] = dmax(bounds[2], p[0]); bounds[3] = dmax(bounds[3], p[1]);
            }
        }
        const double width = bounds[2] - bounds[0], height = dmax(1., bounds[3] - bounds[1]);
        const float margin = static_cast<float>(name.view()=="Mouth" ? dmax(.25, .135 * width / height) :
            dmax(.18, 1 + .08 * width / height));
        for (auto id : targets) {
            if (meshed.contains(id)) continue;
            ngRigCheckpoint(task);
            using Grid = viewport::vertex::automesh::grid::GridAutoMeshProcessor;
            auto processor = createNativeObject<Grid>();
            processor->maskThreshold = 1; processor->xSegments = 10; processor->ySegments = 10; processor->margin = margin;
            processor->ngPostParamWrite(compat::RetainedString::fromStaticLiteral("margin"));
            compat::SharedDelegate<bool()> canceled = [task] { return task->isCanceled(); };
            std::shared_ptr<commands::CommandResult> result;
            task->runOnMainThread([&] {
                result = commandSource(
                    [&] {
                        return createNativeObject<commands::automesh::dynamic::ApplyAutoMeshPT<Grid>>( processor, canceled);
                    },
                    [&] {
                        return editorContext({project::incActivePuppet()->findNodeById(id)});
                    });
            });
            result = result->waitForCompletion(); enforceNativeLazy(result->succeeded,[&] { return result->message; },807);
            task->runOnMainThread([&] {
                auto composite = std::dynamic_pointer_cast<nicxlive::core::nodes::Projectable>(
                    project::incActivePuppet()->findNodeById(id));
                enforceNative(composite && !composite->autoResizedMesh,"Feature composite did not retain its native generated mesh",810);
                core::settings::SettingsJsonArray parts;
                for (auto part : ids) parts.append(Json(part));
                composites.append(objectEvidence({{stringLiteralEvidence("uuid"), id}, {stringLiteralEvidence("name"),core::settings::SettingsString(name)}, {stringLiteralEvidence("parts"), Json(parts)},
                    {stringLiteralEvidence("mapping"), textureMapping(composite)}, {stringLiteralEvidence("margin"), margin}, {stringLiteralEvidence("segments"), 10}}));
            });
            meshed[id]=true;
        }
    }
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("feature_composites")) = Json(composites);
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("composite_source_uv_readback")) = verifySourceFrames(state, task);
    return state;
}
}
