#include "set_deform_binding.hpp"
#include "detail/node_deform_offsets.hpp"
#include "detail/parameter_keypoint.hpp"
#include "detail/set_deform_binding_vec2_arg.hpp"
#include "set_deform_binding_commands.hpp"
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/actions/binding_add_remove_action.hpp>
#include <nijigenerate/actions/binding_value_action.hpp>
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/ext/nodes/node_name.hpp>
#include <nijigenerate/commands/binding/base.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <nijigenerate/viewport/model/mesheditor/editor.hpp>
#include <nijigenerate/project.hpp>
#include <param/binding_impl.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <cmath>
#include <memory>
#include <numbers>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace nijigenerate::commands::model::set_deform_binding {
namespace {
// These are explicit non-template Source classes, not names derived from RTTI.
[[maybe_unused]] const bool sourceClassNamesRegistered = [] {
    using nicxlive::core::d_runtime::ngRegisterObjectClassName;
    ngRegisterObjectClassName(typeid(SetDeformBindingCommand), compat::RetainedString::fromStaticLiteral(
        "nijigenerate.commands.model.set_deform_binding.SetDeformBindingCommand"));
    ngRegisterObjectClassName(typeid(SetTRSBindingCommand), compat::RetainedString::fromStaticLiteral(
        "nijigenerate.commands.model.set_deform_binding.SetTRSBindingCommand"));
    ngRegisterObjectClassName(typeid(SetRotationXBindingCommand), compat::RetainedString::fromStaticLiteral(
        "nijigenerate.commands.model.set_deform_binding.SetRotationXBindingCommand"));
    ngRegisterObjectClassName(typeid(SetRotationYBindingCommand), compat::RetainedString::fromStaticLiteral(
        "nijigenerate.commands.model.set_deform_binding.SetRotationYBindingCommand"));
    return true;
}();
}
namespace rt = nicxlive::core::param;
namespace deform = nijigenerate::viewport::model::deform;
using Parameter = std::shared_ptr<rt::Parameter>;
using Node = std::shared_ptr<nicxlive::core::nodes::Node>;
using Nodes = compat::Slice<Node>;
using Binding = std::shared_ptr<rt::ParameterBinding>;
using Bindings = compat::Slice<Binding>;
using Vec2 = nicxlive::core::math::Vec2;
using Vec2Array = nicxlive::core::math::Vec2Array;
using Key = rt::Vec2u;
using Editor = std::shared_ptr<nijigenerate::viewport::common::mesheditor::IncMeshEditor>;
using Group = std::shared_ptr<actions::GroupAction>;
namespace {
template<std::size_t N> compat::RetainedString tr(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}
template<std::size_t L, std::size_t D>
detail::CommandLabels translatedLabels(const char (&label)[L], const char (&description)[D]) {
    auto translatedLabel = tr(label);
    auto translatedDescription = tr(description);
    return {std::move(translatedLabel), std::move(translatedDescription)};
}
bool findIndex(Parameter param, std::size_t& index) {
    index = 0;
    if (!param) return false;
    if (auto ex = std::dynamic_pointer_cast<ext::ExParameter>(param)) {
        if (auto parent = ex->getParent()) {
            const auto children = parent->children;
            for (std::size_t i = 0; i < children.size(); ++i)
                if (nicxlive::core::ngResourceObjectEquals(children[i], param)) { index = i; return true; }
        }
    }
    if (auto puppet = project::incActivePuppet()) {
        const auto parameters = puppet->parameters;
        for (std::size_t i = 0; i < parameters.size(); ++i)
            if (nicxlive::core::ngResourceObjectEquals(parameters[i], param)) { index = i; return true; }
    }
    return false;
}
std::shared_ptr<CommandResult> ngPrepareSetDeformBindingContext(std::shared_ptr<Context> ctx, Parameter param,
        Key& kp, std::size_t& index) {
    kp = {}; index = 0;
    if (!param) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No parameter resolved"));
    if (!findIndex(param, index)) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("Parameter not found in active puppet"));
    if (actions::node::detail::requireActionObject(ctx)->hasParameterValue()) {
        const auto value = ctx->parameterValue();
        compat::Vec2u keyPoint;
        compat::RetainedStringSlice message;
        if (!detail::ngResolveParameterValueToKeyPoint(param, value, keyPoint, message))
            return CommandResult::opCall(false, message.retainedString());
        kp = {keyPoint.x, keyPoint.y};
    } else if (ctx->hasKeyPoint() && ctx->hasExplicitKeyPoint()) {
        const auto x = ctx->keyPoint().x;
        if (x >= param->axisPointCount(0))
            return CommandResult::opCall(false,
                compat::RetainedString::fromStaticLiteral("keyPoint is out of range for parameter"));
        const auto y = ctx->keyPoint().y;
        if (y >= param->axisPointCount(1))
            return CommandResult::opCall(false,
                compat::RetainedString::fromStaticLiteral("keyPoint is out of range for parameter"));
        kp = ctx->keyPoint();
    } else kp = param->findClosestKeypoint();
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ngResolveSetDeformBindingParameter(std::shared_ptr<Context> ctx, Parameter& param,
        Key& kp, std::size_t& index) {
    param.reset(); kp = {}; index = 0;
    if (actions::node::detail::requireActionObject(ctx)->hasArmedParameters() &&
            !ctx->armedParameters().empty()) {
        if (ctx->armedParameters().size() != 1) return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("SetDeformBinding requires exactly one armed parameter"));
        const auto parameters = ctx->armedParameters();
        if (parameters.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, parameters.size(),
            compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/commands/model/set_deform_binding.d"), 103);
        param = parameters[0];
    } else if (ctx->hasParameters() && !ctx->parameters().empty()) {
        if (ctx->parameters().size() != 1) return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("SetDeformBinding requires exactly one parameter"));
        const auto parameters = ctx->parameters();
        if (parameters.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, parameters.size(),
            compat::RetainedString::fromStaticLiteral(
                "source/nijigenerate/commands/model/set_deform_binding.d"), 107);
        param = parameters[0];
    } else param = project::incArmedParameter();
    if (!param) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No parameters resolved"));
    return ngPrepareSetDeformBindingContext(ctx, param, kp, index);
}
Nodes targets(std::shared_ptr<Context> ctx) {
    return compat::ngRequireObject(ctx)->hasNodes() ? ctx->nodes() : project::incSelectedNodes();
}
bool hasParameter(std::shared_ptr<Context> ctx) {
    return (compat::ngRequireObject(ctx)->hasArmedParameters() && !ctx->armedParameters().empty()) ||
        (ctx->hasParameters() && !ctx->parameters().empty()) || project::incArmedParameter() != nullptr;
}
bool runnableNodes(std::shared_ptr<Context> ctx) {
    if (!hasParameter(ctx)) return false;
    for (auto node : targets(ctx)) if (node) return true;
    return false;
}
Binding findBinding(Parameter param, std::shared_ptr<nicxlive::core::Resource> target,
        compat::RetainedStringSlice name) {
    if (!param || !target) return {};
    if (auto direct = param->getBinding(target, ext::nodes::ngRetainNodeName(name))) return direct;
    const auto bindings = param->bindings;
    for (auto binding : bindings) {
        if (!binding || binding->getNameDescriptor().view() != name.view()) continue;
        const auto bindingTarget = binding->getTarget().getResource();
        if (!bindingTarget) {
            const auto bindingUuid = binding->getNodeUUID();
            const auto targetUuid = target->getUUID();
            if (bindingUuid == targetUuid) {
                binding->setResourceTarget(target, ext::nodes::ngRetainNodeName(name)); return binding;
            }
        }
    }
    return {};
}
void finalize(Parameter param) {
    if (!param) return;
    binding::paramPointChanged(param); deform::ngViewportNodeDeformNotifyParamValueChanged();
}
bool allZero(const Vec2Array& offsets) {
    for (std::size_t i = 0; i < offsets.size(); ++i) {
        const auto value = offsets.at(i); if (value.x != 0 || value.y != 0) return false;
    }
    return true;
}
bool ngCanUseActiveDeformEditor(Parameter param) {
    const auto armed = project::incArmedParameter();
    if (!armed || armed != param) return false;
    return deform::ngViewportModelDeformGetEditor() != nullptr;
}
void flushActive() {
    const auto editor = deform::ngViewportModelDeformGetEditor();
    if (!editor) return;
    for (auto node : editor->getTargets()) if (auto target = editor->getEditorFor(node)) {
        target->pushDeformAction(); target->forceResetAction();
    }
}
Editor temporary(Nodes nodes) {
    using DeformEditor = nijigenerate::viewport::model::mesheditor::editor::DeformMeshEditor;
    auto editor = std::allocate_shared<DeformEditor>(nicxlive::core::d_runtime::SourceAllocator<DeformEditor>{});
    editor->setTargets(nodes); editor->resetMesh(); return editor;
}
bool ngNormalizeDeformOffsetsThroughEditor(Editor editor, Node node, Vec2Array& offsets, bool update,
        Vec2Array& normalized, compat::RetainedString& message) {
    // D initializes out descriptors before any branch or editor operation.
    std::destroy_at(&normalized);
    std::construct_at(&normalized);
    message.reset();
    if (!editor) {
        message = compat::RetainedString::fromStaticLiteral("Deform editor is not available");
        return false;
    }
    const auto target = editor->getEditorFor(node);
    if (!target) {
        const auto name = actions::node::detail::requireActionObject(node)->getName();
        constexpr std::string_view prefix = "Deform editor does not support target node: ";
        try {
            std::string text;
            text.reserve(prefix.size() + name.size());
            text.append(prefix);
            if (!name.empty()) text.append(name.data(), name.size());
            message = compat::RetainedString(std::move(text));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        return false;
    }
    target->resetMesh(); target->applyOffsets(offsets); normalized = target->getOffsets();
    if (update) target->adjustPathTransform();
    if (normalized.size() != offsets.size()) {
        message = compat::RetainedString::fromStaticLiteral(
            "Editor-produced deform offset count does not match target vertices");
        return false;
    }
    return true;
}
std::shared_ptr<rt::ValueParameterBinding> valueBinding(Parameter param, Node node, compat::RetainedStringSlice name,
        Group group, Bindings& created) {
    if (!param || !node) return {};
    if (!node->hasParamSource(name.retainedString())) return {};
    if (auto existing = std::dynamic_pointer_cast<rt::ValueParameterBinding>(findBinding(param, node, name))) return existing;
    const auto result = std::dynamic_pointer_cast<rt::ValueParameterBinding>(param->createBinding(
        node, ext::nodes::ngRetainNodeName(name)));
    if (!result) return {};
    param->addBinding(result);
    compat::ngRequireObject(group)->addAction(
        actions::binding::ngCreateBindingAddRemoveAction<actions::binding::ParameterBindingAddAction>(param, result));
    created.append(result); return result;
}
bool setValue(Parameter param, Key kp, Nodes nodes, compat::RetainedStringSlice name,
        float value, Group group, Bindings& created) {
    compat::Slice<std::shared_ptr<rt::ValueParameterBinding>> bindings;
    for (auto node : nodes) if (auto binding = valueBinding(param, node, name, group, created)) bindings.append(binding);
    if (bindings.empty()) return false;
    using ValueAction = actions::binding::ParameterBindingValueChangeAction<float, decltype(bindings)>;
    const auto action = actions::binding::ngCreateBindingValueAction<ValueAction>(name, bindings,
        static_cast<int>(kp.x), static_cast<int>(kp.y));
    for (auto binding : bindings) binding->setValue(kp, value);
    action->updateNewState(); compat::ngRequireObject(group)->addAction(action); return true;
}
template<std::size_t N> bool setValue(Parameter param, Key kp, Nodes nodes, const char (&name)[N],
        float value, Group group, Bindings& created) {
    return setValue(param, kp, nodes, compat::RetainedString::fromStaticLiteral(name), value, group, created);
}
template<std::size_t N, class ReadValue> std::shared_ptr<CommandResult> rotation(
        std::shared_ptr<Context> ctx, const char (&name)[N], ReadValue readValue) {
    Parameter param; Key kp; std::size_t index;
    const auto result = ngResolveSetDeformBindingParameter(ctx, param, kp, index); if (!result->succeeded) return result;
    const auto nodes = targets(ctx); if (nodes.empty()) return CommandResult::opCall(false, "No target nodes");
    auto group = std::allocate_shared<actions::GroupAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::GroupAction>{});
    Bindings created;
    if (!setValue(param, kp, nodes, name, readValue(), group, created))
        return CommandResult::opCall(false, "No bindings updated");
    if (!group->empty()) core::actionstack::incActionPush(group);
    finalize(param);
    if (!created.empty()) return CreateResult<Binding>::opCall(true, created);
    return CommandResult::opCall(true);
}
}

detail::CommandLabels SetDeformBindingCommand::sourceConstructorLabels() {
    return translatedLabels("Set Deform Binding", "Set deform binding offsets at current keypoint.");
}
SetDeformBindingCommand::SetDeformBindingCommand(compat::RetainedStringSlice name, compat::Slice<float> vals,
        bool createZero, bool notify)
    : SetDeformBindingCommand(sourceConstructorLabels(), name, vals, createZero, notify) {}
SetDeformBindingCommand::SetDeformBindingCommand(detail::CommandLabels labels,
        compat::RetainedStringSlice name, compat::Slice<float> vals, bool createZero, bool notify)
    : ExCommand(std::move(labels.label), std::move(labels.description), name, vals) {
    SourceFlagProgram::initialize(*this, createZero, notify);
}
detail::CommandLabels SetTRSBindingCommand::sourceConstructorLabels() {
    return translatedLabels("Set TRS Binding",
        "Set node transform translation, rotation, and scale bindings at current keypoint.");
}
SetTRSBindingCommand::SetTRSBindingCommand(compat::Slice<float> translation, compat::Slice<float> scale,
        float degrees, bool rotate)
    : SetTRSBindingCommand(sourceConstructorLabels(), translation, scale, degrees, rotate) {}
SetTRSBindingCommand::SetTRSBindingCommand(detail::CommandLabels labels,
        compat::Slice<float> translation, compat::Slice<float> scale, float degrees, bool rotate)
    : ExCommand(std::move(labels.label), std::move(labels.description),
        translation, scale, degrees, rotate) {}
detail::CommandLabels SetRotationXBindingCommand::sourceConstructorLabels() {
    return translatedLabels("Set X Rotation Binding", "Set node transform X rotation binding at current keypoint.");
}
SetRotationXBindingCommand::SetRotationXBindingCommand(float radians)
    : SetRotationXBindingCommand(sourceConstructorLabels(), radians) {}
SetRotationXBindingCommand::SetRotationXBindingCommand(detail::CommandLabels labels, float radians)
    : ExCommand(std::move(labels.label), std::move(labels.description), radians) {}
detail::CommandLabels SetRotationYBindingCommand::sourceConstructorLabels() {
    return translatedLabels("Set Y Rotation Binding", "Set node transform Y rotation binding at current keypoint.");
}
SetRotationYBindingCommand::SetRotationYBindingCommand(float radians)
    : SetRotationYBindingCommand(sourceConstructorLabels(), radians) {}
SetRotationYBindingCommand::SetRotationYBindingCommand(detail::CommandLabels labels, float radians)
    : ExCommand(std::move(labels.label), std::move(labels.description), radians) {}
bool SetDeformBindingCommand::runnable(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!hasParameter(ctx)) return false;
    for (auto node : targets(ctx)) if (std::dynamic_pointer_cast<nicxlive::core::nodes::Drawable>(node) ||
        std::dynamic_pointer_cast<nicxlive::core::nodes::Deformable>(node)) return true;
    return false;
}
bool SetTRSBindingCommand::runnable(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return runnableNodes(ctx);
}
bool SetRotationXBindingCommand::runnable(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return runnableNodes(ctx);
}
bool SetRotationYBindingCommand::runnable(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    return runnableNodes(ctx);
}
std::shared_ptr<CommandResult> SetRotationXBindingCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!runnable(ctx)) return CommandResult::opCall(false, "No applicable parameters or nodes");
    return rotation(ctx, "transform.r.x", [this] { return rotationRadians; });
}
std::shared_ptr<CommandResult> SetRotationYBindingCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!runnable(ctx)) return CommandResult::opCall(false, "No applicable parameters or nodes");
    return rotation(ctx, "transform.r.y", [this] { return rotationRadians; });
}
std::shared_ptr<CommandResult> SetTRSBindingCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!runnable(ctx)) return CommandResult::opCall(false, "No applicable parameters or nodes");
    const bool hasTranslation = !translation.empty(), hasScale = !scale.empty();
    const bool hasRotation = applyRotation || rotationDegrees != 0;
    if (!hasTranslation && !hasScale && !hasRotation) return CommandResult::opCall(false, "No TRS values provided");
    Vec2 translationValue, scaleValue;
    compat::RetainedString error;
    if (hasTranslation) {
        const auto input = translation;
        if (!detail::ngResolveVec2Arg(std::span<const float>(input.data(), input.size()), {0, 0},
                translationValue, compat::RetainedString::fromStaticLiteral("translation"), error))
            return CommandResult::opCall(false, error);
    }
    if (hasScale) {
        const auto input = scale;
        if (!detail::ngResolveVec2Arg(std::span<const float>(input.data(), input.size()), {1, 1},
                scaleValue, compat::RetainedString::fromStaticLiteral("scale"), error))
            return CommandResult::opCall(false, error);
    }
    Parameter param; Key kp; std::size_t index;
    const auto result = ngResolveSetDeformBindingParameter(ctx, param, kp, index); if (!result->succeeded) return result;
    const auto nodes = targets(ctx); if (nodes.empty()) return CommandResult::opCall(false, "No target nodes");
    bool changed = false;
    Bindings created;
    const auto group = std::allocate_shared<actions::GroupAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::GroupAction>{});
    if (hasTranslation) {
        changed = setValue(param, kp, nodes, "transform.t.x", translationValue.x, group, created) || changed;
        changed = setValue(param, kp, nodes, "transform.t.y", translationValue.y, group, created) || changed;
    }
    if (hasScale) {
        changed = setValue(param, kp, nodes, "transform.s.x", scaleValue.x, group, created) || changed;
        changed = setValue(param, kp, nodes, "transform.s.y", scaleValue.y, group, created) || changed;
    }
    if (hasRotation) {
        const double radians = rotationDegrees * std::numbers::pi_v<double> / 180.0f;
        changed = setValue(param, kp, nodes, "transform.r.z", static_cast<float>(radians), group, created) || changed;
    }
    if (!changed) return CommandResult::opCall(false, "No bindings updated");
    if (!group->empty()) core::actionstack::incActionPush(group);
    finalize(param);
    if (!created.empty()) return CreateResult<Binding>::opCall(changed, created, "");
    return CommandResult::opCall(changed, "");
}
std::shared_ptr<CommandResult> SetDeformBindingCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    if (!runnable(ctx)) return CommandResult::opCall(false, "No applicable parameters or nodes");
    if (values.empty()) return CommandResult::opCall(false, "No values provided");
    if (bindingName.view() != "deform") return CommandResult::opCall(false, "SetDeformBinding only supports bindingName='deform'");
    if (values.size() < 2 || values.size() % 2 != 0)
        return CommandResult::opCall(false, "Deform values must be flattened [dx,dy]* offsets");
    Parameter param; Key kp; std::size_t index;
    const auto result = ngResolveSetDeformBindingParameter(ctx, param, kp, index); if (!result->succeeded) return result;
    const auto nodes = targets(ctx); if (nodes.empty()) return CommandResult::opCall(false, "No target nodes");
    bool changed = false, usedActive = false; Bindings created; Nodes activeNodes;
    Vec2Array offsets; offsets.resize(values.size() / 2);
    for (std::size_t i = 0; i < offsets.size(); ++i) offsets.set(i, {values[i*2], values[i*2+1]});
    Nodes editorNodes;
    for (auto node : nodes) {
        if (!detail::ngNodeAcceptsDeformOffsets(node, offsets.size())) continue;
        const auto existing = findBinding(param, node, bindingName);
        if (existing && !std::dynamic_pointer_cast<rt::DeformationParameterBinding>(existing)) continue;
        if (!existing && allZero(offsets) && !createZeroBinding) continue;
        editorNodes.append(node);
    }
    if (editorNodes.empty()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No bindings updated"));
    Editor active = ngCanUseActiveDeformEditor(param) ? deform::ngViewportModelDeformGetEditor() : Editor{};
    Nodes temporaryNodes;
    if (active) {
        flushActive(); const auto current = param->findClosestKeypoint();
        if (current.x != kp.x || current.y != kp.y) param->value = param->getKeypointValue(kp);
        for (auto node : editorNodes) {
            if (active->getEditorFor(node)) activeNodes.append(node); else temporaryNodes.append(node);
        }
        if (!activeNodes.empty()) {
            deform::ngViewportModelDeformSyncEditor(param, kp, activeNodes);
            active = deform::ngViewportModelDeformGetEditor(); usedActive = true;
        }
    } else temporaryNodes = editorNodes;
    const auto temp = !temporaryNodes.empty() ? temporary(temporaryNodes) : Editor{};
    if (!temporaryNodes.empty() && !temp) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("Could not prepare deformation editor"));
    compat::Slice<std::shared_ptr<rt::DeformationParameterBinding>> bindings;
    compat::Slice<Vec2Array> normalizedOffsets; Bindings added;
    for (auto node : editorNodes) {
        auto binding = std::dynamic_pointer_cast<rt::DeformationParameterBinding>(findBinding(param, node, bindingName));
        if (!binding) {
            binding = std::dynamic_pointer_cast<rt::DeformationParameterBinding>(param->getOrAddBinding(
                node, nicxlive::core::common::NullableString::fromStaticLiteral("deform")));
            if (!binding) continue;
            added.append(binding);
        }
        Vec2Array normalized; compat::RetainedString message;
        const auto editor = active && active->getEditorFor(node) ? active : temp;
        if (!ngNormalizeDeformOffsetsThroughEditor(editor, node, offsets, editor == active, normalized, message)) {
            if (!message.empty()) return CommandResult::opCall(false, message);
            continue;
        }
        bindings.append(binding); normalizedOffsets.append(normalized);
    }
    if (bindings.empty()) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No bindings updated"));
    const auto group = std::allocate_shared<actions::GroupAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::GroupAction>{});
    for (auto binding : added) {
        group->addAction(actions::binding::ngCreateBindingAddRemoveAction<actions::binding::ParameterBindingAddAction>(param, binding));
        created.append(binding);
    }
    const auto action = actions::parameter::ngCreateParameterBindingsActionDeferred<
        actions::parameter::ParameterChangeBindingsValueAction>([&] {
        return std::tuple{tr("Set Deform Binding"), param, bindings,
            static_cast<int>(kp.x), static_cast<int>(kp.y), notifyDepthBone};
    });
    const auto currentBindings = bindings;
    for (std::size_t i = 0; i < currentBindings.size(); ++i) {
        const auto binding = currentBindings[i];
        const auto offsets = normalizedOffsets[i];
        compat::ngRequireObject(binding)->update(kp, offsets);
    }
    action->updateNewState(); group->addAction(action);
    if (!group->empty()) { core::actionstack::incActionPush(group); changed = true; }
    binding::paramPointChanged(param);
    if (usedActive) deform::ngViewportModelDeformSyncEditor(param, kp, activeNodes);
    if (!created.empty()) return CreateResult<Binding>::opCall(changed, created, changed ? "" : "No bindings updated");
    return CommandResult::opCall(changed, changed ? "" : "No bindings updated");
}
void ngInitModelBindingCommands() {
    initialization::Program::run<initialization::Table>();
}
} // namespace nijigenerate::commands::model::set_deform_binding
