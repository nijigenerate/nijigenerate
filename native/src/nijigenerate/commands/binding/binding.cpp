#include "binding.hpp"
#include "base.hpp"
#include "binding_value.hpp"
#include "detail/registration_diagnostic.hpp"
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/actions/node_value_action.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/math/triangle_resampling.hpp>
#include <nijigenerate/viewport/model/deform.hpp>
#include <nijigenerate/windows/flipconfig.hpp>
#include <nijigenerate/utils/transform.hpp>
#include <param/binding_impl.hpp>
#include <nodes/deformable.hpp>
#include <compat/source_allocator.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <cmath>
#include <string>
#include <string_view>
#include <new>
#include <source_location>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace nijigenerate::commands::binding {
namespace rt = nicxlive::core::param;
namespace pa = actions::parameter;
namespace stack = core::actionstack;
namespace transform = utils::transform;
using Param = std::shared_ptr<Parameter>;
using Binding = std::shared_ptr<ParameterBinding>;
using Bindings = compat::Slice<Binding>;
using Key = rt::Vec2u;
namespace {
template<std::size_t N> compat::RetainedString tr(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}
void notify() { nijigenerate::viewport::model::deform::ngViewportNodeDeformNotifyParamValueChanged(); }
template<class Range> decltype(auto) sourceAt(Range& values, std::size_t index, std::size_t line) {
    if(index>=values.size()) throw compat::ngStaticError<compat::ArrayIndexError>(index,values.size(),
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/binding/binding.d"),line);
    return values[index];
}
rt::DeformSlot& deformationAt(const std::shared_ptr<rt::DeformationParameterBinding>& binding,
        Key point,std::size_t line) {
    auto& row=sourceAt(compat::ngRequireObject(binding)->values,point.x,line);
    return sourceAt(row,point.y,line);
}
template<std::size_t N>
std::shared_ptr<pa::ParameterChangeBindingsValueAction> action(const char (&name)[N], Param param, Bindings bindings, Key point) {
    return pa::ngCreateParameterBindingsAction<pa::ParameterChangeBindingsValueAction>(
        compat::RetainedString::fromStaticLiteral(name),param,bindings,
        static_cast<int>(point.x),static_cast<int>(point.y));
}
std::shared_ptr<CommandResult> ngResolveBindingKeyContext(std::shared_ptr<Context> ctx, Param& param,
        Bindings& bindings, Key& kp, bool& targetNull) {
    param.reset(); bindings = {}; kp = {0, 0}; targetNull = false;
    targetNull = !compat::ngRequireObject(ctx)->hasActiveBindings() || ctx->activeBindings().isNull();
    if (ctx->hasArmedParameters() && !ctx->armedParameters().empty()) {
        if (ctx->armedParameters().size() != 1) return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("Binding key command requires exactly one armed parameter"));
        const auto parameters = ctx->armedParameters();
        if (parameters.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, parameters.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/binding/binding.d"), 69);
        param = parameters[0];
    } else if (ctx->hasParameters() && !ctx->parameters().empty()) {
        if (ctx->parameters().size() != 1) return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("Binding key command requires exactly one parameter"));
        const auto parameters = ctx->parameters();
        if (parameters.empty()) throw compat::ngStaticError<compat::ArrayIndexError>(0, parameters.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/binding/binding.d"), 73);
        param = parameters[0];
    } else return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No armed parameter"));
    if ((!ctx->hasBindings() && !ctx->hasActiveBindings()) || (!targetNull && ctx->activeBindings().empty()))
        return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No bindings"));
    bindings = !targetNull ? ctx->activeBindings() : ctx->bindings();
    if (bindings.empty()) return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No bindings"));
    if (ctx->hasParameterValue()) {
        const auto receiver = param;
        const auto value = ctx->parameterValue();
        kp = {};
        compat::Vec2u point;
        std::optional<std::string_view> message;
        if (!detail::ngResolveBindingParameterValueToKeyPoint(receiver, value, point, message))
            return CommandResult::opCall(false, compat::RetainedString::fromStaticView(*message));
        kp = {point.x, point.y};
    } else if (ctx->hasKeyPoint()) {
        const auto x = ctx->keyPoint().x;
        if (x >= compat::ngRequireObject(param)->axisPointCount(0))
            return CommandResult::opCall(false,
                compat::RetainedString::fromStaticLiteral("keyPoint is out of range for parameter"));
        const auto y = ctx->keyPoint().y;
        if (y >= compat::ngRequireObject(param)->axisPointCount(1))
            return CommandResult::opCall(false,
                compat::RetainedString::fromStaticLiteral("keyPoint is out of range for parameter"));
        const auto point = ctx->keyPoint();
        kp = {point.x, point.y};
    } else return CommandResult::opCall(false, compat::RetainedString::fromStaticLiteral("No keypoint"));
    return CommandResult::opCall(true);
}
template<std::size_t N, class F>
std::shared_ptr<CommandResult> change(std::shared_ptr<Context> ctx, const char (&name)[N], F function) {
    Param param; Bindings bindings; Key kp; bool targetNull;
    const auto resolved = ngResolveBindingKeyContext(ctx, param, bindings, kp, targetNull); if (!resolved->succeeded) return resolved;
    const auto undo = action(name, param, bindings, kp);
    for (auto binding : bindings) function(binding, kp);
    undo->updateNewState(); stack::incActionPush(undo); notify(); return CommandResult::opCall(true);
}
template<std::size_t N>
std::shared_ptr<CommandResult> mirror(std::shared_ptr<Context> ctx, const char (&name)[N], int axis, bool oneDimensional) {
    Param param; Bindings bindings; Key kp; bool targetNull;
    const auto resolved = ngResolveBindingKeyContext(ctx, param, bindings, kp, targetNull); if (!resolved->succeeded) return resolved;
    stack::incActionPushGroup(); const auto undo = action(name, param, bindings, kp);
    for (auto binding : bindings) {
        if (auto target = compat::ngRequireObject(binding)->getTarget().target.lock()) {
            const auto pair = windows::flipconfig::ngGetFlipPairFor(target);
            const auto paired = transform::ngBindingGetPairFor(param, target, pair,
                compat::RetainedString(binding->getTarget().name), true);
            const compat::Vec2u point{static_cast<std::uint32_t>(kp.x),static_cast<std::uint32_t>(kp.y)};
            if (!oneDimensional || targetNull) transform::ngBindingAutoFlip(binding, paired, point, axis);
            else if (paired) transform::ngBindingAutoFlip(paired, binding, point, axis);
        }
    }
    undo->updateNewState(); stack::incActionPush(undo); stack::incActionPopGroup(); notify();
    return CommandResult::opCall(true);
}
Binding findTarget(Param param, const BindingTarget& target) {
    return compat::ngRequireObject(param)->getBinding(target.target.getResource(),target.name);
}
Binding ensureTarget(Param param, const BindingTarget& target) {
    return compat::ngRequireObject(param)->getOrAddBinding(target.target.getResource(),target.name);
}
}
#define NG_CTOR(Type) Type::Type() : ExCommand(detail::BindingSourceConstructor<Type>::description()) {}
NG_CTOR(UnsetKeyFrameCommand) NG_CTOR(SetKeyFrameCommand)
NG_CTOR(ResetKeyFrameCommand) NG_CTOR(InvertKeyFrameCommand)
NG_CTOR(MirrorKeyFrameHorizontallyCommand)
NG_CTOR(MirrorKeyFrameVerticallyCommand)
NG_CTOR(FlipDeformCommand) NG_CTOR(SymmetrizeDeformCommand)
NG_CTOR(SetFromHorizontalMirrorCommand)
NG_CTOR(SetFromVerticalMirrorCommand)
NG_CTOR(SetFromDiagonalMirrorCommand) NG_CTOR(SetFrom1DMirrorCommand)
NG_CTOR(CopyBindingCommand)
#undef NG_CTOR
PasteBindingCommand::PasteBindingCommand()
    : NativeBase(detail::BindingSourceConstructor<PasteBindingCommand>::description()) {}
RemoveBindingCommand::RemoveBindingCommand()
    : NativeBase({}, detail::BindingSourceConstructor<RemoveBindingCommand>::description()) {}
std::shared_ptr<SetInterpolationCommand> detail::BindingSourceConstructor<SetInterpolationCommand>::construct(Mode mode) {
    return std::allocate_shared<SetInterpolationCommand>(
        nicxlive::core::d_runtime::SourceAllocator<SetInterpolationCommand>{}, mode);
}
compat::RetainedString detail::BindingSourceConstructor<SetInterpolationCommand>::description(Mode) {
    auto text = core::translation::ngFormatTyped(tr("Set Bindings to %s").view(),
        std::array{core::translation::ngFormatArgument(compat::RetainedString::fromStaticLiteral("mode"))});
    try {
        return compat::RetainedString(std::move(text));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
SetInterpolationCommand::SetInterpolationCommand(nicxlive::core::common::InterpolateMode value)
    : ExCommand({}, detail::BindingSourceConstructor<SetInterpolationCommand>::description(value), value) {}
#define NG_CHANGE(Type, Name, Body) std::shared_ptr<CommandResult> Type::run(std::shared_ptr<Context> ctx) { \
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock(); \
    return change(ctx, Name, [](Binding binding, Key point) { Body; }); }
NG_CHANGE(UnsetKeyFrameCommand,"unset",compat::ngRequireObject(binding)->unset(point))
NG_CHANGE(SetKeyFrameCommand,"setCurrent",compat::ngRequireObject(binding)->setCurrent(point))
NG_CHANGE(ResetKeyFrameCommand,"reset",compat::ngRequireObject(binding)->reset(point))
NG_CHANGE(InvertKeyFrameCommand,"invert",compat::ngRequireObject(binding)->scaleValueAt(point,-1,-1))
NG_CHANGE(MirrorKeyFrameHorizontallyCommand,"mirror Horizontally",compat::ngRequireObject(binding)->scaleValueAt(point,0,-1))
NG_CHANGE(MirrorKeyFrameVerticallyCommand,"mirror Vertically",compat::ngRequireObject(binding)->scaleValueAt(point,1,-1))
#undef NG_CHANGE
std::shared_ptr<CommandResult> FlipDeformCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return change(ctx,"Flip Deform",[](Binding value, Key kp) {
        if (auto binding = std::dynamic_pointer_cast<rt::DeformationParameterBinding>(value)) {
            auto result = core::math::triangle::deformByDeformationBinding(binding.get(),binding.get(),kp,true);
            if (result) binding->setValue(kp,*result);
        }
    });
}
std::shared_ptr<CommandResult> SymmetrizeDeformCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return change(ctx,"Symmetrize Deform",[](Binding value, Key kp) {
        if (auto binding = std::dynamic_pointer_cast<rt::DeformationParameterBinding>(value)) {
            auto result = core::math::triangle::deformByDeformationBinding(binding.get(),binding.get(),kp,true);
            if (result && result->vertexOffsets.size() == deformationAt(binding,kp,281).vertexOffsets.size()) {
                for (std::size_t i=0;i<result->vertexOffsets.size();++i) {
                    auto destination=result->vertexOffsets[i];
                    const nicxlive::core::math::Vec2 a=deformationAt(binding,kp,283).vertexOffsets[i];
                    const nicxlive::core::math::Vec2 b=result->vertexOffsets[i];
                    destination=nicxlive::core::math::Vec2{(a.x+b.x)/2,(a.y+b.y)/2};
                }
                binding->setValue(kp,*result);
            }
        }
    });
}
std::shared_ptr<CommandResult> SetFromHorizontalMirrorCommand::run(std::shared_ptr<Context> c) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return mirror(c,"set From Mirror (Horizontally)",0,false);
}
std::shared_ptr<CommandResult> SetFromVerticalMirrorCommand::run(std::shared_ptr<Context> c) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return mirror(c,"set From Mirror (Vertically)",1,false);
}
std::shared_ptr<CommandResult> SetFromDiagonalMirrorCommand::run(std::shared_ptr<Context> c) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return mirror(c,"set From Mirror (Diagonally)",-1,false);
}
std::shared_ptr<CommandResult> SetFrom1DMirrorCommand::run(std::shared_ptr<Context> c) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return mirror(c,"set From Mirror",0,true);
}
std::shared_ptr<CommandResult> CopyBindingCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    Param param; Bindings bindings; Key kp; bool targetNull;
    const auto resolved=ngResolveBindingKeyContext(ctx,param,bindings,kp,targetNull); if(!resolved->succeeded)return resolved;
    cClipboardPoint={static_cast<std::uint32_t>(kp.x),static_cast<std::uint32_t>(kp.y)}; cClipboardBindings.clear();
    for(auto binding:bindings)cClipboardBindings[BindingTarget(compat::ngRequireObject(binding)->getTarget())]=binding;
    return CommandResult::opCall(true);
}
std::shared_ptr<PasteBindingCommand::Result> PasteBindingCommand::run(const std::shared_ptr<Context>& ctx) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return runTyped(ctx);
}
std::shared_ptr<PasteBindingCommand::Result> PasteBindingCommand::runTyped(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    Param param; Bindings bindings; Key kp; bool targetNull;
    const auto resolved=ngResolveBindingKeyContext(ctx,param,bindings,kp,targetNull);
    if(!resolved->succeeded)return Result::opCall(false,{},resolved->message);
    Bindings targets,sources,created;
    if(!targetNull) {
        for(auto binding:bindings) {
            const BindingTarget key(compat::ngRequireObject(binding)->getTarget()); const auto source=cClipboardBindings.find(key);
            if(source==cClipboardBindings.end())continue;
            // Keep the original AA value cell accessible after ensureTarget callbacks.
            const auto* sourceValue=&source->second;
            auto ensured=ensureTarget(param,key);
            if(ensured!=binding)created.append(ensured);
            targets.append(ensured);sources.append(*sourceValue);
        }
    } else {
        const auto clipboard=cClipboardBindings;
        for(const auto& entry:clipboard) {
            const auto key=entry.first; const auto source=entry.second;
            auto dest=findTarget(param,key);
            if(!dest) { dest=ensureTarget(param,key);created.append(dest); }
            targets.append(dest);sources.append(source);
        }
    }
    std::shared_ptr<Result> payload;
    if(!targets.empty()) {
        const bool single=targets.size()==1&&cClipboardBindings.size()==1;
        if(!single)stack::incActionPushGroup();
        if(!created.empty()) {
            auto add=pa::ngCreateParameterBindingsAction<pa::ParameterAddBindingsAction>(
                compat::RetainedString::fromStaticLiteral("paste"),param,created);
            add->updateNewState();stack::incActionPush(add);payload=Result::opCall(true,created);
        }
        for(std::size_t i=0;i<targets.size();++i) {
            const auto target=targets[i];
            const auto undo=pa::ngCreateParameterBindingsActionDeferred<pa::ParameterChangeBindingsValueAction>([&] {
                const auto name = compat::RetainedString::fromStaticLiteral("paste");
                const auto parameter = param;
                const auto targetBindings = Bindings{target};
                const auto pointX = static_cast<int>(kp.x);
                const auto pointY = static_cast<int>(kp.y);
                return std::tuple{name, parameter, targetBindings, pointX, pointY};
            });
            const auto source=compat::ngRequireObject(sourceAt(sources,i,498));
            source->copyKeypointToBinding({cClipboardPoint.x,cClipboardPoint.y},target,kp);
            undo->updateNewState();stack::incActionPush(undo);
        }
        if(!single)stack::incActionPopGroup();
    } else if(bindings.size()==1&&cClipboardBindings.size()==1) {
        const auto binding=sourceAt(bindings,0,505);
        auto clipboardValues=cClipboardBindings.values();
        const auto source=sourceAt(clipboardValues,0,506);
        const auto undo=action("paste",param,bindings,kp);
        if(auto deformation=std::dynamic_pointer_cast<rt::DeformationParameterBinding>(binding)) {
            const auto sourceDeform=std::dynamic_pointer_cast<rt::DeformationParameterBinding>(source);
            auto result=core::math::triangle::deformByDeformationBinding(deformation.get(),sourceDeform.get(),kp,false);
            if(result)deformation->setValue(kp,*result);
        } else {
            const auto value=std::dynamic_pointer_cast<rt::ValueParameterBinding>(binding);
            const auto sourceValue=std::dynamic_pointer_cast<rt::ValueParameterBinding>(source);
            const auto receiver=compat::ngRequireObject(value);
            const auto sample=compat::ngRequireObject(sourceValue)->getValue({cClipboardPoint.x,cClipboardPoint.y});
            receiver->setValue(kp,sample);
        }
        undo->updateNewState();stack::incActionPush(undo);
    }
    return payload?payload:Result::opCall(true,{},compat::RetainedString::emptyLiteral());
}
std::shared_ptr<RemoveBindingCommand::Result> RemoveBindingCommand::run(const std::shared_ptr<Context>& ctx) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    return runTyped(ctx);
}
std::shared_ptr<RemoveBindingCommand::Result> RemoveBindingCommand::runTyped(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    if(!compat::ngRequireObject(ctx)->hasParameters()||ctx->parameters().empty()||!ctx->hasActiveBindings()||ctx->activeBindings().empty())
        return Result::opCall(false,{},compat::RetainedString::fromStaticLiteral("No parameters/bindings"));
    auto parameters=ctx->parameters();
    const auto param=sourceAt(parameters,0,541);
    const auto group=std::allocate_shared<actions::GroupAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::GroupAction>{});Bindings removed;
    const auto bindings=ctx->activeBindings();
    for(auto binding:bindings) {
        const auto candidates=compat::ngRequireObject(param)->bindings;
        bool found=false;for(auto candidate:candidates)
            if(nicxlive::core::param::ngParameterBindingObjectEquals(candidate,binding)){found=true;break;}
        if(!found)continue;
        group->addAction(actions::binding::ngCreateBindingAddRemoveAction<actions::binding::ParameterBindingRemoveAction>(param,binding));
        param->removeBinding(binding);cSelectedBindings.erase(BindingTarget(compat::ngRequireObject(binding)->getTarget()));
        if(auto node=binding->getTarget().target.lock())node->notifyChange(node,nicxlive::core::nodes::NotifyReason::StructureChanged);
        removed.append(binding);
    }
    if(!removed.empty())stack::incActionPush(group);notify();
    return !removed.empty()?Result::opCall(true,removed,compat::RetainedString::fromStaticLiteral("Bindings removed")):
        Result::opCall(false,{},compat::RetainedString::fromStaticLiteral("No bindings removed"));
}
std::shared_ptr<CommandResult> SetInterpolationCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto commandOwner = this->weak_from_this().lock();
    if(!compat::ngRequireObject(ctx)->hasParameters()||ctx->parameters().empty()||!ctx->hasActiveBindings()||ctx->activeBindings().empty())
        return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("No parameters/bindings"));
    const auto group=std::allocate_shared<actions::GroupAction>(
        nicxlive::core::d_runtime::SourceAllocator<actions::GroupAction>{});const auto bindings=ctx->activeBindings();
    for(auto binding:bindings) {
        const auto currentMode=compat::ngRequireObject(binding)->interpolateMode();
        if(currentMode==mode)continue;
        group->addAction(actions::binding::ngCreateBindingInterpolationChangeAction(
            actions::binding::detail::BindingArgumentsProducer{[&] {
                const auto actionBinding = binding;
                const auto previousMode = compat::ngRequireObject(binding)->interpolateMode();
                const auto nextMode = mode;
                return std::tuple{actionBinding, previousMode, nextMode};
            }}));
        binding->setInterpolateMode(mode);
    }
    if(!group->actions.empty())stack::incActionPush(group);notify();return CommandResult::opCall(true);
}
void ngInitBindingCommands() {
    detail::Initialization::run();
    const auto checkDeclaration = [&]<class Declaration>(Declaration) {
        if(!commands.contains(Declaration::value)) {
            using Alias = typename detail::BindingRegistrationDeclaration<
                Declaration::sourceIdentifier>::SourceAlias;
            constexpr auto missingMessage = Alias::missingMessage.view();
            // The Source enforce owns its nonempty message before constructing
            // the real exception. Catch only this passive string backing copy.
            const auto message = [&] {
                try { return compat::RetainedString(missingMessage); }
                catch (const std::bad_alloc&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                } catch (const std::length_error&) {
                    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
                }
            }();
            throw compat::ThrowableException(message,
                compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/binding/binding.d"), 681);
        }
    };
    std::apply([&](auto... declaration) {
        (checkDeclaration(declaration), ...);
    }, detail::bindingCommandSourceDeclarations);
    ngInitBindingProviders();
}
} // namespace nijigenerate::commands::binding
