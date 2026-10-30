// Independent source/nijigenerate/commands/parameter/param.d complete bodies.
#include "param_commands.hpp"
#include "param_table.hpp"
#include "base.hpp"
#include <nijigenerate/actions/parameter.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/ext/param.hpp>
#include <param/binding_impl.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <compat/object_metadata.hpp>
#include <compat/source_allocator.hpp>
#include <array>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace nijigenerate::commands::parameter::param {
namespace {
using ParameterRef = std::shared_ptr<nicxlive::core::param::Parameter>;
// Materialize the Source array argument inside allocator.construct, after the
// actual Result object storage has been allocated.
struct ResultParameterSliceArgument {
    const ParameterRef& value;
    operator compat::Slice<ParameterRef>() const { return {value}; }
};
struct ResultParameterDuplicateArgument {
    const std::shared_ptr<Context>& context;
    operator compat::Slice<ParameterRef>() const { return context->parameters().duplicate(); }
};
template<std::size_t N> compat::RetainedString translated(const char (&value)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(value));
}
template<class... Args> compat::RetainedString format(compat::RetainedString pattern, const Args&... args) {
    const std::array<core::translation::TypedFormatArgument, sizeof...(Args)> values{
        core::translation::ngFormatArgument(args)...};
    auto formatted = core::translation::ngFormatTyped(pattern.view(), values);
    try { return compat::RetainedString(std::move(formatted)); }
    catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<std::size_t N>
nicxlive::core::common::NullableString parameterName(const char (&pattern)[N], std::size_t count) {
    return format(compat::RetainedString::fromStaticLiteral(pattern), count).toNullableString();
}
}
#define NG_PARAM_CLASS_INFO(Type) nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(Type), \
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.param." #Type))
Add1DParameterCommand::SourceConstructorArguments
Add1DParameterCommand::sourceConstructorArguments(std::int32_t lo, std::int32_t hi) {
    auto description = format(translated("Add 1D Parameter (%d..%d)"), lo, hi);
    return {std::move(description), lo, hi};
}
Add1DParameterCommand::Add1DParameterCommand(std::int32_t lo, std::int32_t hi)
    : Add1DParameterCommand(sourceConstructorArguments(lo, hi)) {}
Add1DParameterCommand::Add1DParameterCommand(SourceConstructorArguments arguments)
    : NativeBase({}, arguments.description, arguments.min, arguments.max) {
    NG_PARAM_CLASS_INFO(Add1DParameterCommand);
}
Add2DParameterCommand::SourceConstructorArguments
Add2DParameterCommand::sourceConstructorArguments(std::int32_t lo, std::int32_t hi) {
    auto description = format(translated("Add 2D Parameter (%d..%d)"), lo, hi);
    return {std::move(description), lo, hi};
}
Add2DParameterCommand::Add2DParameterCommand(std::int32_t lo, std::int32_t hi)
    : Add2DParameterCommand(sourceConstructorArguments(lo, hi)) {}
Add2DParameterCommand::Add2DParameterCommand(SourceConstructorArguments arguments)
    : NativeBase({}, arguments.description, arguments.min, arguments.max) {
    NG_PARAM_CLASS_INFO(Add2DParameterCommand);
}
AddMouthParameterCommand::SourceConstructorArguments AddMouthParameterCommand::sourceConstructorArguments() {
    return {translated("Add Mouth Parameter")};
}
AddMouthParameterCommand::AddMouthParameterCommand()
    : AddMouthParameterCommand(sourceConstructorArguments()) {}
AddMouthParameterCommand::AddMouthParameterCommand(SourceConstructorArguments arguments)
    : NativeBase({}, arguments.description) { NG_PARAM_CLASS_INFO(AddMouthParameterCommand); }
RemoveParameterCommand::SourceConstructorArguments RemoveParameterCommand::sourceConstructorArguments() {
    return {translated("Remove selected parameters from the active puppet")};
}
RemoveParameterCommand::RemoveParameterCommand()
    : RemoveParameterCommand(sourceConstructorArguments()) {}
RemoveParameterCommand::RemoveParameterCommand(SourceConstructorArguments arguments)
    : NativeBase({}, arguments.description) {
    NG_PARAM_CLASS_INFO(RemoveParameterCommand);
}
#undef NG_PARAM_CLASS_INFO
void Add1DParameterCommand::prepareSourceClassInfo() {
    NativeBase::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<Add1DParameterCommand>();
}
void Add2DParameterCommand::prepareSourceClassInfo() {
    NativeBase::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<Add2DParameterCommand>();
}
void AddMouthParameterCommand::prepareSourceClassInfo() {
    NativeBase::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AddMouthParameterCommand>();
}
void RemoveParameterCommand::prepareSourceClassInfo() {
    NativeBase::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<RemoveParameterCommand>();
}
std::shared_ptr<Add1DParameterCommand::Result> Add1DParameterCommand::run(
        const std::shared_ptr<Context>& context) { return runTyped(context); }
std::shared_ptr<Add2DParameterCommand::Result> Add2DParameterCommand::run(
        const std::shared_ptr<Context>& context) { return runTyped(context); }
std::shared_ptr<AddMouthParameterCommand::Result> AddMouthParameterCommand::run(
        const std::shared_ptr<Context>& context) { return runTyped(context); }
std::shared_ptr<RemoveParameterCommand::Result> RemoveParameterCommand::run(
        const std::shared_ptr<Context>& context) { return runTyped(context); }
std::shared_ptr<Add1DParameterCommand::Result> Add1DParameterCommand::runTyped(std::shared_ptr<Context> context) {
    const auto commandOwner = weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasPuppet()) return Result::opCall(false, {}, "No puppet");
    ParameterRef parameter = ext::ExParameter::createSourceDeferred(
        [&] { return parameterName("Param #%d\0", context->parameters().size()); }, [] { return false; });
    parameter->min.x = static_cast<float>(min); parameter->max.x = static_cast<float>(max);
    if (static_cast<std::uint32_t>(min) + static_cast<std::uint32_t>(max) == 0)
        parameter->insertAxisPoint(0, 0.5f);
    compat::ngRequireObject(context->puppet())->parameters.push_back(parameter);
    core::actionstack::incActionPush(actions::parameter::ParameterAddAction::createSourceDeferred(
        [&] { return parameter; }, [&] { return context->puppet(); }));
    return std::allocate_shared<Result>(nicxlive::core::d_runtime::SourceAllocator<Result>{},
        true, ResultParameterSliceArgument{parameter}, compat::RetainedString::fromStaticLiteral("Parameter created"));
}
std::shared_ptr<Add2DParameterCommand::Result> Add2DParameterCommand::runTyped(std::shared_ptr<Context> context) {
    const auto commandOwner = weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasPuppet()) return Result::opCall(false, {}, "No puppet");
    ParameterRef parameter = ext::ExParameter::createSourceDeferred(
        [&] { return parameterName("Param #%d\0", context->parameters().size()); }, [] { return true; });
    parameter->min = {static_cast<float>(min), static_cast<float>(min)};
    parameter->max = {static_cast<float>(max), static_cast<float>(max)};
    if (static_cast<std::uint32_t>(min) + static_cast<std::uint32_t>(max) == 0) {
        parameter->insertAxisPoint(0, 0.5f); parameter->insertAxisPoint(1, 0.5f);
    }
    compat::ngRequireObject(context->puppet())->parameters.push_back(parameter);
    core::actionstack::incActionPush(actions::parameter::ParameterAddAction::createSourceDeferred(
        [&] { return parameter; }, [&] { return context->puppet(); }));
    return std::allocate_shared<Result>(nicxlive::core::d_runtime::SourceAllocator<Result>{},
        true, ResultParameterSliceArgument{parameter}, compat::RetainedString::fromStaticLiteral("Parameter created"));
}
std::shared_ptr<AddMouthParameterCommand::Result> AddMouthParameterCommand::runTyped(std::shared_ptr<Context> context) {
    const auto commandOwner = weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasPuppet()) return Result::opCall(false, {}, "No puppet");
    ParameterRef parameter = ext::ExParameter::createSourceDeferred(
        [&] { return parameterName("Mouth #%d\0", context->parameters().size()); }, [] { return true; });
    parameter->min = {-1, 0}; parameter->max = {1, 1};
    parameter->insertAxisPoint(0, 0.25f); parameter->insertAxisPoint(0, 0.5f);
    parameter->insertAxisPoint(0, 0.6f); parameter->insertAxisPoint(1, 0.3f);
    parameter->insertAxisPoint(1, 0.5f); parameter->insertAxisPoint(1, 0.6f);
    compat::ngRequireObject(context->puppet())->parameters.push_back(parameter);
    core::actionstack::incActionPush(actions::parameter::ParameterAddAction::createSourceDeferred(
        [&] { return parameter; }, [&] { return context->puppet(); }));
    return std::allocate_shared<Result>(nicxlive::core::d_runtime::SourceAllocator<Result>{},
        true, ResultParameterSliceArgument{parameter}, compat::RetainedString::fromStaticLiteral("Parameter created"));
}
std::shared_ptr<RemoveParameterCommand::Result> RemoveParameterCommand::runTyped(std::shared_ptr<Context> context) {
    const auto commandOwner = weak_from_this().lock();
    if (!compat::ngRequireObject(context)->hasParameters()) return Result::opCall(false, {}, "No parameters");
    const auto parameters = context->parameters();
    for (const auto parameter : parameters) base::removeParameter(parameter);
    return std::allocate_shared<Result>(nicxlive::core::d_runtime::SourceAllocator<Result>{},
        true, ResultParameterDuplicateArgument{context},
        compat::RetainedString::fromStaticLiteral("Parameters removed"));
}
void ngInitParameterCommands() {
    initialization::Program::run<initialization::Table>();
}
} // namespace nijigenerate::commands::parameter::param
