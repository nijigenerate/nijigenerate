#pragma once

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/commands/covariant_command.hpp>
#include <array>
#include <cstdint>
#include <limits>
#include "group_table.hpp"
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::parameter::group {
class MoveParameterCommand;
class CreateParamGroupCommand;
class ChangeGroupColorCommand;
class DeleteParamGroupCommand;
}
namespace nicxlive::core::d_runtime {
#define NG_PARAMETER_GROUP_SOURCE_NAMES(Type) \
template<> struct SourceTypeSpellings<::nijigenerate::commands::parameter::group::Type> { \
    inline static constexpr bool isNativeObject = true; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.parameter.group." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.parameter.group." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
}; \
template<> struct SourceObjectTypeNames<::nijigenerate::commands::parameter::group::Type> { \
    inline static constexpr char className[] = "nijigenerate.commands.parameter.group." #Type; \
    inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.parameter.group." #Type ")"; \
};
NG_PARAMETER_GROUP_SOURCE_NAMES(MoveParameterCommand)
NG_PARAMETER_GROUP_SOURCE_NAMES(CreateParamGroupCommand)
NG_PARAMETER_GROUP_SOURCE_NAMES(ChangeGroupColorCommand)
NG_PARAMETER_GROUP_SOURCE_NAMES(DeleteParamGroupCommand)
#undef NG_PARAMETER_GROUP_SOURCE_NAMES
}

namespace nijigenerate::ext { class ExParameterGroup; }

namespace nijigenerate::commands::parameter::group {

struct MoveParameterArguments {
    std::shared_ptr<ext::ExParameterGroup> group;
    std::int32_t index = 0;
};

struct CreateParamGroupArguments {
    std::int32_t index = 0;
};

struct ChangeGroupColorArguments {
    std::array<float, 3> color{
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::quiet_NaN()};
};

} // namespace nijigenerate::commands::parameter::group

namespace nijigenerate::commands {

template<> struct CommandFields<parameter::group::MoveParameterArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&parameter::group::MoveParameterArguments::group, "group", "", "ExParameterGroup">{},
        FieldDescriptor<&parameter::group::MoveParameterArguments::index, "index", "", "int">{}};
};

template<> struct CommandFields<parameter::group::CreateParamGroupArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&parameter::group::CreateParamGroupArguments::index, "index", "", "int">{}};
};

template<> struct CommandFields<parameter::group::ChangeGroupColorArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&parameter::group::ChangeGroupColorArguments::color, "color",
            "color value for target Parameter Group.", "float[3]">{}};
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::parameter::group {

class MoveParameterCommand : public ExCommand<MoveParameterArguments>,
    public std::enable_shared_from_this<MoveParameterCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString description;
        std::shared_ptr<ext::ExParameterGroup> group;
        std::int32_t index;
        static SourceConstructorFrame capture(std::shared_ptr<ext::ExParameterGroup>& group, std::int32_t& index);
    };
    explicit MoveParameterCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<MoveParameterCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    MoveParameterCommand(std::shared_ptr<ext::ExParameterGroup> group, std::int32_t index);
    MoveParameterCommand(const MoveParameterCommand&) = delete;
    MoveParameterCommand& operator=(const MoveParameterCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class CreateParamGroupCommand : public ::nijigenerate::commands::detail::CovariantCommand<
        CreateParamGroupCommand, CreateParamGroupArguments> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<
        CreateParamGroupCommand, CreateParamGroupArguments>;
    struct SourceConstructorFrame {
        compat::RetainedString description;
        std::int32_t index;
        static SourceConstructorFrame capture(std::int32_t& index);
    };
    explicit CreateParamGroupCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<CreateParamGroupCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = CreateResult<std::shared_ptr<ext::ExParameterGroup>>;
    explicit CreateParamGroupCommand(std::int32_t index = 0);
    CreateParamGroupCommand(const CreateParamGroupCommand&) = delete;
    CreateParamGroupCommand& operator=(const CreateParamGroupCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};

class ChangeGroupColorCommand : public ExCommand<ChangeGroupColorArguments>,
    public std::enable_shared_from_this<ChangeGroupColorCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString description;
        std::array<float, 3> color;
        static SourceConstructorFrame capture(std::array<float, 3>& color);
    };
    explicit ChangeGroupColorCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ChangeGroupColorCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    explicit ChangeGroupColorCommand(std::array<float, 3> color = {0.0f, 0.0f, 0.0f});
    ChangeGroupColorCommand(const ChangeGroupColorCommand&) = delete;
    ChangeGroupColorCommand& operator=(const ChangeGroupColorCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class DeleteParamGroupCommand : public ::nijigenerate::commands::detail::CovariantCommand<DeleteParamGroupCommand> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<DeleteParamGroupCommand>;
    struct SourceConstructorFrame {
        compat::RetainedString description;
        static SourceConstructorFrame capture();
    };
    explicit DeleteParamGroupCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<DeleteParamGroupCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = DeleteResult<std::shared_ptr<ext::ExParameterGroup>>;
    DeleteParamGroupCommand();
    DeleteParamGroupCommand(const DeleteParamGroupCommand&) = delete;
    DeleteParamGroupCommand& operator=(const DeleteParamGroupCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};

void ngInitParameterGroupCommands();

} // namespace nijigenerate::commands::parameter::group

namespace nijigenerate::commands {

template<> struct CommandTraits<parameter::group::MoveParameterCommand> {
    static constexpr auto attributes = std::tuple{EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<parameter::group::MoveParameterArguments>, Command>;
};
template<> struct CommandTraits<parameter::group::CreateParamGroupCommand> {
    static constexpr auto attributes = std::tuple{EffectCreate};
    using BaseClasses = CommandTypeList<ExCommand<parameter::group::CreateParamGroupArguments>, Command>;
};
template<> struct CommandTraits<parameter::group::ChangeGroupColorCommand> {
    static constexpr auto attributes = std::tuple{EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<parameter::group::ChangeGroupColorArguments>, Command>;
};
template<> struct CommandTraits<parameter::group::DeleteParamGroupCommand> {
    static constexpr auto attributes = std::tuple{EffectDelete};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};

template<> struct SourceCommandConstructor<parameter::group::MoveParameterCommand> {
    template<class... Args> struct Program {
        using C = parameter::group::MoveParameterCommand;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 2) return false;
            else return []<class Group, class Index>(CommandTypeList<Group, Index>) {
                return ngCommandInitializationSupported<std::shared_ptr<ext::ExParameterGroup>, Group&&>() &&
                    ngCommandInitializationSupported<std::int32_t, Index&&>() &&
                    requires(std::shared_ptr<ext::ExParameterGroup>& group, std::int32_t& index) {
                        { C::SourceConstructorFrame::capture(group, index) } ->
                            std::same_as<typename C::SourceConstructorFrame>;
                    } &&
                    SourceExCommandConstructorProgram<parameter::group::MoveParameterArguments>::
                        preferredSupported<std::shared_ptr<ext::ExParameterGroup>, std::int32_t>();
            }(CommandTypeList<Args...>{}) && detail::declaredSourceParametersSupported<
                std::tuple<std::shared_ptr<ext::ExParameterGroup>, std::int32_t>, Args...>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            return detail::constructDeclaredSourceCommand<C,
                std::tuple<std::shared_ptr<ext::ExParameterGroup>, std::int32_t>>(
                    allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<parameter::group::CreateParamGroupCommand> {
    template<class... Args> struct Program {
        using C = parameter::group::CreateParamGroupCommand;
        static consteval bool supported() {
            using Base = SourceExCommandConstructorProgram<parameter::group::CreateParamGroupArguments>;
            constexpr bool body = requires(std::int32_t& index) {
                { C::SourceConstructorFrame::capture(index) } -> std::same_as<typename C::SourceConstructorFrame>;
            } && Base::preferredSupported<std::int32_t>();
            if constexpr (sizeof...(Args) == 0) return body;
            else if constexpr (sizeof...(Args) == 1)
                return (ngCommandInitializationSupported<std::int32_t, Args&&>() && ...) &&
                    body && detail::declaredSourceParametersSupported<std::tuple<std::int32_t>, Args...>();
            else return false;
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            if constexpr (sizeof...(Args) == 0) return std::allocate_shared<C>(allocator);
            else {
                return detail::constructDeclaredSourceCommand<C, std::tuple<std::int32_t>>(
                    allocator, std::forward<Args>(args)...);
            }
        }
    };
};

template<> struct SourceCommandConstructor<parameter::group::ChangeGroupColorCommand> {
    template<class... Args> struct Program {
        using C = parameter::group::ChangeGroupColorCommand;
        static consteval bool supported() {
            using Color = std::array<float, 3>;
            using Base = SourceExCommandConstructorProgram<parameter::group::ChangeGroupColorArguments>;
            constexpr bool body = requires(Color& color) {
                { C::SourceConstructorFrame::capture(color) } -> std::same_as<typename C::SourceConstructorFrame>;
            } && Base::preferredSupported<Color>();
            if constexpr (sizeof...(Args) == 0) return body;
            else if constexpr (sizeof...(Args) == 1)
                return (ngCommandInitializationSupported<Color, Args&&>() && ...) && body &&
                    detail::declaredSourceParametersSupported<std::tuple<Color>, Args...>();
            else return false;
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            if constexpr (sizeof...(Args) == 0) return std::allocate_shared<C>(allocator);
            else {
                return detail::constructDeclaredSourceCommand<C, std::tuple<std::array<float, 3>>>(
                    allocator, std::forward<Args>(args)...);
            }
        }
    };
};

template<> struct SourceCommandConstructor<parameter::group::DeleteParamGroupCommand> {
    template<class... Args> struct Program {
        using C = parameter::group::DeleteParamGroupCommand;
        static consteval bool supported() {
            return sizeof...(Args) == 0 && requires {
                { C::SourceConstructorFrame::capture() } -> std::same_as<typename C::SourceConstructorFrame>;
            } && SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::parameter::group {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct MoveArguments { static auto get() { return std::tuple{nullptr, std::int32_t{0}}; } };
struct CreateArguments { static auto get() { return std::tuple{std::int32_t{0}}; } };
struct ColorArguments { static auto get() { return std::tuple{std::array<float, 3>{0, 0, 0}}; } };
template<auto Id, class C>
using Optional = ::nijigenerate::commands::detail::OptionalCommandRegistration<Id, C>;
template<auto Id, class C, class Args>
using Required = ::nijigenerate::commands::detail::ConstructedCommandRegistration<Id, C, Args>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Optional<GroupCommand::MoveParameter, MoveParameterCommand>,
    Optional<GroupCommand::CreateParamGroup, CreateParamGroupCommand>,
    Optional<GroupCommand::ChangeGroupColor, ChangeGroupColorCommand>,
    Optional<GroupCommand::DeleteParamGroup, DeleteParamGroupCommand>,
    Required<GroupCommand::MoveParameter, MoveParameterCommand, MoveArguments>,
    Required<GroupCommand::CreateParamGroup, CreateParamGroupCommand, CreateArguments>,
    Required<GroupCommand::ChangeGroupColor, ChangeGroupColorCommand, ColorArguments>>;
}
template<class T> requires (std::same_as<T, GroupCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitParameterGroupCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<parameter::group::GroupCommand> {
    static consteval bool supported() {
        return parameter::group::initialization::Program::supported<parameter::group::initialization::Table>();
    }
    template<class ActualKey = parameter::group::GroupCommand>
        requires (std::same_as<ActualKey, parameter::group::GroupCommand> && supported())
    static void invoke() { parameter::group::ngInitCommands<ActualKey>(); }
};
}
