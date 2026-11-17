#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "geometry.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/actions/package.hpp>
#include <algorithm>
#include <nijigenerate/commands/inspector/node_commands.hpp>
#include <cmath>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
Json registerSourceUV(Json state, Task task, const std::shared_ptr<actions::AsyncGroupAction>& owner) {
    core::settings::SettingsJsonArray readback, unavailable;
    if (const auto previous = state.sourceObjectRef().if_contains("unmeshed_parts"))
        unavailable = previous->sourceArrayValue().duplicate();
    const auto unavailablePart = [&](std::uint32_t id) {
        return std::ranges::any_of(unavailable, [id](const auto& value) { return uuid(value) == id; });
    };
    for (auto material : state.sourceIndexValue("materials").sourceArrayValue()) {
        if (material.sourceIndexValue("static").as_bool()) continue;
        const auto id = uuid(material.sourceIndexValue("uuid"));
        if (!task->acceptsTarget(id)) continue;
        if (unavailablePart(id)) {
            task->recordUnit({{id}, framework::AutoRigTaskState::Blocked, "Mesh evidence is missing"});
            continue;
        }
        if (task->targetScope()) {
            Json currentSource;
            task->runOnMainThread([&] {
                auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(
                    project::incActivePuppet()->findNodeById(id));
                if (part) currentSource = authoredRootTextureMapping(part);
            });
            if (!currentSource.is_null()) {
                bool aligned = true;
                try { geometry::ngRigVerifySourceUV(material.sourceIndexValue("source_root_mapping"), currentSource); }
                catch (const compat::ThrowableException&) { aligned = false; }
                if (aligned) {
                    task->recordUnit({{id}, framework::AutoRigTaskState::NotApplicable,
                        "The source UV frame already matches"});
                    continue;
                }
            }
        }
        ngRigCheckpoint(task);
        actions::AsyncGroupAction::Savepoint saved;
        task->runOnMainThread([&] { saved = owner->savepoint(); });
        try {
            Json current;
            Point2 oldScale, oldTranslation;
            double rotation;
            task->runOnMainThread([&] {
                auto part = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(
                    project::incActivePuppet()->findNodeById(uuid(material.sourceIndexValue("uuid"))));
                enforceNative(bool(part),"UV registration Part is missing",707);
                enforceNative(project::ngModelEditSubMode() == project::ModelEditSubMode::Layout,"UV registration requires layout mode",708);
                const auto transform = part->localTransform;
                enforceNative(std::abs(transform.rotation.x) < 1e-6 && std::abs(transform.rotation.y) < 1e-6 &&
                    [&] { const auto origin=part->getMesh().origin; return origin.x==0&&origin.y==0; }(),
                    "UV registration needs a resolved Part frame",709);
                current = textureMapping(part);
                oldScale = Point2{part->localTransform.scale.x, part->localTransform.scale.y};
                oldTranslation = Point2{part->localTransform.translation.x, part->localTransform.translation.y};
                rotation = part->localTransform.rotation.z;
            });
            auto operation = geometry::ngRigSourceUVRegistration(material.sourceIndexValue("source_mapping"), current);
            const auto scale = ngRigNumbers(operation.sourceIndexValue("scale")), shift = ngRigNumbers(operation.sourceIndexValue("shift"));
            const Point2 shifted{readNativeArray(shift,0,719) * oldScale[0],readNativeArray(shift,1,719) * oldScale[1]};
            const Point2 translation{oldTranslation[0] + shifted[0] * std::cos(rotation) - shifted[1] * std::sin(rotation),
                oldTranslation[1] + shifted[0] * std::sin(rotation) + shifted[1] * std::cos(rotation)};
            task->runOnMainThread([&] {
                auto part = project::incActivePuppet()->findNodeById(uuid(material.sourceIndexValue("uuid")));
                auto context = editorContext({part});
                using namespace commands::inspector::apply_node;
                auto inspector = panels::inspector::ngCreateInspector<NodeApplyInspector>(
                    panels::inspector::Selection<nicxlive::core::nodes::Node>{part},
                    panels::inspector::ModelEditSubMode::Layout);
                context->inspectors({inspector});
                auto sx = createNativeObject<ScaleXCommand>(); sx->value = static_cast<float>(oldScale[0] * readNativeArray(scale,0,725));
                commandSource(
                    [&] {
                        return sx;
                    },
                    [&] {
                        return context;
                    });
                auto sy = createNativeObject<ScaleYCommand>(); sy->value = static_cast<float>(oldScale[1] * readNativeArray(scale,1,726));
                commandSource(
                    [&] {
                        return sy;
                    },
                    [&] {
                        return context;
                    });
                auto tx = createNativeObject<TranslationXCommand>(); tx->value = static_cast<float>(translation[0]);
                commandSource(
                    [&] {
                        return tx;
                    },
                    [&] {
                        return context;
                    });
                auto ty = createNativeObject<TranslationYCommand>(); ty->value = static_cast<float>(translation[1]);
                commandSource(
                    [&] {
                        return ty;
                    },
                    [&] {
                        return context;
                    });
                enforceNative(textureMapping(std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(part)) == current,"UV registration changed native mesh arrays",729);
            });
            operation.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("part")) = material.sourceIndexValue("uuid");
            readback.append(operation);
            task->recordUnit({{id}, framework::AutoRigTaskState::Succeeded, {}});
        } catch (const compat::ThrowableException& error) {
            ngRigCheckpoint(task);
            task->runOnMainThread([&] { owner->rollbackTo(saved); });
            unavailable.append(id);
            task->recordUnit({{id}, framework::AutoRigTaskState::Failed, error.msg});
        }
    }
    if (!unavailable.empty() && !task->targetScope()) {
        assignNative(state, "unmeshed_parts", unavailable);
        auto materials = state.sourceIndexValue("materials").sourceArrayValue().duplicate();
        for (auto& material : materials) {
            if (!unavailablePart(uuid(material.sourceIndexValue("uuid")))) continue;
            material = Json(material.sourceObjectRef().duplicate());
            assignNative(material, "static", true);
        }
        assignNative(state, "materials", materials);
        for (const auto key : {"shoulder_pairs", "surface_pairs"}) {
            core::settings::SettingsJsonArray usable;
            if (const auto pairs = state.sourceObjectRef().if_contains(key))
                for (const auto& pair : pairs->sourceArrayValue())
                    if (!unavailablePart(uuid(pair.sourceIndexValue("source"))) &&
                        !unavailablePart(uuid(pair.sourceIndexValue("target")))) usable.append(pair);
            assignNative(state, compat::RetainedString::fromStaticView(key), usable);
        }
    }
    if (task->targetScope()) {
        if (const auto prior = state.sourceObjectRef().if_contains("source_uv_registration"))
            for (const auto& record : prior->sourceArrayValue()) {
                const auto id = uuid(record.sourceIndexValue("part"));
                if (!std::ranges::any_of(readback, [id](const auto& changed) {
                        return uuid(changed.sourceIndexValue("part")) == id;
                    })) readback.append(record);
            }
    }
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("source_uv_registration")) = Json(readback);
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("source_uv_program_sha256")) = core::settings::SettingsString(ngRigDigest(Json(readback)));
    Json verificationState = core::settings::SettingsJsonValue(state.sourceObjectRef().duplicate());
    if (!unavailable.empty() || task->targetScope()) {
        core::settings::SettingsJsonArray materials;
        for (const auto& material : state.sourceIndexValue("materials").sourceArrayValue()) {
            const auto id = uuid(material.sourceIndexValue("uuid"));
            if (!unavailablePart(id) && (!task->targetScope() || task->targetScope()->contains(id)))
                materials.append(material);
        }
        assignNative(verificationState, "materials", materials);
    }
    assignNative(state, "source_uv_readback", verifySourceFrames(verificationState, task));
    return state;
}
}
