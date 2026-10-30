#pragma once
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include "welding.hpp"
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>

#include <nijigenerate/commands/command.hpp>
#include <limits>
#include <concepts>
#include <memory>
#include <utility>
#include <compat/object_metadata.hpp>

namespace nicxlive::core::d_runtime {
#define NG_WELDING_SOURCE_NAMES(Type) \
template<> struct SourceTypeSpellings<::nijigenerate::commands::node::welding::Type> { \
    inline static constexpr bool isNativeObject = true; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.node.welding." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.node.welding." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
}; \
template<> struct SourceObjectTypeNames<::nijigenerate::commands::node::welding::Type> { \
    inline static constexpr char className[] = "nijigenerate.commands.node.welding." #Type; \
    inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.node.welding." #Type ")"; \
};
NG_WELDING_SOURCE_NAMES(AddWeldingCommand)
NG_WELDING_SOURCE_NAMES(RemoveWeldingCommand)
NG_WELDING_SOURCE_NAMES(ChangeWeldingWeightCommand)
#undef NG_WELDING_SOURCE_NAMES
}

namespace nicxlive::core::nodes { class Drawable; }

namespace nijigenerate::commands::node::welding {
namespace detail {
struct WeldingCommandText { compat::RetainedString label; compat::RetainedString description; };
}

struct AddWeldingArguments {
    std::shared_ptr<nicxlive::core::nodes::Drawable> target;
    float weight = std::numeric_limits<float>::quiet_NaN();
};
struct RemoveWeldingArguments {
    std::shared_ptr<nicxlive::core::nodes::Drawable> target;
};
struct ChangeWeldingWeightArguments {
    std::shared_ptr<nicxlive::core::nodes::Drawable> target;
    float weight = std::numeric_limits<float>::quiet_NaN();
};

} // namespace nijigenerate::commands::node::welding

namespace nijigenerate::commands {

template<> struct CommandFields<node::welding::AddWeldingArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&node::welding::AddWeldingArguments::target, "target", "Target drawable", "Drawable">{},
        FieldDescriptor<&node::welding::AddWeldingArguments::weight, "weight", "Welding weight", "float">{}};
};
template<> struct CommandFields<node::welding::RemoveWeldingArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&node::welding::RemoveWeldingArguments::target, "target", "Target drawable", "Drawable">{}};
};
template<> struct CommandFields<node::welding::ChangeWeldingWeightArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&node::welding::ChangeWeldingWeightArguments::target, "target",
            "Target drawable", "Drawable">{},
        FieldDescriptor<&node::welding::ChangeWeldingWeightArguments::weight, "weight",
            "New welding weight", "float">{}};
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::node::welding {

class AddWeldingCommand : public ExCommand<AddWeldingArguments>,
    public std::enable_shared_from_this<AddWeldingCommand> {
    static detail::WeldingCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<AddWeldingCommand>;
    AddWeldingCommand(detail::WeldingCommandText text,
        std::shared_ptr<nicxlive::core::nodes::Drawable> target, float weight);
protected:
    void prepareSourceClassInfo() override;
public:
    AddWeldingCommand(std::shared_ptr<nicxlive::core::nodes::Drawable> target, float weight);
    AddWeldingCommand(const AddWeldingCommand&) = delete;
    AddWeldingCommand& operator=(const AddWeldingCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class RemoveWeldingCommand : public ExCommand<RemoveWeldingArguments>,
    public std::enable_shared_from_this<RemoveWeldingCommand> {
    static detail::WeldingCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<RemoveWeldingCommand>;
    RemoveWeldingCommand(detail::WeldingCommandText text,
        std::shared_ptr<nicxlive::core::nodes::Drawable> target);
protected:
    void prepareSourceClassInfo() override;
public:
    explicit RemoveWeldingCommand(std::shared_ptr<nicxlive::core::nodes::Drawable> target);
    RemoveWeldingCommand(const RemoveWeldingCommand&) = delete;
    RemoveWeldingCommand& operator=(const RemoveWeldingCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class ChangeWeldingWeightCommand : public ExCommand<ChangeWeldingWeightArguments>,
    public std::enable_shared_from_this<ChangeWeldingWeightCommand> {
    static detail::WeldingCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ChangeWeldingWeightCommand>;
    ChangeWeldingWeightCommand(detail::WeldingCommandText text,
        std::shared_ptr<nicxlive::core::nodes::Drawable> target, float weight);
protected:
    void prepareSourceClassInfo() override;
public:
    ChangeWeldingWeightCommand(std::shared_ptr<nicxlive::core::nodes::Drawable> target, float weight);
    ChangeWeldingWeightCommand(const ChangeWeldingWeightCommand&) = delete;
    ChangeWeldingWeightCommand& operator=(const ChangeWeldingWeightCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

} // namespace nijigenerate::commands::node::welding

namespace nijigenerate::commands {

template<> struct CommandTraits<node::welding::AddWeldingCommand> {
    static constexpr auto attributes = std::tuple{EffectCreate};
    using BaseClasses = CommandTypeList<ExCommand<node::welding::AddWeldingArguments>, Command>;
};
template<> struct CommandTraits<node::welding::RemoveWeldingCommand> {
    static constexpr auto attributes = std::tuple{EffectDelete};
    using BaseClasses = CommandTypeList<ExCommand<node::welding::RemoveWeldingArguments>, Command>;
};
template<> struct CommandTraits<node::welding::ChangeWeldingWeightCommand> {
    static constexpr auto attributes = std::tuple{EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<node::welding::ChangeWeldingWeightArguments>, Command>;
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands {

template<> struct SourceCommandConstructor<node::welding::AddWeldingCommand> {
    template<class... Args> struct Program {
        using C = node::welding::AddWeldingCommand;
        using Target = std::shared_ptr<nicxlive::core::nodes::Drawable>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 2) return false;
            else return []<class T, class W>(CommandTypeList<T, W>) {
                return detail::declaredSourceParametersSupported<std::tuple<Target, float>, T, W>() &&
                    requires {
                        { C::sourceConstructorText() } -> std::same_as<node::welding::detail::WeldingCommandText>;
                    } && SourceExCommandConstructorProgram<node::welding::AddWeldingArguments>::
                        preferredSupported<Target&, float&>();
            }(CommandTypeList<Args...>{});
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return [&]<class T, class W>(T&& target, W&& weight) {
                return detail::constructDeclaredSourceCommand<C, std::tuple<Target, float>>(allocator,
                    std::forward<T>(target), std::forward<W>(weight));
            }(std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::welding::ChangeWeldingWeightCommand> {
    template<class... Args> struct Program {
        using C = node::welding::ChangeWeldingWeightCommand;
        using Target = std::shared_ptr<nicxlive::core::nodes::Drawable>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 2) return false;
            else return []<class T, class W>(CommandTypeList<T, W>) {
                return detail::declaredSourceParametersSupported<std::tuple<Target, float>, T, W>() &&
                    requires {
                        { C::sourceConstructorText() } -> std::same_as<node::welding::detail::WeldingCommandText>;
                    } && SourceExCommandConstructorProgram<node::welding::ChangeWeldingWeightArguments>::
                        preferredSupported<Target&, float&>();
            }(CommandTypeList<Args...>{});
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return [&]<class T, class W>(T&& target, W&& weight) {
                return detail::constructDeclaredSourceCommand<C, std::tuple<Target, float>>(allocator,
                    std::forward<T>(target), std::forward<W>(weight));
            }(std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<node::welding::RemoveWeldingCommand> {
    template<class... Args> struct Program {
        using C = node::welding::RemoveWeldingCommand;
        using Target = std::shared_ptr<nicxlive::core::nodes::Drawable>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 1) return false;
            else return detail::declaredSourceParametersSupported<std::tuple<Target>, Args...>() &&
                requires {
                    { C::sourceConstructorText() } -> std::same_as<node::welding::detail::WeldingCommandText>;
                } && SourceExCommandConstructorProgram<node::welding::RemoveWeldingArguments>::
                    preferredSupported<Target&>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return detail::constructDeclaredSourceCommand<C, std::tuple<Target>>(allocator,
                std::forward<Args>(args)...);
        }
    };
};
} // namespace nijigenerate::commands

namespace nijigenerate::commands::node::welding {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct Weighted {
    static auto get() { return std::tuple{std::shared_ptr<nicxlive::core::nodes::Drawable>{}, 0.5f}; }
};
struct Target { static auto get() { return std::tuple{std::shared_ptr<nicxlive::core::nodes::Drawable>{}}; } };
template<auto Id, class C>
using Optional = ::nijigenerate::commands::detail::OptionalCommandRegistration<Id, C>;
template<auto Id, class C, class Args>
using OptionalArgs = ::nijigenerate::commands::detail::OptionalConstructedCommandRegistration<Id, C, Args>;
template<auto Id, class C, class Args>
using Required = ::nijigenerate::commands::detail::ConstructedCommandRegistration<Id, C, Args>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Optional<NodeWeldingCommand::AddWelding, AddWeldingCommand>,
    Optional<NodeWeldingCommand::RemoveWelding, RemoveWeldingCommand>,
    Optional<NodeWeldingCommand::ChangeWeldingWeight, ChangeWeldingWeightCommand>,
    OptionalArgs<NodeWeldingCommand::AddWelding, AddWeldingCommand, Weighted>,
    OptionalArgs<NodeWeldingCommand::RemoveWelding, RemoveWeldingCommand, Target>,
    OptionalArgs<NodeWeldingCommand::ChangeWeldingWeight, ChangeWeldingWeightCommand, Weighted>,
    Required<NodeWeldingCommand::AddWelding, AddWeldingCommand, Weighted>,
    Required<NodeWeldingCommand::RemoveWelding, RemoveWeldingCommand, Target>,
    Required<NodeWeldingCommand::ChangeWeldingWeight, ChangeWeldingWeightCommand, Weighted>>;
}
template<class T> requires (std::same_as<T, NodeWeldingCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitWeldingCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<node::welding::NodeWeldingCommand> {
    static consteval bool supported() {
        return node::welding::initialization::Program::supported<node::welding::initialization::Table>();
    }
    template<class ActualKey = node::welding::NodeWeldingCommand>
        requires (std::same_as<ActualKey, node::welding::NodeWeldingCommand> && supported())
    static void invoke() { node::welding::ngInitCommands<ActualKey>(); }
};
}
