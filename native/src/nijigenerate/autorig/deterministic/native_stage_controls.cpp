#include "detail/native_stage.hpp"
#include "native_stage.hpp"
#include "controls.hpp"
#include "review.hpp"
#include "detail/evidence.hpp"
#include <nijigenerate/project.hpp>
#include <nijigenerate/commands/parameter/param_commands.hpp>
#include <nijigenerate/commands/model/set_deform_binding_commands.hpp>
#include <nijigenerate/commands/node/mask_commands.hpp>
#include <nijigenerate/commands/inspector/node_commands.hpp>
#include <cmath>

namespace nijigenerate::autorig::deterministic::native_detail {
using namespace evidence::detail;
Json applyControls(Json state, Task task) {
    auto compiled = controls::ngRigCompileControls(state, task);
    assignNative(state, "control_geometry", review::ngRigControlGeometry(compiled));
    Json operations;
    compiled = review::ngRigReviewControls(compiled, reviewSettings(task), operations);
    task->previewJson("review-operations", operations);
    for (auto mechanism : indexNative(compiled, "mechanisms").sourceArrayValue()) {
        std::uint32_t parameterId{};
        task->runOnMainThread([&] {
            auto puppet = project::incActivePuppet();
            auto name = nativeString(indexNative(mechanism, "name"));
            const auto parameters=puppet->parameters;
            for (auto parameter : parameters)
                enforceNativeLazy(parameter->getName().view() != name.view(),[&] { return nativeText({"Local control parameter already exists: ",name.view()}); },1217);
            auto created = std::dynamic_pointer_cast<commands::CreateResult<ParameterRef>>(commandSource(
                [&] {
                    return createNativeObject<commands::parameter::param::Add2DParameterCommand>(-1, 1);
                },
                [&] {
                    return editorContext();
                }));
            enforceNative(created && created->created.size() == 1,"Could not create local control parameter",1220);
            auto parameter = created->created[0];
            parameter->setName(name.toNullableString());
            auto xs = nativeNumbers(indexNative(mechanism, "axisX")), ys = nativeNumbers(indexNative(mechanism, "axisY"));
            parameter->min = {xs[0], ys[0]};
            parameter->max = {xs[xs.size() - 1], ys[ys.size() - 1]};
            parameter->axisPoints[0].clear(); parameter->axisPoints[1].clear();
            for (auto x : xs) parameter->axisPoints[0].push_back((x - xs[0]) / (xs[xs.size() - 1] - xs[0]));
            for (auto y : ys) parameter->axisPoints[1].push_back((y - ys[0]) / (ys[ys.size() - 1] - ys[0]));
            parameter->defaults = {0, 0}; parameter->value = parameter->defaults;
            parameterId = parameter->getUUID();
        });
        for (auto operation : indexNative(mechanism, "operations").sourceArrayValue()) {
            for (auto key : indexNative(operation, "keys").sourceArrayValue()) {
                ngRigCheckpoint(task);
                auto offsets = nativeNumbers(indexNative(key, "offsets"));
                auto index = ngRigNumbers(indexNative(key, "key"));
                task->runOnMainThread([&] {
                    auto puppet = project::incActivePuppet();
                    auto node = puppet->findNodeById(uuid(indexNative(operation, "part")));
                    auto parameter = puppet->findParameter(parameterId);
                    enforceNative(node && parameter,"Local control target disappeared",1237);
                    auto context = editorContext({node}); context->parameters({parameter});
                    context->keyPoint({static_cast<std::uint32_t>(readNativeArray(index,0,1239)),
                        static_cast<std::uint32_t>(readNativeArray(index,1,1239))});
                    context->hasExplicitKeyPoint(true);
                    commandSource(
                        [&] {
                            return createNativeObject<commands::model::set_deform_binding::SetDeformBindingCommand>(
                                compat::RetainedString::fromStaticLiteral("deform"), offsets, true);
                        },
                        [&] {
                            return context;
                        });
                });
            }
        }
    }
    task->runOnMainThread([&] {
        auto puppet = project::incActivePuppet();
        for (auto mask : indexNative(compiled, "masks").sourceArrayValue()) {
            auto target = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(
                puppet->findNodeById(uuid(indexNative(mask, "part"))));
            auto source = std::dynamic_pointer_cast<nicxlive::core::nodes::Part>(
                puppet->findNodeById(uuid(indexNative(mask, "source"))));
            enforceNative(target && source,"Iris clipping target disappeared",1249);
            bool exists = false;
            for (const auto& binding : target->masks) if (binding.maskSrc.lock() == source) exists = true;
            if (!exists) commandSource(
                [&] {
                    return createNativeObject<commands::node::mask::AddMaskCommand>( source, nicxlive::core::nodes::MaskingMode::Mask);
                },
                [&] {
                    return editorContext({target});
                });
        }
        for (auto operation : indexNative(compiled, "draw_order").sourceArrayValue()) {
            auto node = puppet->findNodeById(uuid(indexNative(operation, "part")));
            enforceNative(bool(node),"Facial draw-order target disappeared",1256);
            auto context = editorContext({node});
            context->inspectors({panels::inspector::ngCreateInspector<commands::inspector::apply_node::NodeApplyInspector>(
                panels::inspector::Selection<nicxlive::core::nodes::Node>{node},
                panels::inspector::ModelEditSubMode::Layout)});
            auto apply = createNativeObject<commands::inspector::apply_node::ZSortCommand>();
            apply->value = static_cast<float>(ngRigNumber(indexNative(operation, "relative_zsort")));
            commandSource(
                [&] {
                    return apply;
                },
                [&] {
                    return context;
                });
            enforceNative(std::abs(node->relZSort() - apply->value) < 1e-6,"Facial draw-order readback failed",1260);
        }
        for (auto parameter : puppet->parameters) parameter->value = parameter->defaults;
        puppet->root->build(); puppet->update();
    });
    assignNative(state, "controls", compiled);
    return state;
}
}
