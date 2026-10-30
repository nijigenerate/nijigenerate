#include "simplephysics.hpp"
#include "detail/simplephysics_notification.hpp"
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nodes/simple_physics_driver.hpp>
#include <resource.hpp>
#include <concepts>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace nijigenerate::commands::node::simplephysics {
thread_local CommandTable commands;

namespace detail {
void notifySimplePhysics(std::shared_ptr<nicxlive::core::nodes::SimplePhysicsDriver> node, bool rescan) {
    if (!node) actions::node::detail::failActionObjectReference();
    node->notifyChange(node, nicxlive::core::nodes::NotifyReason::AttributeChanged);
    if (rescan) {
        const auto puppet = project::incActivePuppet();
        if (!puppet) actions::node::detail::failActionObjectReference();
        puppet->rescanNodes();
    }
}
}

namespace {
using SimplePhysics = std::shared_ptr<nicxlive::core::nodes::SimplePhysicsDriver>;
using Parameter = std::shared_ptr<nicxlive::core::param::Parameter>;
using PhysicsModel = nicxlive::core::nodes::SourcePhysicsModel;
using ParamMapMode = nicxlive::core::nodes::SourceParamMapMode;

[[maybe_unused]] Parameter getPhysicsParam(SimplePhysics node) {
    if (!node) compat::ngFailNullAccess(false);
    return node->getParam();
}
inline constexpr auto getPhysicsParamSource = SimplePhysicsSourceAlias<getPhysicsParam, "getPhysicsParam">{};
// Reading the model property does not run its setter's reset side effect.
[[maybe_unused]] PhysicsModel getPhysicsModelType(SimplePhysics node) {
    if (!node) compat::ngFailNullAccess(false);
    return node->getModelType();
}
inline constexpr auto getPhysicsModelTypeSource =
    SimplePhysicsSourceAlias<getPhysicsModelType, "getPhysicsModelType">{};
[[maybe_unused]] ParamMapMode getPhysicsMapMode(SimplePhysics node) {
    return compat::ngRequireObject(node)->mapMode;
}
inline constexpr auto getPhysicsMapModeSource = SimplePhysicsSourceAlias<getPhysicsMapMode, "getPhysicsMapMode">{};
[[maybe_unused]] bool getPhysicsLocalOnly(SimplePhysics node) {
    return compat::ngRequireObject(node)->localOnly;
}
inline constexpr auto getPhysicsLocalOnlySource =
    SimplePhysicsSourceAlias<getPhysicsLocalOnly, "getPhysicsLocalOnly">{};
[[maybe_unused]] float getPhysicsGravity(SimplePhysics node) {
    return compat::ngRequireObject(node)->gravity;
}
inline constexpr auto getPhysicsGravitySource = SimplePhysicsSourceAlias<getPhysicsGravity, "getPhysicsGravity">{};
[[maybe_unused]] float getPhysicsLength(SimplePhysics node) {
    return compat::ngRequireObject(node)->length;
}
inline constexpr auto getPhysicsLengthSource = SimplePhysicsSourceAlias<getPhysicsLength, "getPhysicsLength">{};
[[maybe_unused]] float getPhysicsFrequency(SimplePhysics node) {
    return compat::ngRequireObject(node)->frequency;
}
inline constexpr auto getPhysicsFrequencySource =
    SimplePhysicsSourceAlias<getPhysicsFrequency, "getPhysicsFrequency">{};
[[maybe_unused]] float getPhysicsAngleDamping(SimplePhysics node) {
    return compat::ngRequireObject(node)->angleDamping;
}
inline constexpr auto getPhysicsAngleDampingSource =
    SimplePhysicsSourceAlias<getPhysicsAngleDamping, "getPhysicsAngleDamping">{};
[[maybe_unused]] float getPhysicsLengthDamping(SimplePhysics node) {
    return compat::ngRequireObject(node)->lengthDamping;
}
inline constexpr auto getPhysicsLengthDampingSource =
    SimplePhysicsSourceAlias<getPhysicsLengthDamping, "getPhysicsLengthDamping">{};
[[maybe_unused]] float getPhysicsOutputScaleX(SimplePhysics node) {
    return compat::ngRequireObject(node)->outputScale[0];
}
inline constexpr auto getPhysicsOutputScaleXSource =
    SimplePhysicsSourceAlias<getPhysicsOutputScaleX, "getPhysicsOutputScaleX">{};
[[maybe_unused]] float getPhysicsOutputScaleY(SimplePhysics node) {
    return compat::ngRequireObject(node)->outputScale[1];
}
inline constexpr auto getPhysicsOutputScaleYSource =
    SimplePhysicsSourceAlias<getPhysicsOutputScaleY, "getPhysicsOutputScaleY">{};

[[maybe_unused]] void setPhysicsParam(SimplePhysics node, Parameter value) {
    if (!node) compat::ngFailNullAccess(false);
    node->setParam(value);
}
inline constexpr auto setPhysicsParamSource = SimplePhysicsSourceAlias<setPhysicsParam, "setPhysicsParam">{};
[[maybe_unused]] void setPhysicsModelType(SimplePhysics node, PhysicsModel value) {
    if (!node) compat::ngFailNullAccess(false);
    node->setModelType(value);
}
inline constexpr auto setPhysicsModelTypeSource =
    SimplePhysicsSourceAlias<setPhysicsModelType, "setPhysicsModelType">{};
[[maybe_unused]] void setPhysicsMapMode(SimplePhysics node, ParamMapMode value) {
    if (!node) compat::ngFailNullAccess(true);
    node->mapMode = value;
}
inline constexpr auto setPhysicsMapModeSource = SimplePhysicsSourceAlias<setPhysicsMapMode, "setPhysicsMapMode">{};
[[maybe_unused]] void setPhysicsLocalOnly(SimplePhysics node, bool value) {
    if (!node) compat::ngFailNullAccess(true);
    node->localOnly = value;
}
inline constexpr auto setPhysicsLocalOnlySource =
    SimplePhysicsSourceAlias<setPhysicsLocalOnly, "setPhysicsLocalOnly">{};
[[maybe_unused]] void setPhysicsGravity(SimplePhysics node, float value) {
    if (!node) compat::ngFailNullAccess(true);
    node->gravity = value;
}
inline constexpr auto setPhysicsGravitySource = SimplePhysicsSourceAlias<setPhysicsGravity, "setPhysicsGravity">{};
[[maybe_unused]] void setPhysicsLength(SimplePhysics node, float value) {
    if (!node) compat::ngFailNullAccess(true);
    node->length = value;
}
inline constexpr auto setPhysicsLengthSource = SimplePhysicsSourceAlias<setPhysicsLength, "setPhysicsLength">{};
[[maybe_unused]] void setPhysicsFrequency(SimplePhysics node, float value) {
    if (!node) compat::ngFailNullAccess(true);
    node->frequency = value;
}
inline constexpr auto setPhysicsFrequencySource =
    SimplePhysicsSourceAlias<setPhysicsFrequency, "setPhysicsFrequency">{};
[[maybe_unused]] void setPhysicsAngleDamping(SimplePhysics node, float value) {
    if (!node) compat::ngFailNullAccess(true);
    node->angleDamping = value;
}
inline constexpr auto setPhysicsAngleDampingSource =
    SimplePhysicsSourceAlias<setPhysicsAngleDamping, "setPhysicsAngleDamping">{};
[[maybe_unused]] void setPhysicsLengthDamping(SimplePhysics node, float value) {
    if (!node) compat::ngFailNullAccess(true);
    node->lengthDamping = value;
}
inline constexpr auto setPhysicsLengthDampingSource =
    SimplePhysicsSourceAlias<setPhysicsLengthDamping, "setPhysicsLengthDamping">{};
[[maybe_unused]] void setPhysicsOutputScaleX(SimplePhysics node, float value) {
    if (!node) compat::ngFailNullAccess(true);
    node->outputScale[0] = value;
}
inline constexpr auto setPhysicsOutputScaleXSource =
    SimplePhysicsSourceAlias<setPhysicsOutputScaleX, "setPhysicsOutputScaleX">{};
[[maybe_unused]] void setPhysicsOutputScaleY(SimplePhysics node, float value) {
    if (!node) compat::ngFailNullAccess(true);
    node->outputScale[1] = value;
}
inline constexpr auto setPhysicsOutputScaleYSource =
    SimplePhysicsSourceAlias<setPhysicsOutputScaleY, "setPhysicsOutputScaleY">{};

template<class T, auto Getter>
bool simplePhysicsValueWouldChange(compat::Slice<SimplePhysics> nodes, T newValue) {
    if (nodes.empty()) return false;
    for (auto node : nodes) {
        if constexpr (std::convertible_to<decltype(Getter(node)), std::shared_ptr<nicxlive::core::Resource>> &&
            std::convertible_to<T, std::shared_ptr<nicxlive::core::Resource>>) {
            if (!nicxlive::core::ngResourceObjectEquals(Getter(node), newValue)) return true;
        } else if constexpr (requires (T value) {
            nicxlive::core::common::ngClassObjectEquals(value, value);
        }) {
            // Mapped class handles use the actual Object protocol, including
            // non-Resource classes. Missing protocols must not become identity.
            if (!nicxlive::core::common::ngClassObjectEquals<typename T::element_type>(
                    Getter(node), newValue)) return true;
        } else {
            if (Getter(node) != newValue) return true;
        }
    }
    return false;
}

} // namespace
} // namespace nijigenerate::commands::node::simplephysics

// Whole original action, concrete command bodies and initializer.
#include "simplephysics_complete.hpp"
#include "simplephysics_action.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
namespace nijigenerate::commands::node::simplephysics {
namespace {
template<class C, std::size_t N> void registerSimplePhysicsCommandClass(const char (&name)[N]) {
    static const bool registered = [&] {
        try {
            (void)nicxlive::core::d_runtime::ngObjectTemplateArgumentName<C>();
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(C),
                compat::RetainedString::fromStaticLiteral(name));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        return true;
    }();
    (void)registered;
}
// Nonempty Phobos enforce messages are duplicated only on the failure path.
// The Throwable constructor and every user callback stay outside this catch.
compat::RetainedString copySimplePhysicsEnforceMessage(std::string_view message) {
    try {
        return compat::RetainedString(message);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(
            compat::RetainedString::fromStaticLiteral("core/internal/array/construction.d"), 394);
    }
}
compat::Slice<SimplePhysics> simplePhysicsTargets(std::shared_ptr<Context> ctx) {
    if (!compat::ngRequireObject(ctx)->hasNodes() || compat::ngRequireObject(ctx)->nodes().empty())
        throw compat::ThrowableException(
            copySimplePhysicsEnforceMessage("No SimplePhysics node in context"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/node/simplephysics.d"), 31);
    compat::Slice<SimplePhysics> result;
    for (auto node : compat::ngRequireObject(ctx)->nodes())
        if (auto physics=std::dynamic_pointer_cast<nicxlive::core::nodes::SimplePhysicsDriver>(node))
            result.append(physics);
    if (result.empty())
        throw compat::ThrowableException(
            copySimplePhysicsEnforceMessage("Context does not contain a SimplePhysics node"),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/node/simplephysics.d"), 36);
    return result;
}
template<std::size_t N> compat::RetainedString tr(const char (&key)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(key));
}
template<std::size_t N, std::size_t M>
detail::PhysicsCommandText commandText(const char (&label)[N], const char (&description)[M]) {
    auto translatedLabel = tr(label);
    auto translatedDescription = tr(description);
    return {std::move(translatedLabel), std::move(translatedDescription)};
}
}
namespace detail {
compat::Slice<std::shared_ptr<nicxlive::core::nodes::SimplePhysicsDriver>>
collectFloatCommandTargets(std::shared_ptr<Context> context) {
    return simplePhysicsTargets(std::move(context));
}
}
SetSimplePhysicsParameterCommand::SetSimplePhysicsParameterCommand(Parameter value)
    : SetSimplePhysicsParameterCommand(sourceConstructorText(), value) {}
detail::PhysicsCommandText SetSimplePhysicsParameterCommand::sourceConstructorText() {
    return commandText("Set SimplePhysics Parameter", "Assign the parameter driven by the target SimplePhysics node");
}
SetSimplePhysicsParameterCommand::SetSimplePhysicsParameterCommand(detail::PhysicsCommandText text,Parameter value)
    : ExCommand(std::move(text.label), std::move(text.description), value) {
    registerSimplePhysicsCommandClass<SetSimplePhysicsParameterCommand>(
        "nijigenerate.commands.node.simplephysics.SetSimplePhysicsParameterCommand");
}
ClearSimplePhysicsParameterCommand::ClearSimplePhysicsParameterCommand()
    : ClearSimplePhysicsParameterCommand(sourceConstructorText()) {}
detail::PhysicsCommandText ClearSimplePhysicsParameterCommand::sourceConstructorText() {
    return commandText("Clear SimplePhysics Parameter", "Unassign the parameter driven by the target SimplePhysics node");
}
ClearSimplePhysicsParameterCommand::ClearSimplePhysicsParameterCommand(detail::PhysicsCommandText text)
    : ExCommand(std::move(text.label), std::move(text.description)) {
    registerSimplePhysicsCommandClass<ClearSimplePhysicsParameterCommand>(
        "nijigenerate.commands.node.simplephysics.ClearSimplePhysicsParameterCommand");
}
std::shared_ptr<CommandResult> SetSimplePhysicsParameterCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto targets=simplePhysicsTargets(ctx);
    if (!simplePhysicsValueWouldChange<Parameter,getPhysicsParamSource>(targets,parameter))
        return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("SimplePhysics parameter is unchanged"));
    core::actionstack::incActionPush(ngCreateSimplePhysicsValueAction<SimplePhysicsValueChangeAction<Parameter,
        getPhysicsParamSource,setPhysicsParamSource,true>>(compat::RetainedString::fromStaticLiteral("parameter"),targets,parameter));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ClearSimplePhysicsParameterCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    auto targets=simplePhysicsTargets(ctx);
    if (!simplePhysicsValueWouldChange<Parameter,getPhysicsParamSource>(targets,{}))
        return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("SimplePhysics parameter is unchanged"));
    core::actionstack::incActionPush(ngCreateSimplePhysicsValueAction<SimplePhysicsValueChangeAction<Parameter,
        getPhysicsParamSource,setPhysicsParamSource,true>>(compat::RetainedString::fromStaticLiteral("parameter"),targets,Parameter{}));
    return CommandResult::opCall(true);
}
SetSimplePhysicsModelTypeCommand::SetSimplePhysicsModelTypeCommand(PhysicsModel value)
    : SetSimplePhysicsModelTypeCommand(sourceConstructorText(), value) {}
detail::PhysicsCommandText SetSimplePhysicsModelTypeCommand::sourceConstructorText() {
    return commandText("Set SimplePhysics Model Type", "Set SimplePhysics model type");
}
SetSimplePhysicsModelTypeCommand::SetSimplePhysicsModelTypeCommand(detail::PhysicsCommandText text,PhysicsModel value)
    : ExCommand(std::move(text.label),std::move(text.description),value) {
    registerSimplePhysicsCommandClass<SetSimplePhysicsModelTypeCommand>(
        "nijigenerate.commands.node.simplephysics.SetSimplePhysicsModelTypeCommand");
}
std::shared_ptr<CommandResult> SetSimplePhysicsModelTypeCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    core::actionstack::incActionPush(ngCreateSimplePhysicsValueAction<SimplePhysicsValueChangeAction<PhysicsModel,
        getPhysicsModelTypeSource,setPhysicsModelTypeSource>>(ngSimplePhysicsActionArguments([&] {
            const auto property = compat::RetainedString::fromStaticLiteral("modelType");
            const auto targets = simplePhysicsTargets(ctx);
            const auto newValue = modelType;
            return std::tuple{property, targets, newValue};
        })));
    return CommandResult::opCall(true);
}
SetSimplePhysicsMapModeCommand::SetSimplePhysicsMapModeCommand(ParamMapMode value)
    : SetSimplePhysicsMapModeCommand(sourceConstructorText(), value) {}
detail::PhysicsCommandText SetSimplePhysicsMapModeCommand::sourceConstructorText() {
    return commandText("Set SimplePhysics Map Mode", "Set SimplePhysics parameter mapping mode");
}
SetSimplePhysicsMapModeCommand::SetSimplePhysicsMapModeCommand(detail::PhysicsCommandText text,ParamMapMode value)
    : ExCommand(std::move(text.label),std::move(text.description),value) {
    registerSimplePhysicsCommandClass<SetSimplePhysicsMapModeCommand>(
        "nijigenerate.commands.node.simplephysics.SetSimplePhysicsMapModeCommand");
}
std::shared_ptr<CommandResult> SetSimplePhysicsMapModeCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    core::actionstack::incActionPush(ngCreateSimplePhysicsValueAction<SimplePhysicsValueChangeAction<ParamMapMode,
        getPhysicsMapModeSource,setPhysicsMapModeSource>>(ngSimplePhysicsActionArguments([&] {
            const auto property = compat::RetainedString::fromStaticLiteral("mapMode");
            const auto targets = simplePhysicsTargets(ctx);
            const auto newValue = mapMode;
            return std::tuple{property, targets, newValue};
        })));
    return CommandResult::opCall(true);
}
SetSimplePhysicsLocalOnlyCommand::SetSimplePhysicsLocalOnlyCommand(bool value)
    : SetSimplePhysicsLocalOnlyCommand(sourceConstructorText(), value) {}
detail::PhysicsCommandText SetSimplePhysicsLocalOnlyCommand::sourceConstructorText() {
    return commandText("Set SimplePhysics Local Only", "Set whether SimplePhysics listens only to local transform");
}
SetSimplePhysicsLocalOnlyCommand::SetSimplePhysicsLocalOnlyCommand(detail::PhysicsCommandText text,bool value)
    : ExCommand(std::move(text.label),std::move(text.description),value) {
    registerSimplePhysicsCommandClass<SetSimplePhysicsLocalOnlyCommand>(
        "nijigenerate.commands.node.simplephysics.SetSimplePhysicsLocalOnlyCommand");
}
std::shared_ptr<CommandResult> SetSimplePhysicsLocalOnlyCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
    core::actionstack::incActionPush(ngCreateSimplePhysicsValueAction<SimplePhysicsValueChangeAction<bool,
        getPhysicsLocalOnlySource,setPhysicsLocalOnlySource>>(ngSimplePhysicsActionArguments([&] {
            const auto property = compat::RetainedString::fromStaticLiteral("localOnly");
            const auto targets = simplePhysicsTargets(ctx);
            const auto newValue = localOnly;
            return std::tuple{property, targets, newValue};
        })));
    return CommandResult::opCall(true);
}
namespace {
using GravityRecipe = SimplePhysicsFloatCommand<"SetSimplePhysicsGravityCommand", "Set SimplePhysics Gravity",
        "Set SimplePhysics gravity scale", "gravity", getPhysicsGravitySource, setPhysicsGravitySource>;
using LengthRecipe = SimplePhysicsFloatCommand<"SetSimplePhysicsLengthCommand", "Set SimplePhysics Length",
        "Set SimplePhysics pendulum or spring rest length", "length", getPhysicsLengthSource, setPhysicsLengthSource>;
using FrequencyRecipe = SimplePhysicsFloatCommand<"SetSimplePhysicsFrequencyCommand", "Set SimplePhysics Frequency",
        "Set SimplePhysics resonant frequency", "frequency", getPhysicsFrequencySource, setPhysicsFrequencySource>;
using AngleDampingRecipe = SimplePhysicsFloatCommand<"SetSimplePhysicsAngleDampingCommand", "Set SimplePhysics Angle Damping",
        "Set SimplePhysics angular damping ratio", "angleDamping", getPhysicsAngleDampingSource, setPhysicsAngleDampingSource>;
using LengthDampingRecipe = SimplePhysicsFloatCommand<"SetSimplePhysicsLengthDampingCommand", "Set SimplePhysics Length Damping",
        "Set SimplePhysics length damping ratio", "lengthDamping", getPhysicsLengthDampingSource, setPhysicsLengthDampingSource>;
using OutputScaleXRecipe = SimplePhysicsFloatCommand<"SetSimplePhysicsOutputScaleXCommand", "Set SimplePhysics Output Scale X",
        "Set SimplePhysics output scale X", "outputScale.x", getPhysicsOutputScaleXSource, setPhysicsOutputScaleXSource>;
using OutputScaleYRecipe = SimplePhysicsFloatCommand<"SetSimplePhysicsOutputScaleYCommand", "Set SimplePhysics Output Scale Y",
        "Set SimplePhysics output scale Y", "outputScale.y", getPhysicsOutputScaleYSource, setPhysicsOutputScaleYSource>;

template<class C, class Recipe> void registerFloatClass() {
    static const bool registered = [] {
        using Body = typename Recipe::type;
        (void)nicxlive::core::d_runtime::ngObjectTemplateArgumentName<C>();
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(C), Body::sourceTypeName());
        return true;
    }();
    (void)registered;
}
}
SetSimplePhysicsGravityCommand::SetSimplePhysicsGravityCommand(float value)
    : SetSimplePhysicsGravityCommand(sourceConstructorText(), value) {}
detail::PhysicsCommandText SetSimplePhysicsGravityCommand::sourceConstructorText() {
    return commandText("Set SimplePhysics Gravity", "Set SimplePhysics gravity scale");
}
SetSimplePhysicsGravityCommand::SetSimplePhysicsGravityCommand(detail::PhysicsCommandText text, float value)
    : ExCommand(std::move(text.label), std::move(text.description), value) {
    registerFloatClass<SetSimplePhysicsGravityCommand, GravityRecipe>();
}
std::shared_ptr<CommandResult> SetSimplePhysicsGravityCommand::run(std::shared_ptr<Context> ctx) {
    return GravityRecipe::runOn(*this, std::move(ctx));
}
SetSimplePhysicsLengthCommand::SetSimplePhysicsLengthCommand(float value)
    : SetSimplePhysicsLengthCommand(sourceConstructorText(), value) {}
detail::PhysicsCommandText SetSimplePhysicsLengthCommand::sourceConstructorText() {
    return commandText("Set SimplePhysics Length", "Set SimplePhysics pendulum or spring rest length");
}
SetSimplePhysicsLengthCommand::SetSimplePhysicsLengthCommand(detail::PhysicsCommandText text, float value)
    : ExCommand(std::move(text.label), std::move(text.description), value) {
    registerFloatClass<SetSimplePhysicsLengthCommand, LengthRecipe>();
}
std::shared_ptr<CommandResult> SetSimplePhysicsLengthCommand::run(std::shared_ptr<Context> ctx) {
    return LengthRecipe::runOn(*this, std::move(ctx));
}
SetSimplePhysicsFrequencyCommand::SetSimplePhysicsFrequencyCommand(float value)
    : SetSimplePhysicsFrequencyCommand(sourceConstructorText(), value) {}
detail::PhysicsCommandText SetSimplePhysicsFrequencyCommand::sourceConstructorText() {
    return commandText("Set SimplePhysics Frequency", "Set SimplePhysics resonant frequency");
}
SetSimplePhysicsFrequencyCommand::SetSimplePhysicsFrequencyCommand(detail::PhysicsCommandText text, float value)
    : ExCommand(std::move(text.label), std::move(text.description), value) {
    registerFloatClass<SetSimplePhysicsFrequencyCommand, FrequencyRecipe>();
}
std::shared_ptr<CommandResult> SetSimplePhysicsFrequencyCommand::run(std::shared_ptr<Context> ctx) {
    return FrequencyRecipe::runOn(*this, std::move(ctx));
}
SetSimplePhysicsAngleDampingCommand::SetSimplePhysicsAngleDampingCommand(float value)
    : SetSimplePhysicsAngleDampingCommand(sourceConstructorText(), value) {}
detail::PhysicsCommandText SetSimplePhysicsAngleDampingCommand::sourceConstructorText() {
    return commandText("Set SimplePhysics Angle Damping", "Set SimplePhysics angular damping ratio");
}
SetSimplePhysicsAngleDampingCommand::SetSimplePhysicsAngleDampingCommand(detail::PhysicsCommandText text, float value)
    : ExCommand(std::move(text.label), std::move(text.description), value) {
    registerFloatClass<SetSimplePhysicsAngleDampingCommand, AngleDampingRecipe>();
}
std::shared_ptr<CommandResult> SetSimplePhysicsAngleDampingCommand::run(std::shared_ptr<Context> ctx) {
    return AngleDampingRecipe::runOn(*this, std::move(ctx));
}
SetSimplePhysicsLengthDampingCommand::SetSimplePhysicsLengthDampingCommand(float value)
    : SetSimplePhysicsLengthDampingCommand(sourceConstructorText(), value) {}
detail::PhysicsCommandText SetSimplePhysicsLengthDampingCommand::sourceConstructorText() {
    return commandText("Set SimplePhysics Length Damping", "Set SimplePhysics length damping ratio");
}
SetSimplePhysicsLengthDampingCommand::SetSimplePhysicsLengthDampingCommand(detail::PhysicsCommandText text, float value)
    : ExCommand(std::move(text.label), std::move(text.description), value) {
    registerFloatClass<SetSimplePhysicsLengthDampingCommand, LengthDampingRecipe>();
}
std::shared_ptr<CommandResult> SetSimplePhysicsLengthDampingCommand::run(std::shared_ptr<Context> ctx) {
    return LengthDampingRecipe::runOn(*this, std::move(ctx));
}
SetSimplePhysicsOutputScaleXCommand::SetSimplePhysicsOutputScaleXCommand(float value)
    : SetSimplePhysicsOutputScaleXCommand(sourceConstructorText(), value) {}
detail::PhysicsCommandText SetSimplePhysicsOutputScaleXCommand::sourceConstructorText() {
    return commandText("Set SimplePhysics Output Scale X", "Set SimplePhysics output scale X");
}
SetSimplePhysicsOutputScaleXCommand::SetSimplePhysicsOutputScaleXCommand(detail::PhysicsCommandText text, float value)
    : ExCommand(std::move(text.label), std::move(text.description), value) {
    registerFloatClass<SetSimplePhysicsOutputScaleXCommand, OutputScaleXRecipe>();
}
std::shared_ptr<CommandResult> SetSimplePhysicsOutputScaleXCommand::run(std::shared_ptr<Context> ctx) {
    return OutputScaleXRecipe::runOn(*this, std::move(ctx));
}
SetSimplePhysicsOutputScaleYCommand::SetSimplePhysicsOutputScaleYCommand(float value)
    : SetSimplePhysicsOutputScaleYCommand(sourceConstructorText(), value) {}
detail::PhysicsCommandText SetSimplePhysicsOutputScaleYCommand::sourceConstructorText() {
    return commandText("Set SimplePhysics Output Scale Y", "Set SimplePhysics output scale Y");
}
SetSimplePhysicsOutputScaleYCommand::SetSimplePhysicsOutputScaleYCommand(detail::PhysicsCommandText text, float value)
    : ExCommand(std::move(text.label), std::move(text.description), value) {
    registerFloatClass<SetSimplePhysicsOutputScaleYCommand, OutputScaleYRecipe>();
}
std::shared_ptr<CommandResult> SetSimplePhysicsOutputScaleYCommand::run(std::shared_ptr<Context> ctx) {
    return OutputScaleYRecipe::runOn(*this, std::move(ctx));
}
void ngInitSimplePhysicsCommands() {
    initialization::Program::run<initialization::Table>();
}
}
