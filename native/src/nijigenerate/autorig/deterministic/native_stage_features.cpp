#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/commands/node/node_commands.hpp>
#include <nodes/projectable.hpp>
#include <cmath>

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
            NativeIntegerMap<std::uint32_t, compat::Slice<std::uint32_t>> partitions;
            for (auto id : ids) {
                const auto part = puppet->findNodeById(id);
                NodeRef surface;
                for (auto parent = part->parentPtr(); parent && parent != puppet->root; parent = parent->parentPtr())
                    if (parent->typeId() == "Composite" || parent->typeId() == "DynamicComposite")
                        surface = parent;
                // A feature may span several parents, for example when lashes
                // and the clipped iris live in different PSD groups.
                if (!surface) partitions[part->parentPtr()->getUUID()].append(id);
                else if (surface->typeId() == "DynamicComposite") targets.append(surface->getUUID());
            }
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
            puppet->root->build(); puppet->update();
        });
        for (auto id : targets) {
            if (meshed.contains(id)) continue;
            ngRigCheckpoint(task);
            task->runOnMainThread([&] {
                auto composite = std::dynamic_pointer_cast<nicxlive::core::nodes::Projectable>(
                    project::incActivePuppet()->findNodeById(id));
                enforceNative(bool(composite), "Missing feature composite", 807);
                // The auto-sized target already covers its children at neutral.
                // Subdivide it in local space; transient GPU alpha is not input art.
                if (composite->autoResizedMesh) composite->createSimpleMesh();
                const auto& source = composite->getMesh();
                enforceNative(!source.vertices.empty(), "Feature composite has no neutral extent", 808);
                auto lo = source.vertices.at(0), hi = lo;
                for (const auto p : source.vertices) {
                    lo.x = std::min(lo.x, p.x); lo.y = std::min(lo.y, p.y);
                    hi.x = std::max(hi.x, p.x); hi.y = std::max(hi.y, p.y);
                }
                enforceNative(hi.x > lo.x && hi.y > lo.y, "Feature composite extent is degenerate", 809);
                nicxlive::core::nodes::MeshData mesh;
                mesh.origin = source.origin;
                mesh.gridAxes.resize(2);
                constexpr unsigned segments = 10;
                for (unsigned i = 0; i <= segments; ++i) {
                    const auto t = static_cast<float>(i) / segments;
                    mesh.gridAxes[0].push_back(std::lerp(lo.y, hi.y, t));
                    mesh.gridAxes[1].push_back(std::lerp(lo.x, hi.x, t));
                }
                enforceNative(mesh.regenerateGrid(), "Feature grid could not be generated", 810);
                composite->autoResizedMesh = false;
                composite->normalizeUV(&mesh);
                composite->rebuffer(mesh);
                enforceNative(!composite->autoResizedMesh && composite->getMesh().isGrid(),
                    "Feature composite did not retain a fixed grid", 811);
                core::settings::SettingsJsonArray parts;
                for (auto part : ids)
                    for (auto parent = project::incActivePuppet()->findNodeById(part)->parentPtr();
                         parent; parent = parent->parentPtr())
                        if (parent == composite) {
                            parts.append(Json(part));
                            break;
                        }
                composites.append(objectEvidence({{stringLiteralEvidence("uuid"), id}, {stringLiteralEvidence("name"),core::settings::SettingsString(name)}, {stringLiteralEvidence("parts"), Json(parts)},
                    {stringLiteralEvidence("mapping"), textureMapping(composite)}, {stringLiteralEvidence("segments"), segments}}));
            });
            meshed[id]=true;
        }
    }
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("feature_composites")) = Json(composites);
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("composite_source_uv_readback")) = verifySourceFrames(state, task);
    return state;
}
}
