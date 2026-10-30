#pragma once
#include "command_scope.hpp"
#include "command.hpp"
#include "register_command.hpp"
#include "detail/initializer_body.hpp"
#include <nijigenerate/core/settings_value.hpp>
#include <concepts>
#include <memory>
#include <utility>
namespace nijigenerate::commands {
class GetCurrentCommandScopeCommand : public ExCommand<>,
    public std::enable_shared_from_this<GetCurrentCommandScopeCommand> {
    struct SourceConstructorArguments {
        compat::RetainedString label;
        compat::RetainedString description;
    };
    static SourceConstructorArguments sourceConstructorArguments();
    explicit GetCurrentCommandScopeCommand(SourceConstructorArguments arguments);
    friend struct SourceCommandConstructor<GetCurrentCommandScopeCommand>;
public:
    using Result = ExCommandResult<core::settings::SettingsJsonValue>;
    GetCurrentCommandScopeCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
    std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
protected:
    void prepareSourceClassInfo() override;
};
template<> struct SourceCommandConstructor<GetCurrentCommandScopeCommand> {
    template<class... Args> struct Program {
        using C = GetCurrentCommandScopeCommand;
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

template<> struct CommandTraits<GetCurrentCommandScopeCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, CommandScopes<GlobalCommandScope>{}};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};

namespace detail {
struct ScopeInitializationBinding {
    using map_type = decltype(scopeCommands);
    static map_type& get() { return scopeCommands; }
};
using ScopeInitialization = CommandRegistrationSequence<DefaultCommandRegistration<
    CommandScopeCommand::GetCurrentCommandScope, GetCurrentCommandScopeCommand>>;
}

template<class T> requires (std::same_as<T, CommandScopeCommand> &&
    detail::ScopeInitialization::supported<detail::ScopeInitializationBinding>())
void ngInitCommands() { ngInitCommandScopeCommands(); }

namespace detail {
template<> struct InitializerBody<CommandScopeCommand> {
    static consteval bool supported() { return ScopeInitialization::supported<ScopeInitializationBinding>(); }
    template<class ActualKey = CommandScopeCommand>
        requires (std::same_as<ActualKey, CommandScopeCommand> && supported())
    static void invoke() { ::nijigenerate::commands::ngInitCommands<ActualKey>(); }
};
}
}

namespace nicxlive::core::d_runtime {
template<> struct SourceObjectTypeNames<::nijigenerate::commands::GetCurrentCommandScopeCommand> {
    inline static constexpr char className[] =
        "nijigenerate.commands.command_scope.GetCurrentCommandScopeCommand";
    inline static constexpr char templateArgumentName[] =
        "(nijigenerate.commands.command_scope.GetCurrentCommandScopeCommand)";
};
template<> struct SourceTypeSpellings<::nijigenerate::commands::GetCurrentCommandScopeCommand> {
    inline static constexpr bool isNativeObject = true;
    inline static constexpr char qualifiedArgument[] =
        "(nijigenerate.commands.command_scope.GetCurrentCommandScopeCommand)";
    inline static constexpr char unqualifiedArgument[] = "(GetCurrentCommandScopeCommand)";
    inline static constexpr char qualifiedElement[] =
        "nijigenerate.commands.command_scope.GetCurrentCommandScopeCommand";
    inline static constexpr char unqualifiedElement[] = "GetCurrentCommandScopeCommand";
};
}
