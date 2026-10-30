#pragma once

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <concepts>
#include <memory>

namespace nijigenerate::commands::viewport::control {
namespace detail { struct ControlCommandText; }

class ToggleMirrorViewCommand : public ExCommand<>,
    public std::enable_shared_from_this<ToggleMirrorViewCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    ToggleMirrorViewCommand();
    ToggleMirrorViewCommand(const ToggleMirrorViewCommand&) = delete;
    ToggleMirrorViewCommand& operator=(const ToggleMirrorViewCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ToggleMirrorViewCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit ToggleMirrorViewCommand(compat::RetainedString description);
};
class ToggleOnionSliceCommand : public ExCommand<>,
    public std::enable_shared_from_this<ToggleOnionSliceCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    ToggleOnionSliceCommand();
    ToggleOnionSliceCommand(const ToggleOnionSliceCommand&) = delete;
    ToggleOnionSliceCommand& operator=(const ToggleOnionSliceCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ToggleOnionSliceCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit ToggleOnionSliceCommand(compat::RetainedString description);
};
class TogglePhysicsCommand : public ExCommand<>,
    public std::enable_shared_from_this<TogglePhysicsCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    TogglePhysicsCommand();
    TogglePhysicsCommand(const TogglePhysicsCommand&) = delete;
    TogglePhysicsCommand& operator=(const TogglePhysicsCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<TogglePhysicsCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit TogglePhysicsCommand(compat::RetainedString description);
};
class TogglePostProcessCommand : public ExCommand<>,
    public std::enable_shared_from_this<TogglePostProcessCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    TogglePostProcessCommand();
    TogglePostProcessCommand(const TogglePostProcessCommand&) = delete;
    TogglePostProcessCommand& operator=(const TogglePostProcessCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<TogglePostProcessCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit TogglePostProcessCommand(compat::RetainedString description);
};
class ResetPhysicsCommand : public ExCommand<>,
    public std::enable_shared_from_this<ResetPhysicsCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    ResetPhysicsCommand();
    ResetPhysicsCommand(const ResetPhysicsCommand&) = delete;
    ResetPhysicsCommand& operator=(const ResetPhysicsCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ResetPhysicsCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit ResetPhysicsCommand(compat::RetainedString description);
};
class ResetParametersCommand : public ExCommand<>,
    public std::enable_shared_from_this<ResetParametersCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    ResetParametersCommand();
    ResetParametersCommand(const ResetParametersCommand&) = delete;
    ResetParametersCommand& operator=(const ResetParametersCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ResetParametersCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit ResetParametersCommand(compat::RetainedString description);
};
class OpenFlipPairWindowCommand : public ExCommand<>,
    public std::enable_shared_from_this<OpenFlipPairWindowCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    OpenFlipPairWindowCommand();
    OpenFlipPairWindowCommand(const OpenFlipPairWindowCommand&) = delete;
    OpenFlipPairWindowCommand& operator=(const OpenFlipPairWindowCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<OpenFlipPairWindowCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit OpenFlipPairWindowCommand(compat::RetainedString description);
};
class OpenAutomeshBatchingCommand : public ExCommand<>,
    public std::enable_shared_from_this<OpenAutomeshBatchingCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    OpenAutomeshBatchingCommand();
    OpenAutomeshBatchingCommand(const OpenAutomeshBatchingCommand&) = delete;
    OpenAutomeshBatchingCommand& operator=(const OpenAutomeshBatchingCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<OpenAutomeshBatchingCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit OpenAutomeshBatchingCommand(compat::RetainedString description);
};
class ResetViewportZoomCommand : public ExCommand<>,
    public std::enable_shared_from_this<ResetViewportZoomCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    ResetViewportZoomCommand();
    ResetViewportZoomCommand(const ResetViewportZoomCommand&) = delete;
    ResetViewportZoomCommand& operator=(const ResetViewportZoomCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ResetViewportZoomCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit ResetViewportZoomCommand(compat::RetainedString description);
};
class ResetViewportPositionCommand : public ExCommand<>,
    public std::enable_shared_from_this<ResetViewportPositionCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    ResetViewportPositionCommand();
    ResetViewportPositionCommand(const ResetViewportPositionCommand&) = delete;
    ResetViewportPositionCommand& operator=(const ResetViewportPositionCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ResetViewportPositionCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit ResetViewportPositionCommand(compat::RetainedString description);
};
class FitViewportToModelCommand : public ExCommand<>,
    public std::enable_shared_from_this<FitViewportToModelCommand> {
    friend struct ::nijigenerate::commands::SourceCommandConstructor<FitViewportToModelCommand>;
    static detail::ControlCommandText sourceConstructorArguments();
    explicit FitViewportToModelCommand(detail::ControlCommandText text);
protected:
    void prepareSourceClassInfo() override;
public:
    FitViewportToModelCommand();
    FitViewportToModelCommand(const FitViewportToModelCommand&) = delete;
    FitViewportToModelCommand& operator=(const FitViewportToModelCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

} // namespace nijigenerate::commands::viewport::control

namespace nicxlive::core::d_runtime {
#define NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::commands::viewport::control::Type> { \
        inline static constexpr char className[] = "nijigenerate.commands.viewport.control." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.viewport.control." #Type ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::viewport::control::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.viewport.control." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.viewport.control." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(ToggleMirrorViewCommand)
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(ToggleOnionSliceCommand)
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(TogglePhysicsCommand)
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(TogglePostProcessCommand)
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(ResetPhysicsCommand)
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(ResetParametersCommand)
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(OpenFlipPairWindowCommand)
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(OpenAutomeshBatchingCommand)
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(ResetViewportZoomCommand)
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(ResetViewportPositionCommand)
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(FitViewportToModelCommand)
#undef NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands {

template<> struct CommandTraits<viewport::control::ToggleMirrorViewCommand> {
    static constexpr auto attributes = std::tuple{PresentationOnly{}};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<viewport::control::ToggleOnionSliceCommand> {
    static constexpr auto attributes = std::tuple{PresentationOnly{}};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<viewport::control::TogglePhysicsCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<viewport::control::TogglePostProcessCommand> {
    static constexpr auto attributes = std::tuple{PresentationOnly{}};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<viewport::control::ResetPhysicsCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<viewport::control::ResetParametersCommand> {
    static constexpr auto attributes = std::tuple{EffectKeyframeEdit};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<viewport::control::OpenFlipPairWindowCommand> {
    static constexpr auto attributes = std::tuple{McpHidden, GuiWindow};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<viewport::control::OpenAutomeshBatchingCommand> {
    static constexpr auto attributes = std::tuple{McpHidden, GuiModal};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<viewport::control::ResetViewportZoomCommand> {
    static constexpr auto attributes = std::tuple{PresentationOnly{}};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<viewport::control::ResetViewportPositionCommand> {
    static constexpr auto attributes = std::tuple{PresentationOnly{}};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<viewport::control::FitViewportToModelCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectLayoutReset, PresentationOnly{}};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands {

// Each record belongs to one actual Source declaration. Its description hook
// also executes in that class's production constructor, before the same base
// initialization program and the existing exact-class metadata publication.
#define NG_CONTROL_DESCRIPTION_CONSTRUCTOR(Type, Storage, Query) \
    template<> struct SourceCommandConstructor<viewport::control::Type> { \
        template<class... Args> struct Program { \
            using C = viewport::control::Type; \
            static consteval bool supported() { \
                return sizeof...(Args) == 0 && requires { \
                    { C::sourceConstructorDescription() } -> std::same_as<compat::RetainedString>; \
                } && SourceExCommandConstructorProgram<Storage>::Query(); \
            } \
            static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, \
                Args&&...) requires(supported()) { \
                return std::allocate_shared<C>(allocator); \
            } \
        }; \
    };

NG_CONTROL_DESCRIPTION_CONSTRUCTOR(ToggleMirrorViewCommand, CommandNoArguments, descriptionSupported)
NG_CONTROL_DESCRIPTION_CONSTRUCTOR(ToggleOnionSliceCommand, CommandNoArguments, descriptionSupported)
NG_CONTROL_DESCRIPTION_CONSTRUCTOR(TogglePhysicsCommand, CommandNoArguments, descriptionSupported)
NG_CONTROL_DESCRIPTION_CONSTRUCTOR(TogglePostProcessCommand, CommandNoArguments, descriptionSupported)
NG_CONTROL_DESCRIPTION_CONSTRUCTOR(ResetPhysicsCommand, CommandNoArguments, descriptionSupported)
NG_CONTROL_DESCRIPTION_CONSTRUCTOR(ResetParametersCommand, CommandNoArguments, descriptionSupported)
NG_CONTROL_DESCRIPTION_CONSTRUCTOR(OpenFlipPairWindowCommand, CommandNoArguments, descriptionSupported)
NG_CONTROL_DESCRIPTION_CONSTRUCTOR(OpenAutomeshBatchingCommand, CommandNoArguments, descriptionSupported)
NG_CONTROL_DESCRIPTION_CONSTRUCTOR(ResetViewportZoomCommand, CommandNoArguments, descriptionSupported)
NG_CONTROL_DESCRIPTION_CONSTRUCTOR(ResetViewportPositionCommand, CommandNoArguments, descriptionSupported)

#undef NG_CONTROL_DESCRIPTION_CONSTRUCTOR

template<> struct SourceCommandConstructor<viewport::control::FitViewportToModelCommand> {
    template<class... Args> struct Program {
        using C = viewport::control::FitViewportToModelCommand;
        static consteval bool supported() {
            // The existing private two-string carrier is defined in control.cpp;
            // its function type can be checked without inventing a Source type.
            return sizeof...(Args) == 0 && std::same_as<decltype(&C::sourceConstructorArguments),
                viewport::control::detail::ControlCommandText (*)()> &&
                SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&...) requires(supported()) {
            return std::allocate_shared<C>(allocator);
        }
    };
};

} // namespace nijigenerate::commands
