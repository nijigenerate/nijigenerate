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
    if (task->targetScope() || task->eligibleTargets()) {
        core::settings::SettingsJsonArray mechanisms, masks, drawOrder;
        for (const auto& mechanism : indexNative(compiled, "mechanisms").sourceArrayValue()) {
            std::vector<std::uint32_t> parts;
            bool selected = false, outside = false;
            for (const auto& operation : indexNative(mechanism, "operations").sourceArrayValue()) {
                const auto id = uuid(indexNative(operation, "part"));
                parts.push_back(id);
                selected |= task->targetScope() ? task->targetScope()->contains(id) : true;
                outside |= !task->acceptsTarget(id);
            }
            if (!selected) continue;
            if (outside) task->recordUnit({std::move(parts), framework::AutoRigTaskState::Blocked,
                "The control affects a target outside the selected scope"});
            else mechanisms.append(mechanism);
        }
        for (const auto& mask : indexNative(compiled, "masks").sourceArrayValue())
            if (task->acceptsTarget(uuid(indexNative(mask, "part")))) masks.append(mask);
        for (const auto& operation : indexNative(compiled, "draw_order").sourceArrayValue())
            if (task->acceptsTarget(uuid(indexNative(operation, "part")))) drawOrder.append(operation);
        assignNative(compiled, "mechanisms", mechanisms);
        assignNative(compiled, "masks", masks);
        assignNative(compiled, "draw_order", drawOrder);
    }
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
            for (const auto& parameter : parameters) {
                if (parameter->getName().view() != name.view()) continue;
                bool owned = false;
                if (task->targetScope())
                    if (auto prior = state.sourceObjectRef().if_contains("controls"))
                        for (const auto& old : indexNative(*prior, "mechanisms").sourceArrayValue())
                            owned |= indexNative(old, "name") == indexNative(mechanism, "name");
                enforceNativeLazy(owned, [&] {
                    return nativeText({"Local control parameter already exists: ", name.view()});
                }, 1217);
                const auto xs = nativeNumbers(indexNative(mechanism, "axisX"));
                const auto ys = nativeNumbers(indexNative(mechanism, "axisY"));
                enforceNative(parameter->isVec2 && parameter->axisPoints[0].size() == xs.size() &&
                    parameter->axisPoints[1].size() == ys.size(), "Existing control axis layout changed", 1218);
                for (std::size_t i = 0; i < xs.size(); ++i)
                    enforceNative(std::abs(parameter->axisPoints[0][i] - (xs[i]-xs[0])/(xs[xs.size()-1]-xs[0])) < 1e-6,
                        "Existing control X axis changed", 1218);
                for (std::size_t i = 0; i < ys.size(); ++i)
                    enforceNative(std::abs(parameter->axisPoints[1][i] - (ys[i]-ys[0])/(ys[ys.size()-1]-ys[0])) < 1e-6,
                        "Existing control Y axis changed", 1218);
                parameterId = parameter->getUUID();
                return;
            }
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
        if (task->targetScope()) {
            std::vector<std::uint32_t> parts;
            for (const auto& operation : indexNative(mechanism, "operations").sourceArrayValue())
                parts.push_back(uuid(indexNative(operation, "part")));
            task->recordUnit({std::move(parts), framework::AutoRigTaskState::Succeeded, {}});
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
            if (task->targetScope())
                task->recordUnit({{target->getUUID()}, framework::AutoRigTaskState::Succeeded, {}});
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
            if (task->targetScope())
                task->recordUnit({{node->getUUID()}, framework::AutoRigTaskState::Succeeded, {}});
        }
        if (!task->targetScope())
            for (auto parameter : puppet->parameters) parameter->value = parameter->defaults;
        puppet->root->build(); puppet->update();
    });
    if (!task->targetScope() || !state.sourceObjectRef().if_contains("controls"))
        assignNative(state, "controls", compiled);
    return state;
}
}
