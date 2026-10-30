#pragma once

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/covariant_command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include "view.hpp"

namespace nijigenerate::commands::puppet::view {
struct SaveScreenshotArguments { compat::RetainedStringSlice filename; };
struct CaptureLiveScreenshotArguments { core::settings::SettingsJsonValue overlayObjects; };
}
namespace nijigenerate::commands {
template<> struct CommandFields<puppet::view::SaveScreenshotArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&puppet::view::SaveScreenshotArguments::filename,
            "filename", "file path to save screenshot.", "string">{}};
};
template<> struct CommandFields<puppet::view::CaptureLiveScreenshotArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&puppet::view::CaptureLiveScreenshotArguments::overlayObjects,
            "overlayObjects", "Optional overlay objects. Use [{\"uuid\": 123, \"overlay\": \"bounds\"|\"mesh\"}] or [{\"123\": \"bounds\"|\"mesh\"}].",
            "JSONValue", true, false, CommandJsonSchema::overlayObjects>{}};
};
}

namespace nijigenerate::commands::puppet::view {
class SetDefaultLayoutCommand : public ExCommand<>, public std::enable_shared_from_this<SetDefaultLayoutCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString label, description;
        static SourceConstructorFrame capture();
    };
    explicit SetDefaultLayoutCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetDefaultLayoutCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    SetDefaultLayoutCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class ShowSaveScreenshotDialogCommand : public ExCommand<>,
    public std::enable_shared_from_this<ShowSaveScreenshotDialogCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString label, description;
        static SourceConstructorFrame capture();
    };
    explicit ShowSaveScreenshotDialogCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowSaveScreenshotDialogCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    ShowSaveScreenshotDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class SaveScreenshotCommand : public ExCommand<SaveScreenshotArguments>,
    public std::enable_shared_from_this<SaveScreenshotCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString label, description;
        compat::RetainedStringSlice filename;
        static SourceConstructorFrame capture(compat::RetainedStringSlice& filename);
    };
    explicit SaveScreenshotCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SaveScreenshotCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    explicit SaveScreenshotCommand(compat::RetainedStringSlice filename);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class CaptureLiveScreenshotCommand : public ::nijigenerate::commands::detail::CovariantCommand<
        CaptureLiveScreenshotCommand, CaptureLiveScreenshotArguments> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<
        CaptureLiveScreenshotCommand, CaptureLiveScreenshotArguments>;
    struct SourceConstructorFrame {
        compat::RetainedString label, description;
        static SourceConstructorFrame capture();
    };
    explicit CaptureLiveScreenshotCommand(SourceConstructorFrame frame);
    static consteval bool sourceConstructorBodySupported() {
        // This exact declared JSON carrier supplies empty-array construction
        // and descriptor-preserving JSON assignment in settings_value.hpp.
        // It is not an inferred assignment contract for arbitrary structures.
        return requires(core::settings::SettingsJsonValue& value, core::settings::SettingsJsonArray array) {
            { core::settings::SettingsJsonValue(std::move(array)) } ->
                std::same_as<core::settings::SettingsJsonValue>;
            { value = core::settings::SettingsJsonValue(std::move(array)) } ->
                std::same_as<core::settings::SettingsJsonValue&>;
        };
    }
    void sourceConstructorBody();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<CaptureLiveScreenshotCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = ExCommandResult<core::settings::SettingsJsonValue>;
    CaptureLiveScreenshotCommand();
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};
class ShowStatusForNerdsCommand : public ExCommand<>, public std::enable_shared_from_this<ShowStatusForNerdsCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString label, description;
        static SourceConstructorFrame capture();
    };
    explicit ShowStatusForNerdsCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowStatusForNerdsCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    ShowStatusForNerdsCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class ToggleDifferenceAggregationCommand : public ExCommand<>,
    public std::enable_shared_from_this<ToggleDifferenceAggregationCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString label, description;
        static SourceConstructorFrame capture();
    };
    explicit ToggleDifferenceAggregationCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ToggleDifferenceAggregationCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    ToggleDifferenceAggregationCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
void ngInitViewCommands();
}

namespace nicxlive::core::d_runtime {
#define NG_VIEW_COMMAND_SOURCE_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::commands::puppet::view::Type> { \
        inline static constexpr char className[] = "nijigenerate.commands.puppet.view." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.puppet.view." #Type ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::puppet::view::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.puppet.view." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.puppet.view." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_VIEW_COMMAND_SOURCE_NAMES(SetDefaultLayoutCommand)
NG_VIEW_COMMAND_SOURCE_NAMES(ShowSaveScreenshotDialogCommand)
NG_VIEW_COMMAND_SOURCE_NAMES(SaveScreenshotCommand)
NG_VIEW_COMMAND_SOURCE_NAMES(CaptureLiveScreenshotCommand)
NG_VIEW_COMMAND_SOURCE_NAMES(ShowStatusForNerdsCommand)
NG_VIEW_COMMAND_SOURCE_NAMES(ToggleDifferenceAggregationCommand)
#undef NG_VIEW_COMMAND_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands {
template<> struct CommandTraits<puppet::view::SetDefaultLayoutCommand> {
    static constexpr auto attributes = std::tuple{McpHidden, GuiLayout, EffectLayoutReset, PresentationOnly{}};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<puppet::view::ShowSaveScreenshotDialogCommand> {
    static constexpr auto attributes = std::tuple{McpHidden, GuiDialog};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<puppet::view::SaveScreenshotCommand> {
    static constexpr auto attributes = std::tuple{EffectFileWrite};
    using BaseClasses = CommandTypeList<ExCommand<puppet::view::SaveScreenshotArguments>, Command>;
};
template<> struct CommandTraits<puppet::view::CaptureLiveScreenshotCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, PresentationOnly{}};
    using BaseClasses = CommandTypeList<ExCommand<puppet::view::CaptureLiveScreenshotArguments>, Command>;
};
template<> struct CommandTraits<puppet::view::ShowStatusForNerdsCommand> {
    static constexpr auto attributes = std::tuple{PresentationOnly{}};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<puppet::view::ToggleDifferenceAggregationCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};

// Each record names the actual defining, executable constructor frame. These
// classes have one authored no-argument overload and no argument-field writes.
#define NG_VIEW_PAIR_CONSTRUCTOR(Type) \
template<> struct SourceCommandConstructor<puppet::view::Type> { \
    template<class... Args> struct Program { \
        using C = puppet::view::Type; \
        static consteval bool supported() { \
            return sizeof...(Args) == 0 && requires { \
                { C::SourceConstructorFrame::capture() } -> std::same_as<typename C::SourceConstructorFrame>; \
            } && SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported(); \
        } \
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args) \
            requires(supported()) { \
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...); \
        } \
    }; \
};
NG_VIEW_PAIR_CONSTRUCTOR(SetDefaultLayoutCommand)
NG_VIEW_PAIR_CONSTRUCTOR(ShowSaveScreenshotDialogCommand)
NG_VIEW_PAIR_CONSTRUCTOR(ShowStatusForNerdsCommand)
NG_VIEW_PAIR_CONSTRUCTOR(ToggleDifferenceAggregationCommand)
#undef NG_VIEW_PAIR_CONSTRUCTOR

template<> struct SourceCommandConstructor<puppet::view::SaveScreenshotCommand> {
    template<class... Args> struct Program {
        using C = puppet::view::SaveScreenshotCommand;
        using String = compat::RetainedStringSlice;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 1) return false;
            else return (ngCommandInitializationSupported<String, Args&&>() && ...) && requires(String& filename) {
                { C::SourceConstructorFrame::capture(filename) } -> std::same_as<typename C::SourceConstructorFrame>;
            } && SourceExCommandConstructorProgram<puppet::view::SaveScreenshotArguments>::preferredSupported<String>() &&
                detail::declaredSourceParametersSupported<std::tuple<String>, Args...>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            return detail::constructDeclaredSourceCommand<C, std::tuple<String>>(
                allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<puppet::view::CaptureLiveScreenshotCommand> {
    template<class... Args> struct Program {
        using C = puppet::view::CaptureLiveScreenshotCommand;
        static consteval bool supported() {
            return sizeof...(Args) == 0 && requires(C& command) {
                { C::SourceConstructorFrame::capture() } -> std::same_as<typename C::SourceConstructorFrame>;
                command.sourceConstructorBody();
            } && SourceExCommandConstructorProgram<puppet::view::CaptureLiveScreenshotArguments>::pairSupported() &&
                C::sourceConstructorBodySupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...);
        }
    };
};
}

namespace nijigenerate::commands::puppet::view {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct ScreenshotArguments { static auto get() { return std::tuple{compat::RetainedString::emptyLiteral()}; } };
template<auto Id, class C>
using Optional = ::nijigenerate::commands::detail::OptionalCommandRegistration<Id, C>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Optional<ViewCommand::SetDefaultLayout, SetDefaultLayoutCommand>,
    Optional<ViewCommand::ShowSaveScreenshotDialog, ShowSaveScreenshotDialogCommand>,
    Optional<ViewCommand::SaveScreenshot, SaveScreenshotCommand>,
    Optional<ViewCommand::CaptureLiveScreenshot, CaptureLiveScreenshotCommand>,
    Optional<ViewCommand::ShowStatusForNerds, ShowStatusForNerdsCommand>,
    Optional<ViewCommand::ToggleDifferenceAggregation, ToggleDifferenceAggregationCommand>,
    ::nijigenerate::commands::detail::ConstructedCommandRegistration<
        ViewCommand::SaveScreenshot, SaveScreenshotCommand, ScreenshotArguments>>;
}
template<class T> requires (std::same_as<T, ViewCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitViewCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<puppet::view::ViewCommand> {
    static consteval bool supported() {
        return puppet::view::initialization::Program::supported<puppet::view::initialization::Table>();
    }
    template<class ActualKey = puppet::view::ViewCommand>
        requires (std::same_as<ActualKey, puppet::view::ViewCommand> && supported())
    static void invoke() { puppet::view::ngInitCommands<ActualKey>(); }
};
}
