// Independent source/nijigenerate/commands/parameter/paramedit.d complete bodies.
#include "paramedit_commands.hpp"
#include "paramedit_table.hpp"
#include "base.hpp"
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/actions/binding_add_remove_action.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <param/binding_impl.hpp>
#include <resource.hpp>
#include <compat/source_allocator.hpp>
#include <compat/object_metadata.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>

namespace nijigenerate::commands::parameter::paramedit {
namespace pa = actions::parameter;
namespace stack = core::actionstack;
namespace runtime = nicxlive::core::param;
using ParameterRef = std::shared_ptr<runtime::Parameter>;
using Vec2 = nicxlive::core::math::Vec2;
namespace {
// Keep the Source array argument unevaluated until the real Result constructor
// is invoked by SourceAllocator::construct, after its object allocation.
struct ResultParameterSliceArgument {
    const ParameterRef& value;
    operator compat::Slice<ParameterRef>() const { return {value}; }
};

template<std::size_t N> compat::RetainedString literal(const char (&value)[N]) {
    return compat::RetainedString::fromStaticLiteral(value);
}
template<std::size_t N> compat::RetainedString tr(const char (&value)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(value));
}
detail::KeypointCommandText keypointCommandText(bool armed) {
    auto label = armed ? tr("Set Armed Parameter and Keypoint") : tr("Set Parameter Keypoint");
    auto description = armed
        ? tr("Set the target parameter to context.parameterValue and make it the GUI ArmedParameter. Prefer context.armedParameters[0]; if omitted, uses the current ArmedParameter. Use SetParameterKeypoint when no GUI ArmedParameter change is desired.")
        : tr("Set context.parameters[0] to context.parameterValue without changing bindings or GUI ArmedParameter. Use SetArmedParameterAndKeypoint when the command should also arm the parameter.");
    return {std::move(label), std::move(description)};
}
void notifyParameter() { viewport::model::deform::ngViewportNodeDeformNotifyParamValueChanged(); }
float& axis(Vec2& value, std::int32_t index, std::size_t line) {
    if (index == 0) return value.x;
    if (index == 1) return value.y;
    throw compat::ngStaticError<compat::ArrayIndexError>(static_cast<std::size_t>(index), 2,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/parameter/paramedit.d"), line);
}
template<class Array> decltype(auto) sourceAt(Array& array, std::size_t index, std::size_t line) {
    if (index >= array.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index, array.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/parameter/paramedit.d"), line);
    return array[index];
}
ParameterRef parameterAtZero(const std::shared_ptr<Context>& context, std::size_t line) {
    auto parameters = compat::ngRequireObject(context)->parameters();
    return sourceAt(parameters, 0, line);
}
template<class F> std::shared_ptr<CommandResult> forParameters(std::shared_ptr<Context> context, F function) {
    if (!compat::ngRequireObject(context)->hasParameters() || context->parameters().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No parameters"));
    const auto parameters = context->parameters();
    for (const auto parameter : parameters) function(parameter);
    return CommandResult::opCall(true);
}
template<std::size_t N> std::shared_ptr<CommandResult> flipParameters(
        std::shared_ptr<Context> context, std::uint32_t index, const char (&text)[N]) {
    return forParameters(context, [&](ParameterRef parameter) {
        auto action = pa::ngCreateParameterBindingsAction<pa::ParameterChangeBindingsAction>(
            literal(text), parameter, pa::Bindings{});
        compat::ngRequireObject(parameter)->reverseAxis(index);
        action->updateNewState(); stack::incActionPush(action);
    });
}
bool findParameterIndex(ParameterRef parameter, std::size_t& index) {
    index = 0; // D out parameter initialization.
    if (!parameter) return false;
    if (auto extended = std::dynamic_pointer_cast<ext::ExParameter>(parameter)) {
        if (auto parent = extended->getParent()) {
            const auto children = parent->children;
            const auto length = std::bit_cast<std::ptrdiff_t>(children.size());
            for (std::ptrdiff_t i = 0; i < length; ++i)
                if (nicxlive::core::ngResourceObjectEquals(
                        children[static_cast<std::size_t>(i)], parameter)) {
                    index = static_cast<std::size_t>(i); return true;
                }
        }
    }
    if (auto puppet = project::incActivePuppet()) {
        const auto parameters = puppet->parameters;
        const auto length = std::bit_cast<std::ptrdiff_t>(parameters.size());
        for (std::ptrdiff_t i = 0; i < length; ++i)
            if (nicxlive::core::ngResourceObjectEquals(
                    parameters[static_cast<std::size_t>(i)], parameter)) {
                index = static_cast<std::size_t>(i); return true;
            }
    }
    return false;
}
std::shared_ptr<CommandResult> resolveParameterKeypointValue(std::shared_ptr<Context> context,
    ParameterRef parameter, runtime::Vec2u& point, Vec2& resolved) {
    point = {}; resolved = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    if (!parameter) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("No parameter"));
    if (compat::ngRequireObject(context)->hasParameterValue()) {
        point = parameter->findClosestKeypoint(context->parameterValue());
        if (point.x >= parameter->axisPointCount(0) || point.y >= parameter->axisPointCount(1))
            return CommandResult::opCall(false,
                compat::RetainedString::fromStaticLiteral("context.parameterValue resolved keypoint is out of range"));
        resolved = parameter->getKeypointValue(point);
        constexpr float epsilon = 1e-5f;
        if (std::abs(resolved.x - context->parameterValue().x) > epsilon ||
            std::abs(resolved.y - context->parameterValue().y) > epsilon)
            return CommandResult::opCall(false,
                compat::RetainedString::fromStaticLiteral("context.parameterValue does not match an existing key value"));
    } else if (context->hasKeyPoint() && context->hasExplicitKeyPoint()) {
        const auto value = context->keyPoint(); point = {value.x, value.y};
        if (point.x >= parameter->axisPointCount(0) || point.y >= parameter->axisPointCount(1))
            return CommandResult::opCall(false,
                compat::RetainedString::fromStaticLiteral("keyPoint is out of range for parameter"));
        resolved = parameter->getKeypointValue(point);
    } else return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("context.parameterValue is required"));
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> applyParameterKeypoint(ParameterRef parameter, runtime::Vec2u point,
    Vec2 resolved, bool armParameter) {
    (void)point;
    std::size_t index;
    if (!findParameterIndex(parameter, index)) return CommandResult::opCall(false,
        compat::RetainedString::fromStaticLiteral("Parameter not found in active puppet"));
    if (project::incEditMode() != project::EditMode::ModelEdit) project::incSetEditMode(project::EditMode::ModelEdit, false);
    parameter->value = resolved; binding::paramPointChanged(parameter);
    if (armParameter) project::incArmParameter(index, parameter);
    notifyParameter(); return CommandResult::opCall(true);
}
}
#define NG_PARAMEDIT_CLASS_INFO(Type) nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Type), \
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit." #Type))
#define NG_PARAMETER_CTOR(Type, Text) \
compat::RetainedString Type::sourceConstructorDescription() { return tr(Text); } \
Type::Type() : Type(sourceConstructorDescription()) {} \
Type::Type(compat::RetainedString description) : ExCommand(std::move(description)) { NG_PARAMEDIT_CLASS_INFO(Type); }
NG_PARAMETER_CTOR(ConvertTo2DParamCommand, "Convert to 2D")
NG_PARAMETER_CTOR(FlipXCommand, "Flip X")
NG_PARAMETER_CTOR(FlipYCommand, "Flip X")
NG_PARAMETER_CTOR(Flip1DCommand, "Flip")
NG_PARAMETER_CTOR(MirrorHorizontallyCommand, "Mirror Horizontally")
NG_PARAMETER_CTOR(MirrorVerticallyCommand, "Mirror Vertically")
NG_PARAMETER_CTOR(MirroredAutoFillDir1Command, "Mirrored Autofill \xEE\x97\x84")
NG_PARAMETER_CTOR(MirroredAutoFillDir2Command, "Mirrored Autofill \xEE\x97\x88")
NG_PARAMETER_CTOR(MirroredAutoFillDir3Command, "Mirrored Autofill \xEE\x97\x98")
NG_PARAMETER_CTOR(MirroredAutoFillDir4Command, "Mirrored Autofill \xEE\x97\x9B")
NG_PARAMETER_CTOR(CopyParameterCommand, "Copy Parameter")
NG_PARAMETER_CTOR(PasteParameterCommand, "Paste Parameter")
NG_PARAMETER_CTOR(PasteParameterWithFlipCommand, "Paste Parameter with Flip")
#undef NG_PARAMETER_CTOR
compat::RetainedString DuplicateParameterCommand::sourceConstructorDescription() { return tr("Duplicate Parameter"); }
DuplicateParameterCommand::DuplicateParameterCommand() : DuplicateParameterCommand(sourceConstructorDescription()) {}
DuplicateParameterCommand::DuplicateParameterCommand(compat::RetainedString description) : NativeBase(std::move(description)) {
    NG_PARAMEDIT_CLASS_INFO(DuplicateParameterCommand);
}
compat::RetainedString DuplicateParameterWithFlipCommand::sourceConstructorDescription() {
    return tr("Duplicate Parameter with Flip");
}
DuplicateParameterWithFlipCommand::DuplicateParameterWithFlipCommand()
    : DuplicateParameterWithFlipCommand(sourceConstructorDescription()) {}
DuplicateParameterWithFlipCommand::DuplicateParameterWithFlipCommand(compat::RetainedString description)
    : NativeBase(std::move(description)) {
    NG_PARAMEDIT_CLASS_INFO(DuplicateParameterWithFlipCommand);
}
compat::RetainedString DeleteParameterCommand::sourceConstructorDescription() { return tr("Delete Parameter"); }
DeleteParameterCommand::DeleteParameterCommand() : DeleteParameterCommand(sourceConstructorDescription()) {}
DeleteParameterCommand::DeleteParameterCommand(compat::RetainedString description) : NativeBase(std::move(description)) {
    NG_PARAMEDIT_CLASS_INFO(DeleteParameterCommand);
}
LinkToCommand::LinkToCommand(ParameterRef target, std::int32_t sourceAxis, std::int32_t targetAxis)
    : LinkToCommand(SourceConstructorFrame::capture(target, sourceAxis, targetAxis)) {}
LinkToCommand::SourceConstructorFrame LinkToCommand::SourceConstructorFrame::capture(
        ParameterRef& target, std::int32_t& sourceAxis, std::int32_t& targetAxis) {
    return {tr("Link To Parameter"), target, sourceAxis, targetAxis};
}
LinkToCommand::LinkToCommand(SourceConstructorFrame frame)
    : ExCommand({}, std::move(frame.description), frame.target, frame.fromAxis, frame.toAxis) {
    NG_PARAMEDIT_CLASS_INFO(LinkToCommand);
}
ToggleParameterArmCommand::ToggleParameterArmCommand(std::int32_t position)
    : ToggleParameterArmCommand(SourceConstructorFrame::capture(position)) {}
ToggleParameterArmCommand::SourceConstructorFrame ToggleParameterArmCommand::SourceConstructorFrame::capture(
        std::int32_t& position) {
    return {tr("Arm/Disarm the selected parameter at index"), position};
}
ToggleParameterArmCommand::ToggleParameterArmCommand(SourceConstructorFrame frame)
    : ExCommand({}, std::move(frame.description), frame.index) {
    NG_PARAMEDIT_CLASS_INFO(ToggleParameterArmCommand);
}
SetParameterKeypointCommand::SetParameterKeypointCommand()
    : SetParameterKeypointCommand(sourceConstructorText()) {}
detail::KeypointCommandText SetParameterKeypointCommand::sourceConstructorText() { return keypointCommandText(false); }
SetParameterKeypointCommand::SetParameterKeypointCommand(detail::KeypointCommandText text)
    : ExCommand(std::move(text.label), std::move(text.description)) { NG_PARAMEDIT_CLASS_INFO(SetParameterKeypointCommand); }
SetArmedParameterAndKeypointCommand::SetArmedParameterAndKeypointCommand()
    : SetArmedParameterAndKeypointCommand(sourceConstructorText()) {}
detail::KeypointCommandText SetArmedParameterAndKeypointCommand::sourceConstructorText() { return keypointCommandText(true); }
SetArmedParameterAndKeypointCommand::SetArmedParameterAndKeypointCommand(detail::KeypointCommandText text)
    : ExCommand(std::move(text.label), std::move(text.description)) {
    NG_PARAMEDIT_CLASS_INFO(SetArmedParameterAndKeypointCommand);
}
SetStartingKeyFrameCommand::SetStartingKeyFrameCommand()
    : SetStartingKeyFrameCommand(sourceConstructorDescription()) {}
compat::RetainedString SetStartingKeyFrameCommand::sourceConstructorDescription() {
    return tr("Set the current parameter value as starting keyframe");
}
SetStartingKeyFrameCommand::SetStartingKeyFrameCommand(compat::RetainedString description)
    : ExCommand({}, std::move(description)) {
    NG_PARAMEDIT_CLASS_INFO(SetStartingKeyFrameCommand);
}
#undef NG_PARAMEDIT_CLASS_INFO
#define NG_PARAMEDIT_CLASS_HOOK(Type, Base) \
void Type::prepareSourceClassInfo() { \
    Base::prepareSourceClassInfo(); \
    ::nijigenerate::commands::detail::prepareCommandResultClass<Type>(); \
}
NG_PARAMEDIT_CLASS_HOOK(ConvertTo2DParamCommand, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(FlipXCommand, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(FlipYCommand, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(Flip1DCommand, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(MirrorHorizontallyCommand, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(MirrorVerticallyCommand, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(MirroredAutoFillDir1Command, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(MirroredAutoFillDir2Command, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(MirroredAutoFillDir3Command, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(MirroredAutoFillDir4Command, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(CopyParameterCommand, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(PasteParameterCommand, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(PasteParameterWithFlipCommand, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(DuplicateParameterCommand, NativeBase)
NG_PARAMEDIT_CLASS_HOOK(DuplicateParameterWithFlipCommand, NativeBase)
NG_PARAMEDIT_CLASS_HOOK(DeleteParameterCommand, NativeBase)
NG_PARAMEDIT_CLASS_HOOK(LinkToCommand, ExCommand<LinkToArguments>)
NG_PARAMEDIT_CLASS_HOOK(ToggleParameterArmCommand, ExCommand<ToggleParameterArmArguments>)
NG_PARAMEDIT_CLASS_HOOK(SetParameterKeypointCommand, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(SetArmedParameterAndKeypointCommand, ExCommand<>)
NG_PARAMEDIT_CLASS_HOOK(SetStartingKeyFrameCommand, ExCommand<>)
#undef NG_PARAMEDIT_CLASS_HOOK
std::shared_ptr<CommandResult> ConvertTo2DParamCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return forParameters(context, base::convertTo2D);
}
std::shared_ptr<CommandResult> FlipXCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return flipParameters(context, 0, "Flip X");
}
std::shared_ptr<CommandResult> FlipYCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return flipParameters(context, 1, "Flip Y");
}
std::shared_ptr<CommandResult> Flip1DCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return flipParameters(context, 0, "Flip");
}
std::shared_ptr<CommandResult> MirrorHorizontallyCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return forParameters(context, [](ParameterRef p) { base::mirrorAll(p, 0); notifyParameter(); });
}
std::shared_ptr<CommandResult> MirrorVerticallyCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return forParameters(context, [](ParameterRef p) { base::mirrorAll(p, 1); notifyParameter(); });
}
#define NG_AUTOFILL(Type, Axis, Low, High) \
std::shared_ptr<CommandResult> Type::run(std::shared_ptr<Context> context) { \
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock(); \
    return forParameters(context, [](ParameterRef p) { base::mirroredAutofill(p, Axis, Low, High); notifyParameter(); }); \
}
NG_AUTOFILL(MirroredAutoFillDir1Command, 0, 0, 0.4999f)
NG_AUTOFILL(MirroredAutoFillDir2Command, 0, 0.5001f, 1)
NG_AUTOFILL(MirroredAutoFillDir3Command, 1, 0.5001f, 1)
NG_AUTOFILL(MirroredAutoFillDir4Command, 1, 0, 0.4999f)
#undef NG_AUTOFILL
std::shared_ptr<CommandResult> CopyParameterCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasParameters() || context->parameters().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No parameters"));
    const auto parameter = parameterAtZero(context, 170);
    binding::cClipboardParameter = compat::ngRequireObject(parameter)->dup();
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> PasteParameterCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasParameters() || context->parameters().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No parameters"));
    const auto parameter = parameterAtZero(context, 182);
    base::pasteParameter(parameter, {}, 2); notifyParameter(); return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> PasteParameterWithFlipCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasParameters() || context->parameters().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No parameters"));
    const auto parameter = parameterAtZero(context, 199);
    base::pasteParameter(parameter, {}, 0); notifyParameter(); return CommandResult::opCall(true);
}
bool PasteParameterCommand::runnable(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return compat::ngRequireObject(context)->hasParameters() && !context->parameters().empty()
        && binding::cClipboardParameter != nullptr;
}
bool PasteParameterWithFlipCommand::runnable(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return compat::ngRequireObject(context)->hasParameters() && !context->parameters().empty()
        && binding::cClipboardParameter != nullptr;
}
std::shared_ptr<DuplicateParameterCommand::Result> DuplicateParameterCommand::run(
        const std::shared_ptr<Context>& context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return runTyped(context);
}
std::shared_ptr<DuplicateParameterWithFlipCommand::Result> DuplicateParameterWithFlipCommand::run(
        const std::shared_ptr<Context>& context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return runTyped(context);
}
std::shared_ptr<DeleteParameterCommand::Result> DeleteParameterCommand::run(
        const std::shared_ptr<Context>& context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return runTyped(context);
}
std::shared_ptr<DuplicateParameterCommand::Result> DuplicateParameterCommand::runTyped(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    if (!(compat::ngRequireObject(context)->hasPuppet() && context->hasParameters()) || context->parameters().empty())
        return Result::opCall(false, {}, literal("No parameters"));
    const auto parameter = parameterAtZero(context, 216); const auto created = compat::ngRequireObject(parameter)->dup();
    compat::ngRequireObject(context->puppet())->parameters.push_back(created);
    if (auto extended = std::dynamic_pointer_cast<ext::ExParameter>(created)) {
        const auto original = std::dynamic_pointer_cast<ext::ExParameter>(parameter);
        extended->setParent(compat::ngRequireObject(original)->getParent());
    }
    stack::incActionPush(std::allocate_shared<pa::ParameterAddAction>(
        nicxlive::core::d_runtime::SourceAllocator<pa::ParameterAddAction>{}, created));
    return std::allocate_shared<Result>(nicxlive::core::d_runtime::SourceAllocator<Result>{},
        true, ResultParameterSliceArgument{created}, literal("Parameter duplicated"));
}
std::shared_ptr<DuplicateParameterWithFlipCommand::Result> DuplicateParameterWithFlipCommand::runTyped(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    if (!(compat::ngRequireObject(context)->hasPuppet() && context->hasParameters()) || context->parameters().empty())
        return Result::opCall(false, {}, literal("No parameters"));
    const auto parameter = parameterAtZero(context, 235); const auto created = compat::ngRequireObject(parameter)->dup();
    compat::ngRequireObject(context->puppet())->parameters.push_back(created);
    compat::ngRequireObject(created)->bindings.resize(0);
    base::pasteParameter<false>(created, parameter, 0);
    if (auto extended = std::dynamic_pointer_cast<ext::ExParameter>(created)) {
        const auto original = std::dynamic_pointer_cast<ext::ExParameter>(parameter);
        extended->setParent(compat::ngRequireObject(original)->getParent());
    }
    stack::incActionPush(std::allocate_shared<pa::ParameterAddAction>(
        nicxlive::core::d_runtime::SourceAllocator<pa::ParameterAddAction>{}, created,
        static_cast<nicxlive::core::common::SharedArray<ParameterRef>*>(nullptr)));
    return std::allocate_shared<Result>(nicxlive::core::d_runtime::SourceAllocator<Result>{},
        true, ResultParameterSliceArgument{created}, literal("Parameter duplicated with flip"));
}
std::shared_ptr<DeleteParameterCommand::Result> DeleteParameterCommand::runTyped(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    if (!(compat::ngRequireObject(context)->hasPuppet() && context->hasParameters()) || context->parameters().empty())
        return Result::opCall(false, {}, literal("No parameters"));
    const auto parameter = parameterAtZero(context, 256);
    if (nicxlive::core::ngResourceObjectEquals(project::incArmedParameter(), parameter)) project::incDisarmParameter();
    if (project::incParamInSelection(parameter)) project::incRemoveSelectParam(parameter);
    stack::incActionPush(std::allocate_shared<pa::ParameterRemoveAction>(
        nicxlive::core::d_runtime::SourceAllocator<pa::ParameterRemoveAction>{}, parameter));
    compat::ngRequireObject(context->puppet())->removeParameter(parameter);
    return std::allocate_shared<Result>(nicxlive::core::d_runtime::SourceAllocator<Result>{},
        true, ResultParameterSliceArgument{parameter}, literal("Parameter deleted"));
}
void addBinding(ParameterRef parameter, ParameterRef target, std::int32_t fromAxis, std::int32_t toAxis) {
    using BindingStorage = runtime::ParameterBindingImpl<float>;
    const auto value = compat::ngRequireObject(parameter)->createBinding(target, toAxis);
    compat::ngRequireObject(parameter)->addBinding(value);
    const auto binding = std::dynamic_pointer_cast<runtime::ParameterParameterBinding>(value);
    Vec2 position{0, 0};
    auto& minimumCoordinateDestination = axis(position, fromAxis, 279);
    minimumCoordinateDestination = axis(compat::ngRequireObject(parameter)->min, fromAxis, 279);
    auto index = compat::ngRequireObject(parameter)->findClosestKeypoint(position);
    auto& minimumValueDestination =
        sourceAt(sourceAt(compat::ngRequireObject(binding)->values, index.x, 281), index.y, 281);
    minimumValueDestination = axis(compat::ngRequireObject(target)->min, toAxis, 281);
    sourceAt(sourceAt(compat::ngRequireObject(binding)->BindingStorage::getIsSet(), index.x, 282), index.y, 282) = true;
    position = {0, 0};
    auto& maximumCoordinateDestination = axis(position, fromAxis, 285);
    maximumCoordinateDestination = axis(compat::ngRequireObject(parameter)->max, fromAxis, 285);
    index = compat::ngRequireObject(parameter)->findClosestKeypoint(position);
    auto& maximumValueDestination =
        sourceAt(sourceAt(compat::ngRequireObject(binding)->values, index.x, 287), index.y, 287);
    maximumValueDestination = axis(compat::ngRequireObject(target)->max, toAxis, 287);
    sourceAt(sourceAt(compat::ngRequireObject(binding)->BindingStorage::getIsSet(), index.x, 288), index.y, 288) = true;
    compat::ngRequireObject(binding)->reInterpolate();
    stack::incActionPush(actions::binding::ngCreateBindingAddRemoveAction<actions::binding::ParameterBindingAddAction>(parameter, value));
}
std::shared_ptr<CommandResult> LinkToCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasParameters() || context->parameters().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No parameters"));
    const auto parameter = parameterAtZero(context, 301);
    addBinding(parameter, toParam, fromAxis, toAxis); return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> ToggleParameterArmCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    if (!(compat::ngRequireObject(context)->hasPuppet() && context->hasParameters()) || context->parameters().empty())
        return CommandResult::opCall(false, literal("No parameters"));
    auto parameter = parameterAtZero(context, 314);
    if (nicxlive::core::ngResourceObjectEquals(project::incArmedParameter(), parameter)) project::incDisarmParameter();
    else {
        const auto destination = compat::ngRequireObject(parameter);
        const auto value = compat::ngRequireObject(parameter)->getClosestKeypointValue();
        destination->value = value;
        binding::paramPointChanged(parameter); project::incArmParameter(static_cast<std::size_t>(index), parameter);
    }
    return CommandResult::opCall(true);
}
std::shared_ptr<CommandResult> SetParameterKeypointCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasParameters() || context->parameters().empty())
        return CommandResult::opCall(false, literal("SetParameterKeypoint requires context.parameters[0]"));
    const auto parameter = parameterAtZero(context, 363); runtime::Vec2u point{}; Vec2 resolved;
    const auto result = resolveParameterKeypointValue(context, parameter, point, resolved);
    if (!result->succeeded) return result;
    return applyParameterKeypoint(parameter, point, resolved, false);
}
std::shared_ptr<CommandResult> SetArmedParameterAndKeypointCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    ParameterRef parameter;
    if (compat::ngRequireObject(context)->hasArmedParameters() && !context->armedParameters().empty()) {
        auto parameters = context->armedParameters();
        parameter = sourceAt(parameters, 0, 387);
    } else parameter = project::incArmedParameter();
    if (!parameter) return CommandResult::opCall(false, literal("No parameter"));
    runtime::Vec2u point{}; Vec2 resolved;
    const auto result = resolveParameterKeypointValue(context, parameter, point, resolved);
    if (!result->succeeded) return result;
    return applyParameterKeypoint(parameter, point, resolved, true);
}
std::shared_ptr<CommandResult> SetStartingKeyFrameCommand::run(std::shared_ptr<Context> context) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasParameters() || context->parameters().empty())
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No parameters"));
    const auto parameter = parameterAtZero(context, 449);
    auto action = pa::ngCreateParameterValueActionDeferred<pa::ParameterValueChangeAction<Vec2>>(
        [] { return compat::RetainedString::fromStaticLiteral("axis points"); },
        [&] { return parameter; },
        [&] {
            const auto defaultsOwner = compat::ngRequireObject(parameter);
            return std::shared_ptr<Vec2>(defaultsOwner, &defaultsOwner->defaults);
        });
    parameter->defaults = parameter->value; action->updateNewState(); stack::incActionPush(action);
    return CommandResult::opCall(true);
}
void ngInitParameterEditCommands() {
    initialization::Program::run<initialization::Table>();
}
} // namespace nijigenerate::commands::parameter::paramedit
