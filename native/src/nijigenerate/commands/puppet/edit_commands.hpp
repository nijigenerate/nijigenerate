#pragma once

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include "edit_table.hpp"
#include <compat/object_metadata.hpp>

namespace nijigenerate::commands::puppet::edit {
class UndoCommand;
class RedoCommand;
class ShowSettingsWindowCommand;
class ShowCommandBrowserWindowCommand;
}
namespace nicxlive::core::d_runtime {
#define NG_PUPPET_EDIT_SOURCE_NAMES(Type) \
template<> struct SourceTypeSpellings<::nijigenerate::commands::puppet::edit::Type> { \
    inline static constexpr bool isNativeObject = true; \
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.puppet.edit." #Type ")"; \
    inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.puppet.edit." #Type; \
    inline static constexpr char unqualifiedElement[] = #Type; \
}; \
template<> struct SourceObjectTypeNames<::nijigenerate::commands::puppet::edit::Type> { \
    inline static constexpr char className[] = "nijigenerate.commands.puppet.edit." #Type; \
    inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.puppet.edit." #Type ")"; \
};
NG_PUPPET_EDIT_SOURCE_NAMES(UndoCommand)
NG_PUPPET_EDIT_SOURCE_NAMES(RedoCommand)
NG_PUPPET_EDIT_SOURCE_NAMES(ShowSettingsWindowCommand)
NG_PUPPET_EDIT_SOURCE_NAMES(ShowCommandBrowserWindowCommand)
#undef NG_PUPPET_EDIT_SOURCE_NAMES
}

namespace nijigenerate::commands::puppet::edit {

namespace detail {
struct EditCommandText {
    compat::RetainedString label;
    compat::RetainedString description;
};
}

void ngInitEditCommands();
template<EditCommand id>
std::shared_ptr<CommandResult> ngRunEditCommand(std::shared_ptr<Context> context) {
    const auto found = commands.find(id);
    if (found == commands.end()) throw std::runtime_error("No registered edit command");
    if (!ngCommandAllowedInCurrentContext(found->second))
        return CommandResult::opCall(false, "Command is not available in the current command scope");
    return ngRunCommand(found->second, context);
}

class UndoCommand : public ExCommand<>, public std::enable_shared_from_this<UndoCommand> {
    static detail::EditCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<UndoCommand>;
    explicit UndoCommand(detail::EditCommandText text);
protected:
    void prepareSourceClassInfo() override;
public:
    UndoCommand();
    UndoCommand(const UndoCommand&) = delete;
    UndoCommand& operator=(const UndoCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
    bool runnable(std::shared_ptr<Context> context) override;
};

class RedoCommand : public ExCommand<>, public std::enable_shared_from_this<RedoCommand> {
    static detail::EditCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<RedoCommand>;
    explicit RedoCommand(detail::EditCommandText text);
protected:
    void prepareSourceClassInfo() override;
public:
    RedoCommand();
    RedoCommand(const RedoCommand&) = delete;
    RedoCommand& operator=(const RedoCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
    bool runnable(std::shared_ptr<Context> context) override;
};

class ShowSettingsWindowCommand : public ExCommand<>, public std::enable_shared_from_this<ShowSettingsWindowCommand> {
    static detail::EditCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowSettingsWindowCommand>;
    explicit ShowSettingsWindowCommand(detail::EditCommandText text);
protected:
    void prepareSourceClassInfo() override;
public:
    ShowSettingsWindowCommand();
    ShowSettingsWindowCommand(const ShowSettingsWindowCommand&) = delete;
    ShowSettingsWindowCommand& operator=(const ShowSettingsWindowCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

class ShowCommandBrowserWindowCommand : public ExCommand<>,
    public std::enable_shared_from_this<ShowCommandBrowserWindowCommand> {
    static detail::EditCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowCommandBrowserWindowCommand>;
    explicit ShowCommandBrowserWindowCommand(detail::EditCommandText text);
protected:
    void prepareSourceClassInfo() override;
public:
    ShowCommandBrowserWindowCommand();
    ShowCommandBrowserWindowCommand(const ShowCommandBrowserWindowCommand&) = delete;
    ShowCommandBrowserWindowCommand& operator=(const ShowCommandBrowserWindowCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

} // namespace nijigenerate::commands::puppet::edit

namespace nijigenerate::commands {

template<> struct CommandTraits<puppet::edit::UndoCommand> {
    static constexpr auto attributes = std::tuple{CommandScopes<GlobalCommandScope>{}};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<puppet::edit::RedoCommand> {
    static constexpr auto attributes = std::tuple{CommandScopes<GlobalCommandScope>{}};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<puppet::edit::ShowSettingsWindowCommand> {
    static constexpr auto attributes = std::tuple{McpHidden, GuiWindow};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<puppet::edit::ShowCommandBrowserWindowCommand> {
    static constexpr auto attributes = std::tuple{McpHidden, GuiWindow};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};

#define NG_EDIT_CONSTRUCTOR(Type) \
template<> struct SourceCommandConstructor<puppet::edit::Type> { \
    template<class... Args> struct Program { \
        using C = puppet::edit::Type; \
        static consteval bool supported() { \
            return sizeof...(Args) == 0 && requires { \
                { C::sourceConstructorText() } -> std::same_as<puppet::edit::detail::EditCommandText>; \
            } && SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported(); \
        } \
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args) \
            requires(supported()) { \
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...); \
        } \
    }; \
};
NG_EDIT_CONSTRUCTOR(UndoCommand)
NG_EDIT_CONSTRUCTOR(RedoCommand)
NG_EDIT_CONSTRUCTOR(ShowSettingsWindowCommand)
NG_EDIT_CONSTRUCTOR(ShowCommandBrowserWindowCommand)
#undef NG_EDIT_CONSTRUCTOR

} // namespace nijigenerate::commands

namespace nijigenerate::commands::puppet::edit {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
template<auto Id, class C>
using Step = ::nijigenerate::commands::detail::OptionalCommandRegistration<Id, C>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Step<EditCommand::Undo, UndoCommand>, Step<EditCommand::Redo, RedoCommand>,
    Step<EditCommand::ShowSettingsWindow, ShowSettingsWindowCommand>,
    Step<EditCommand::ShowCommandBrowserWindow, ShowCommandBrowserWindowCommand>>;
}
template<class T> requires (std::same_as<T, EditCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitEditCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<puppet::edit::EditCommand> {
    static consteval bool supported() {
        return puppet::edit::initialization::Program::supported<puppet::edit::initialization::Table>();
    }
    template<class ActualKey = puppet::edit::EditCommand>
        requires (std::same_as<ActualKey, puppet::edit::EditCommand> && supported())
    static void invoke() { puppet::edit::ngInitCommands<ActualKey>(); }
};
}
