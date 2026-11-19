#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "controls.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/commands/model/set_deform_binding_commands.hpp>
#include <nijigenerate/commands/node/node_commands.hpp>
#include <nodes/path_deformer.hpp>
#include <param/binding_impl.hpp>
#include <serde_json.hpp>
#include <algorithm>
#include <cmath>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
namespace {
compat::RetainedStringSlice skeletonDigest(NodeRef node) {
    return ngRigDigest(serializedJson(node));
}

}
Json applyFixedFootCorrections(Json state, Json feet, Task task) {
    if (task->targetScope() || task->eligibleTargets()) {
        NativeIntegerMap<std::uint32_t, bool> include, exclude;
        for (const auto& target : indexNative(state, "targets").sourceArrayValue()) {
            const auto grid = uuid(indexNative(target, "grid"));
            const auto part = uuid(indexNative(target, "part"));
            if (task->acceptsTarget(part)) include[grid] = true;
            else exclude[grid] = true;
        }
        core::settings::SettingsJsonArray accepted;
        NativeIntegerMap<std::uint32_t, bool> reported;
        for (const auto& operation : indexNative(feet, "operations").sourceArrayValue()) {
            const auto grid = uuid(indexNative(operation, "grid"));
            if (include.contains(grid) && !exclude.contains(grid)) accepted.append(operation);
            else if (include.contains(grid) && !reported.contains(grid)) {
                reported[grid] = true;
                task->recordUnit({{grid}, framework::AutoRigTaskState::Blocked,
                    "Fixed-foot correction requires the complete leg Grid"});
            }
        }
        feet = Json(feet.sourceObjectRef().duplicate());
        assignNative(feet, "operations", accepted);
    }
    if (const auto unavailable = feet.sourceObjectRef().if_contains("unavailable")) {
        auto accepted = core::settings::SettingsJsonArray{};
        for (const auto& item : unavailable->sourceArrayValue()) {
            const auto part = uuid(indexNative(item, "part"));
            if (!task->acceptsTarget(part)) continue;
            accepted.append(item);
            task->recordUnit({{part}, framework::AutoRigTaskState::Blocked, nativeString(indexNative(item, "reason"))});
        }
        feet = Json(feet.sourceObjectRef().duplicate());
        assignNative(feet, "unavailable", accepted);
    }
    NativeIntegerMap<std::uint32_t, bool> appliedGrids;
    for (auto operation : indexNative(feet, "operations").sourceArrayValue()) {
        ngRigCheckpoint(task);
        auto offsets = nativeNumbers(indexNative(operation, "values"));
        auto indices = ngRigNumbers(indexNative(operation, "key"));
        task->runOnMainThread([&] {
            auto puppet = project::incActivePuppet(); auto node = puppet->findNodeById(uuid(indexNative(operation, "grid")));
            auto parameter = parameterByName(puppet, nativeString(indexNative(operation, "parameter")));
            auto context = editorContext({node}); context->parameters({parameter});
            context->keyPoint({static_cast<std::uint32_t>(readNativeArray(indices,0,1394)),
                static_cast<std::uint32_t>(readNativeArray(indices,1,1394))});
            context->hasExplicitKeyPoint(true);
            commandSource(
                [&] {
                    return createNativeObject<commands::model::set_deform_binding::SetDeformBindingCommand>(
                        compat::RetainedString::fromStaticLiteral("deform"), offsets, true, false);
                },
                [&] {
                    return context;
                });
        });
        appliedGrids[uuid(indexNative(operation, "grid"))] = true;
    }
    for (const auto grid : appliedGrids.keys())
        task->recordUnit({{grid}, framework::AutoRigTaskState::Succeeded, {}});
    settleDepthRefresh(task);
    assignNative(state, "fixed_foot_corrections", feet);
    return state;
}
Json applyCheekCorrections(Json state, Json report, Task task) {
    if (!indexNative(report, "applicable").as_bool()) { assignNative(state, "shape_corrections", report); return state; }
    NativeIntegerMap<std::uint64_t, bool> bound;
    NativeIntegerMap<std::uint64_t, bool> ridgeFollowers;
    NativeIntegerMap<std::uint64_t, bool> coincidentSkin;
    NativeIntegerMap<std::uint64_t, bool> skinOutlines;
    for (auto operation : indexNative(report, "operations").sourceArrayValue())
        for (auto value : ngRigNumbers(indexNative(operation, "values")))
            if (value != 0) {
                const auto id = ngRigUnsigned(indexNative(operation,"part"));
                bound[id] = true;
                const auto kind = stringGet(operation,"kind","");
                if (kind=="hair_ridge" || kind=="skin_outline") ridgeFollowers[id] = true;
                if (kind=="coincident_skin") coincidentSkin[id] = true;
                if (kind=="skin_outline") skinOutlines[id] = true;
                break;
            }
    if (task->targetScope() || task->eligibleTargets()) {
        std::vector<std::uint32_t> ids;
        bool outside = false;
        for (auto id : bound.keys()) {
            ids.push_back(static_cast<std::uint32_t>(id));
            outside |= !task->acceptsTarget(static_cast<std::uint32_t>(id));
        }
        if (outside) {
            task->recordUnit({std::move(ids), framework::AutoRigTaskState::Blocked,
                "Face ridge correction requires the skin, outline and participating hair Parts"});
            return state;
        }
    }
    NativeIntegerMap<std::uint64_t, bool> priorBindings;
    if (task->targetScope()) if (const auto previous = state.sourceObjectRef().if_contains("shape_corrections"))
        if (const auto prior = previous->sourceObjectRef().if_contains("bound_targets"))
            for (const auto& part : prior->sourceArrayValue()) priorBindings[ngRigUnsigned(part)] = true;
    compat::RetainedStringSlice rootSignature;
    NativeIntegerMap<std::uint64_t, bool> pathDriven;
    std::uint32_t contourId = 0;
    core::settings::SettingsJsonArray contourSiblingRoots;
    task->runOnMainThread([&] {
        auto puppet = project::incActivePuppet(); auto parameter = parameterByName(puppet, "Face::Yaw-Pitch");
        enforceNative(bool(parameter),"Cheek correction parameter is missing",1410);
        const auto skinId = uuid(indexNative(report, "skin"));
        for (const auto id : coincidentSkin.keys()) {
            auto part = puppet->findNodeById(static_cast<std::uint32_t>(id));
            if (!priorBindings.contains(id) && std::ranges::any_of(puppet->parameters,
                    [&](const auto& control) { return bool(control->getBinding(part, "deform")); }))
                bound.erase(id);
        }
        for (auto [id, unused] : bound) {
            auto part = puppet->findNodeById(static_cast<std::uint32_t>(id));
            enforceNative((id == skinId || ridgeFollowers.contains(id) || coincidentSkin.contains(id)) &&
                std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(part),
                "Invalid face ridge correction Part",1413);
            enforceNative(!parameter->getBinding(part, "deform") || priorBindings.contains(id),
                "Cheek correction would overwrite an existing Part angle binding",1413);
        }
        rootSignature = skeletonDigest(puppet->findNodeById(uuid(indexNative(state, "rigRoot"))));
        if (!bound.contains(skinId)) return;
        const auto isSkin = [&](std::uint32_t id) {
            return bound.contains(id) && (id == skinId || coincidentSkin.contains(id) || skinOutlines.contains(id));
        };
        auto skin = puppet->findNodeById(skinId);
        auto anchor = skin;
        while (auto parent = anchor->parentRef()) {
            if (!isSkin(parent->getUUID())) break;
            anchor = parent;
        }
        auto parent = anchor->parentRef();
        if (!parent) return;
        using nicxlive::core::nodes::PathDeformer;
        static constexpr char pathName[] = "AutoRig::Face Contour";
        auto path = std::dynamic_pointer_cast<PathDeformer>(parent);
        if (!path || path->getName().view() != pathName) path.reset();
        if (path) {
            if (const auto prior = state.sourceObjectRef().if_contains("shape_corrections"))
                if (const auto siblings = prior->sourceObjectRef().if_contains("cheek_path_sibling_roots"))
                    contourSiblingRoots = siblings->sourceArrayValue().duplicate();
        }
        std::vector<NodeRef> roots;
        for (const auto& child : parent->childrenList())
            if (isSkin(child->getUUID())) roots.push_back(child);
        std::vector<NodeRef> features;
        const auto collectFeatures = [&](const auto& self, const NodeRef& node) -> void {
            for (const auto& child : node->childrenList()) {
                if (isSkin(child->getUUID())) self(self, child);
                else features.push_back(child);
            }
        };
        for (const auto& node : roots) collectFeatures(collectFeatures, node);
        const auto curve = indexNative(report, "cheek_path");
        const auto rest = ngRigPoints(indexNative(curve, "rest"));
        enforceNative(rest.size() >= 8 && rest[0][0] == rest[rest.size()-1][0] &&
            rest[0][1] == rest[rest.size()-1][1],
            "Cheek path landmarks are missing", 1415);
        const auto keys = indexNative(curve, "keys").sourceArrayValue();
        enforceNative(keys.size() == parameter->axisPoints[0].size()*parameter->axisPoints[1].size(),
            "Cheek path key grid differs from the face control", 1416);
        if (!path) {
            using commands::node::node::AddNodeCommandT;
            path = std::dynamic_pointer_cast<PathDeformer>(createSourceNode([] {
                return createNativeObject<AddNodeCommandT<true>>(
                    compat::RetainedString::fromStaticLiteral("PathDeformer"), compat::RetainedString::emptyLiteral());
            }, [&] { return editorContext({parent}); }));
            enforceNative(bool(path), "Could not create the cheek PathDeformer", 1417);
            path->setName(compat::RetainedString::fromStaticLiteral(pathName).toNullableString());
        }
        using nicxlive::core::math::Mat4;
        using nicxlive::core::math::Vec2;
        const auto toPath = Mat4::multiply(Mat4::inverse(path->transform().toMat4()),
            puppet->root->transform().toMat4());
        const auto local = [&](const auto& p) {
            return Vec2{static_cast<float>(toPath[0][0]*p[0]+toPath[0][1]*p[1]+toPath[0][3]),
                        static_cast<float>(toPath[1][0]*p[0]+toPath[1][1]*p[1]+toPath[1][3])};
        };
        std::vector<Vec2> controls;
        for (const auto& point : rest) controls.push_back(local(point));
        if (path->getVertices().size() != controls.size())
            if (const auto oldBinding = parameter->getBinding(path, "deform"))
                parameter->removeBinding(oldBinding);
        path->rebuffer(controls);
        const auto moveNode = [&](const NodeRef& node, const NodeRef& destination) {
            using commands::node::node::MoveNodeCommand;
            commandSource([&] {
                return createNativeObject<MoveNodeCommand>(destination, destination->childrenList().size(), true);
            }, [&] { return editorContext({node}); });
            enforceNative(node->parentRef() == destination, "Could not attach the cheek path target", 1418);
        };
        // Leave the eyes, mouth and other features on their existing Grid;
        // the Path only receives the skin and its coincident outline layers.
        const auto gridParent = parent == path ? path->parentRef() : parent;
        enforceNative(bool(gridParent), "Cheek path has no original parent", 1418);
        for (const auto& feature : features) {
            moveNode(feature, gridParent);
            contourSiblingRoots.append(feature->getUUID());
        }
        const auto link = [&](const auto& self, const NodeRef& node) -> void {
            pathDriven[node->getUUID()] = true;
            if (const auto binding = parameter->getBinding(node, "deform")) parameter->removeBinding(binding);
            for (const auto& child : node->childrenList()) self(self, child);
        };
        for (const auto& node : roots) {
            if (node->parentRef() != path) moveNode(node, path);
            link(link, node);
        }
        path->build();
        for (const auto& entry : keys) {
            const auto points = ngRigPoints(indexNative(entry, "points"));
            const auto key = ngRigNumbers(indexNative(entry, "key"));
            enforceNative(points.size() == controls.size(), "Incomplete cheek path control key", 1419);
            compat::Slice<float> offsets;
            for (std::size_t i = 0; i < points.size(); ++i) {
                const auto delta = local(points[i]) - controls[i];
                offsets.append(delta.x);
                offsets.append(delta.y);
            }
            auto context = editorContext({path});
            context->parameters({parameter});
            context->keyPoint({static_cast<unsigned>(key[0]), static_cast<unsigned>(key[1])});
            context->hasExplicitKeyPoint(true);
            commandSource([&] {
                return createNativeObject<commands::model::set_deform_binding::SetDeformBindingCommand>(
                    compat::RetainedString::fromStaticLiteral("deform"), offsets, true);
            }, [&] { return context; });
        }
        auto pathBinding = parameter->getBinding(path, "deform");
        enforceNative(pathBinding && pathBinding->getSetCount() == keys.size(),
            "Cheek PathDeformer keys could not be read back", 1420);
        contourId = path->getUUID();
    });
    for (auto operation : indexNative(report, "operations").sourceArrayValue()) {
        const auto part = ngRigUnsigned(indexNative(operation, "part"));
        if (!bound.contains(part) || pathDriven.contains(part)) continue;
        ngRigCheckpoint(task);
        auto offsets = nativeNumbers(indexNative(operation, "values")); auto indices = ngRigNumbers(indexNative(operation, "key"));
        task->runOnMainThread([&] {
            auto puppet = project::incActivePuppet(); auto node = puppet->findNodeById(uuid(indexNative(operation, "part")));
            auto parameter = parameterByName(puppet, "Face::Yaw-Pitch");
            auto context = editorContext({node}); context->parameters({parameter});
            context->keyPoint({static_cast<std::uint32_t>(readNativeArray(indices,0,1426)),
                static_cast<std::uint32_t>(readNativeArray(indices,1,1426))});
            context->hasExplicitKeyPoint(true);
            commandSource(
                [&] {
                    return createNativeObject<commands::model::set_deform_binding::SetDeformBindingCommand>(
                        compat::RetainedString::fromStaticLiteral("deform"), offsets, true);
                },
                [&] {
                    return context;
                });
            auto binding = std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(parameter->getBinding(node, "deform"));
            auto key = context->keyPoint(); nicxlive::core::param::Vec2u runtimeKey{key.x, key.y};
            enforceNative(binding && binding->isSet(runtimeKey),"Cheek correction key readback is missing",1429);
            auto actual = binding->getValue(runtimeKey).vertexOffsets;
            enforceNative(actual.size() * 2 == offsets.size(),"Cheek correction vertex count changed",1431);
            for (std::size_t i = 0; i < actual.size(); ++i)
                enforceNative(std::abs(actual[i].x - offsets[i * 2]) < .0003 && std::abs(actual[i].y - offsets[i * 2 + 1]) < .0003,"Cheek correction key differs from the compiled residual",1432);
        });
    }
    task->runOnMainThread([&] {
        auto puppet = project::incActivePuppet(); auto parameter = parameterByName(puppet, "Face::Yaw-Pitch");
        parameter->value = parameter->defaults; puppet->update();
        enforceNative(skeletonDigest(puppet->findNodeById(uuid(indexNative(state, "rigRoot")))) == rootSignature,"Cheek correction changed the native skeleton or depth authority",1439);
        for (auto key : indexNative(state, "native_head_keys").sourceArrayValue()) {
            auto grid = puppet->findNodeById(uuid(indexNative(key, "grid")));
            auto binding = std::dynamic_pointer_cast<nicxlive::core::param::DeformationParameterBinding>(parameter->getBinding(grid, "deform"));
            auto index = ngRigNumbers(indexNative(key, "key")), expected = ngRigNumbers(indexNative(key, "offsets"));
            auto actual = binding->getValue({static_cast<std::uint32_t>(readNativeArray(index,0,1445)),
                static_cast<std::uint32_t>(readNativeArray(index,1,1445))}).vertexOffsets;
            enforceNative(actual.size() * 2 == expected.size(),"Cheek correction changed a native grid binding",1446);
            for (std::size_t i = 0; i < actual.size(); ++i)
                enforceNative(actual[i].x == expected[i * 2] && actual[i].y == expected[i * 2 + 1],"Cheek correction changed native depth-generated offsets",1447);
        }
    });
    core::settings::SettingsJsonArray targets;
    for (auto id : bound.keys()) {
        if (!pathDriven.contains(id)) targets.append(id);
        if (task->targetScope()) task->recordUnit({{static_cast<std::uint32_t>(id)},
            framework::AutoRigTaskState::Succeeded, {}});
    }
    assignNative(report, "bound_targets", targets); assignNative(report, "native_grid_depth_bones_unchanged", true);
    if (contourId) {
        assignNative(report, "cheek_path_node", contourId);
        assignNative(report, "cheek_path_sibling_roots", contourSiblingRoots);
    }
    auto unsignedReport=report.sourceObjectRef().duplicate();
    unsignedReport.erase("content_sha256");
    assignNative(report, "content_sha256", core::settings::SettingsString(ngRigDigest(Json(unsignedReport))));
    assignNative(state, "shape_corrections", report); assignNative(state, "shape_corrections_sha256", indexNative(report, "content_sha256"));
    return state;
}
}
