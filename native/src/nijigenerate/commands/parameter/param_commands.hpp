#pragma once
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/commands/covariant_command.hpp>
#include <cstdint>
#include <concepts>
#include <memory>
#include <utility>
#include "param_table.hpp"
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::parameter::param {
class Add1DParameterCommand;
class Add2DParameterCommand;
class AddMouthParameterCommand;
class RemoveParameterCommand;
}
namespace nicxlive::core::d_runtime {
#define NG_PARAMETER_SOURCE_NAMES(Type) \
template<> struct SourceTypeSpellings<::nijigenerate::commands::parameter::param::Type> { \
    inline static constexpr bool isNativeObject = true; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.parameter.param." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.parameter.param." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
}; \
template<> struct SourceObjectTypeNames<::nijigenerate::commands::parameter::param::Type> { \
    inline static constexpr char className[] = "nijigenerate.commands.parameter.param." #Type; \
    inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.parameter.param." #Type ")"; \
};
NG_PARAMETER_SOURCE_NAMES(Add1DParameterCommand)
NG_PARAMETER_SOURCE_NAMES(Add2DParameterCommand)
NG_PARAMETER_SOURCE_NAMES(AddMouthParameterCommand)
NG_PARAMETER_SOURCE_NAMES(RemoveParameterCommand)
#undef NG_PARAMETER_SOURCE_NAMES
}

namespace nicxlive::core::param { class Parameter; }

namespace nijigenerate::commands::parameter::param {

struct ParameterRangeArguments {
    std::int32_t min = 0;
    std::int32_t max = 0;
};

} // namespace nijigenerate::commands::parameter::param

namespace nijigenerate::commands {

template<> struct CommandFields<parameter::param::ParameterRangeArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&parameter::param::ParameterRangeArguments::min, "min",
            "minimum value of the Parameter", "int">{},
        FieldDescriptor<&parameter::param::ParameterRangeArguments::max, "max",
            "maximum value of the Parameter", "int">{}};
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::parameter::param {

class Add1DParameterCommand : public ::nijigenerate::commands::detail::CovariantCommand<
        Add1DParameterCommand, ParameterRangeArguments> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<Add1DParameterCommand, ParameterRangeArguments>;
    struct SourceConstructorArguments { compat::RetainedString description; std::int32_t min; std::int32_t max; };
    static SourceConstructorArguments sourceConstructorArguments(std::int32_t min, std::int32_t max);
    explicit Add1DParameterCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<Add1DParameterCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = CreateResult<std::shared_ptr<nicxlive::core::param::Parameter>>;
    Add1DParameterCommand(std::int32_t min, std::int32_t max);
    Add1DParameterCommand(const Add1DParameterCommand&) = delete;
    Add1DParameterCommand& operator=(const Add1DParameterCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};

class Add2DParameterCommand : public ::nijigenerate::commands::detail::CovariantCommand<
        Add2DParameterCommand, ParameterRangeArguments> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<Add2DParameterCommand, ParameterRangeArguments>;
    struct SourceConstructorArguments { compat::RetainedString description; std::int32_t min; std::int32_t max; };
    static SourceConstructorArguments sourceConstructorArguments(std::int32_t min, std::int32_t max);
    explicit Add2DParameterCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<Add2DParameterCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = CreateResult<std::shared_ptr<nicxlive::core::param::Parameter>>;
    Add2DParameterCommand(std::int32_t min, std::int32_t max);
    Add2DParameterCommand(const Add2DParameterCommand&) = delete;
    Add2DParameterCommand& operator=(const Add2DParameterCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};

class AddMouthParameterCommand : public ::nijigenerate::commands::detail::CovariantCommand<AddMouthParameterCommand> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<AddMouthParameterCommand>;
    struct SourceConstructorArguments { compat::RetainedString description; };
    static SourceConstructorArguments sourceConstructorArguments();
    explicit AddMouthParameterCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<AddMouthParameterCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = CreateResult<std::shared_ptr<nicxlive::core::param::Parameter>>;
    AddMouthParameterCommand();
    AddMouthParameterCommand(const AddMouthParameterCommand&) = delete;
    AddMouthParameterCommand& operator=(const AddMouthParameterCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};

class RemoveParameterCommand : public ::nijigenerate::commands::detail::CovariantCommand<RemoveParameterCommand> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<RemoveParameterCommand>;
    struct SourceConstructorArguments { compat::RetainedString description; };
    static SourceConstructorArguments sourceConstructorArguments();
    explicit RemoveParameterCommand(SourceConstructorArguments arguments);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<RemoveParameterCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = DeleteResult<std::shared_ptr<nicxlive::core::param::Parameter>>;
    RemoveParameterCommand();
    RemoveParameterCommand(const RemoveParameterCommand&) = delete;
    RemoveParameterCommand& operator=(const RemoveParameterCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};

void ngInitParameterCommands();

} // namespace nijigenerate::commands::parameter::param

namespace nijigenerate::commands {

template<> struct CommandTraits<parameter::param::Add1DParameterCommand> {
    static constexpr auto attributes = std::tuple{EffectCreate};
    using BaseClasses = CommandTypeList<ExCommand<parameter::param::ParameterRangeArguments>, Command>;
};
template<> struct CommandTraits<parameter::param::Add2DParameterCommand> {
    static constexpr auto attributes = std::tuple{EffectCreate};
    using BaseClasses = CommandTypeList<ExCommand<parameter::param::ParameterRangeArguments>, Command>;
};
template<> struct CommandTraits<parameter::param::AddMouthParameterCommand> {
    static constexpr auto attributes = std::tuple{EffectCreate};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<parameter::param::RemoveParameterCommand> {
    static constexpr auto attributes = std::tuple{EffectDelete};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands {

template<> struct SourceCommandConstructor<parameter::param::Add1DParameterCommand> {
    template<class... Args> struct Program {
        using C = parameter::param::Add1DParameterCommand;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 2) return false;
            else return detail::declaredSourceParametersSupported<std::tuple<std::int32_t, std::int32_t>, Args...>() &&
                requires(std::int32_t& lo, std::int32_t& hi) {
                    { C::sourceConstructorArguments(lo, hi) } -> std::same_as<typename C::SourceConstructorArguments>;
                } && SourceExCommandConstructorProgram<parameter::param::ParameterRangeArguments>::
                    preferredSupported<std::int32_t&, std::int32_t&>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return detail::constructDeclaredSourceCommand<C, std::tuple<std::int32_t, std::int32_t>>(allocator,
                std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<parameter::param::Add2DParameterCommand> {
    template<class... Args> struct Program {
        using C = parameter::param::Add2DParameterCommand;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 2) return false;
            else return detail::declaredSourceParametersSupported<std::tuple<std::int32_t, std::int32_t>, Args...>() &&
                requires(std::int32_t& lo, std::int32_t& hi) {
                    { C::sourceConstructorArguments(lo, hi) } -> std::same_as<typename C::SourceConstructorArguments>;
                } && SourceExCommandConstructorProgram<parameter::param::ParameterRangeArguments>::
                    preferredSupported<std::int32_t&, std::int32_t&>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return detail::constructDeclaredSourceCommand<C, std::tuple<std::int32_t, std::int32_t>>(allocator,
                std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<parameter::param::AddMouthParameterCommand> {
    template<class... Args> struct Program {
        using C = parameter::param::AddMouthParameterCommand;
        static consteval bool supported() {
            return sizeof...(Args) == 0 && requires {
                { C::sourceConstructorArguments() } -> std::same_as<typename C::SourceConstructorArguments>;
            } && SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<parameter::param::RemoveParameterCommand> {
    template<class... Args> struct Program {
        using C = parameter::param::RemoveParameterCommand;
        static consteval bool supported() {
            return sizeof...(Args) == 0 && requires {
                { C::sourceConstructorArguments() } -> std::same_as<typename C::SourceConstructorArguments>;
            } && SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};
} // namespace nijigenerate::commands

namespace nijigenerate::commands::parameter::param {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct Limits { static auto get() { return std::tuple{-1, 1}; } };
template<auto Id, class C>
using Optional = ::nijigenerate::commands::detail::OptionalCommandRegistration<Id, C>;
template<auto Id, class C, class Args>
using Required = ::nijigenerate::commands::detail::ConstructedCommandRegistration<Id, C, Args>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Optional<ParamCommand::Add1DParameter, Add1DParameterCommand>,
    Optional<ParamCommand::Add2DParameter, Add2DParameterCommand>,
    Optional<ParamCommand::AddMouthParameter, AddMouthParameterCommand>,
    Optional<ParamCommand::RemoveParameter, RemoveParameterCommand>,
    Required<ParamCommand::Add1DParameter, Add1DParameterCommand, Limits>,
    Required<ParamCommand::Add2DParameter, Add2DParameterCommand, Limits>>;
}
template<class T> requires (std::same_as<T, ParamCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitParameterCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<parameter::param::ParamCommand> {
    static consteval bool supported() {
        return parameter::param::initialization::Program::supported<parameter::param::initialization::Table>();
    }
    template<class ActualKey = parameter::param::ParamCommand>
        requires (std::same_as<ActualKey, parameter::param::ParamCommand> && supported())
    static void invoke() { parameter::param::ngInitCommands<ActualKey>(); }
};
}
