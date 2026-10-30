#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "geometry.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/commands/inspector/node_commands.hpp>
#include <cmath>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
Json registerSourceUV(Json state, Task task) {
    core::settings::SettingsJsonArray readback;
    for (auto material : state.sourceIndexValue("materials").sourceArrayValue()) {
        if (material.sourceIndexValue("static").as_bool()) continue;
        ngRigCheckpoint(task);
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
            oldScale = Point2{transform.scale.x, transform.scale.y};
            oldTranslation = Point2{transform.translation.x, transform.translation.y};
            rotation = transform.rotation.z;
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
    }
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("source_uv_registration")) = Json(readback);
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("source_uv_program_sha256")) = core::settings::SettingsString(ngRigDigest(Json(readback)));
    state.sourceIndexAssignSlot(compat::RetainedString::fromStaticLiteral("source_uv_readback")) = verifySourceFrames(state, task);
    return state;
}
}
