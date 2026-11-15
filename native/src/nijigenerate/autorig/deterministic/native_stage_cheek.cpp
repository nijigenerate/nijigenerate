#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "controls.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/commands/model/set_deform_binding_commands.hpp>
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
Json applyCheekCorrections(Json state, Json program, Task task) {
    auto feet = controls::ngRigCompileFixedFootCorrections(state, program, task);
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
    }
    settleDepthRefresh(task);
    assignNative(state, "fixed_foot_corrections", feet);
    assignNative(state,"rear_hair_silhouettes",applyRearCap(state, program, task));
    auto report = controls::ngRigCompileCheekCorrections(state, program, task);
    if (!indexNative(report, "applicable").as_bool()) { assignNative(state, "shape_corrections", report); return state; }
    if (const auto refined = report.sourceObjectRef().if_contains("refined_mesh"))
        task->runOnMainThread([&] {
            auto puppet = project::incActivePuppet();
            auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(
                puppet->findNodeById(uuid(indexNative(*refined,"part"))));
            enforceNative(bool(part),"Missing face skin for ridge mesh",1408);
            auto mesh = part->getMesh().copy();
            const auto original = ngRigUnsigned(indexNative(*refined,"original_count"));
            enforceNative(mesh.vertices.size()==original,"Face mesh changed before ridge refinement",1408);
            const auto vertices = ngRigPoints(indexNative(*refined,"vertices"));
            const auto uv = ngRigPoints(indexNative(*refined,"uv"));
            for (std::size_t i=original;i<vertices.size();++i)
                mesh.add({static_cast<float>(vertices[i][0]),static_cast<float>(vertices[i][1])},
                         {static_cast<float>(uv[i][0]),static_cast<float>(uv[i][1])});
            mesh.indices.resize(0);
            for (const auto& face : indexNative(*refined,"triangles").sourceArrayValue())
                for (const auto index : ngRigNumbers(face)) mesh.indices.push_back(static_cast<std::uint16_t>(index));
            mesh.gridAxes = {};
            part->rebuffer(mesh);
        });
    NativeIntegerMap<std::uint64_t, bool> bound;
    NativeIntegerMap<std::uint64_t, bool> ridgeFollowers;
    NativeIntegerMap<std::uint64_t, bool> coincidentSkin;
    for (auto operation : indexNative(report, "operations").sourceArrayValue())
        for (auto value : ngRigNumbers(indexNative(operation, "values")))
            if (value != 0) {
                const auto id = ngRigUnsigned(indexNative(operation,"part"));
                bound[id] = true;
                const auto kind = stringGet(operation,"kind","");
                if (kind=="hair_ridge" || kind=="skin_outline") ridgeFollowers[id] = true;
                if (kind=="coincident_skin") coincidentSkin[id] = true;
                break;
            }
    compat::RetainedStringSlice rootSignature;
    core::settings::SettingsJsonArray sharedSkinParts;
    task->runOnMainThread([&] {
        auto puppet = project::incActivePuppet(); auto parameter = parameterByName(puppet, "Face::Yaw-Pitch");
        enforceNative(bool(parameter),"Cheek correction parameter is missing",1410);
        const auto skinId = uuid(indexNative(report, "skin"));
        if (!coincidentSkin.empty()) {
            const auto skin = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(puppet->findNodeById(skinId));
            enforceNative(bool(skin),"Missing skin for shared cheek mesh",1411);
            const auto mesh = skin->getMesh();
            for (const auto id : coincidentSkin.keys()) {
                const auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(
                    puppet->findNodeById(static_cast<std::uint32_t>(id)));
                enforceNative(bool(part),"Missing coincident skin Part",1411);
                bool hasDeformation = false;
                for (const auto control : puppet->parameters)
                    hasDeformation |= bool(control->getBinding(part, "deform"));
                const auto origin = part->getMesh().origin;
                if (hasDeformation || origin.x != mesh.origin.x || origin.y != mesh.origin.y) {
                    bound.erase(id);
                    continue;
                }
                // The source texture coordinates and part frames coincide. Use the
                // same triangles so both painted rims follow the same interpolation.
                part->rebuffer(mesh.copy());
                sharedSkinParts.append(id);
            }
            if (!sharedSkinParts.empty()) {
                assignNative(report, "shared_skin_parts", sharedSkinParts);
                assignNative(report, "shared_skin_mapping", textureMapping(skin));
            }
        }
        for (auto [id, unused] : bound) {
            auto part = puppet->findNodeById(static_cast<std::uint32_t>(id));
            enforceNative((id == skinId || ridgeFollowers.contains(id) || coincidentSkin.contains(id)) &&
                std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(part),
                "Invalid face ridge correction Part",1413);
            enforceNative(!parameter->getBinding(part, "deform"),
                "Cheek correction would overwrite an existing Part angle binding",1413);
        }
        rootSignature = skeletonDigest(puppet->findNodeById(uuid(indexNative(state, "rigRoot"))));
    });
    for (auto operation : indexNative(report, "operations").sourceArrayValue()) {
        if (!bound.contains(ngRigUnsigned(indexNative(operation, "part")))) continue;
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
    for (auto id : bound.keys()) targets.append(id);
    assignNative(report, "bound_targets", targets); assignNative(report, "native_grid_depth_bones_unchanged", true);
    auto unsignedReport=report.sourceObjectRef().duplicate();
    unsignedReport.erase("content_sha256");
    assignNative(report, "content_sha256", core::settings::SettingsString(ngRigDigest(Json(unsignedReport))));
    assignNative(state, "shape_corrections", report); assignNative(state, "shape_corrections_sha256", indexNative(report, "content_sha256"));
    return state;
}
}
