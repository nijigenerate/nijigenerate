#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "controls.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/commands/binding/binding_commands.hpp>
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
    for (auto operation : indexNative(report, "operations").sourceArrayValue())
        for (auto value : ngRigNumbers(indexNative(operation, "values")))
            if (value != 0) {
                const auto id = ngRigUnsigned(indexNative(operation,"part"));
                bound[id] = true;
                const auto kind = stringGet(operation,"kind","");
                if (kind=="hair_ridge" || kind=="skin_outline" || kind=="face_surface") ridgeFollowers[id] = true;
                if (kind=="coincident_skin") coincidentSkin[id] = true;
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
    const auto previous = state.sourceObjectRef().if_contains("shape_corrections");
    if (previous)
        if (const auto prior = previous->sourceObjectRef().if_contains("bound_targets"))
            for (const auto& part : prior->sourceArrayValue()) priorBindings[ngRigUnsigned(part)] = true;
    compat::RetainedStringSlice rootSignature;
    core::settings::SettingsJsonArray originSiblings;
    if (previous)
        if (const auto siblings = previous->sourceObjectRef().if_contains("face_origin_sibling_roots"))
            originSiblings = siblings->sourceArrayValue().duplicate();
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

        // Migrate the previous AutoRig wrapper when reapplying the task to an
        // existing result. Move its children through the ordinary editor
        // command so their world transforms and undo history are retained.
        const auto legacyId = previous ? previous->sourceObjectRef().if_contains("cheek_path_node") : nullptr;
        if (!legacyId) return;
        const auto legacy = std::dynamic_pointer_cast<nicxlive::core::nodes::PathDeformer>(
            puppet->findNodeById(uuid(*legacyId)));
        if (!legacy) return;
        auto skin = puppet->findNodeById(skinId);
        while (skin && skin->parentRef()!=legacy) skin = skin->parentRef();
        const auto parent = legacy->parentRef();
        enforceNative(skin && parent,"Saved cheek PathDeformer is no longer the skin's parent",1415);
        if (const auto siblings = previous->sourceObjectRef().if_contains("cheek_path_sibling_roots"))
            originSiblings = siblings->sourceArrayValue().duplicate();
        compat::Slice<NodeRef> children;
        for (const auto& child : legacy->childrenList()) children.append(child);
        std::size_t position = 0;
        for (const auto& sibling : parent->childrenList()) {
            if (sibling==legacy) break;
            ++position;
        }
        using commands::node::node::MoveNodeCommand;
        using commands::node::node::DeleteNodeCommand;
        commandSource([&] { return createNativeObject<MoveNodeCommand>(parent,position,true); },
            [&] { return editorContext(children); });
        enforceNative(legacy->childrenList().empty(),"Could not move Parts out of the cheek PathDeformer",1418);
        for (const auto& control : puppet->parameters) {
            auto context = editorContext();
            auto bindings = context->activeBindings();
            for (const auto& binding : control->bindings)
                if (binding->getTarget().target.lock()==legacy) bindings.append(binding);
            if (bindings.empty()) continue;
            context->parameters({control});
            context->activeBindings(bindings);
            commandSource([&] { return createNativeObject<commands::binding::RemoveBindingCommand>(); },
                [&] { return context; });
        }
        commandSource([&] { return createNativeObject<DeleteNodeCommand>(true); },
            [&] { return editorContext({legacy}); });
    });
    for (auto operation : indexNative(report, "operations").sourceArrayValue()) {
        const auto part = ngRigUnsigned(indexNative(operation, "part"));
        if (!bound.contains(part)) continue;
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
        targets.append(id);
        if (task->targetScope()) task->recordUnit({{static_cast<std::uint32_t>(id)},
            framework::AutoRigTaskState::Succeeded, {}});
    }
    assignNative(report, "bound_targets", targets); assignNative(report, "native_grid_depth_bones_unchanged", true);
    if (!originSiblings.empty()) assignNative(report, "face_origin_sibling_roots", originSiblings);
    auto unsignedReport=report.sourceObjectRef().duplicate();
    unsignedReport.erase("content_sha256");
    assignNative(report, "content_sha256", core::settings::SettingsString(ngRigDigest(Json(unsignedReport))));
    assignNative(state, "shape_corrections", report); assignNative(state, "shape_corrections_sha256", indexNative(report, "content_sha256"));
    return state;
}
}
