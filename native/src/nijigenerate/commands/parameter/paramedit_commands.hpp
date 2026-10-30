#pragma once

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/covariant_command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <cstdint>
#include "paramedit_table.hpp"

namespace nicxlive::core::param { class Parameter; }

namespace nijigenerate::commands::parameter::paramedit {
namespace detail {
struct KeypointCommandText { compat::RetainedString label; compat::RetainedString description; };
}
void ngInitParameterEditCommands();
void addBinding(std::shared_ptr<nicxlive::core::param::Parameter> parameter,
    std::shared_ptr<nicxlive::core::param::Parameter> target, std::int32_t fromAxis, std::int32_t toAxis);
}

namespace nijigenerate::commands::parameter::paramedit {

#define NG_PARAMEDIT_DESCRIPTION_DECL(Type) \
private: \
    static compat::RetainedString sourceConstructorDescription(); \
    explicit Type(compat::RetainedString description); \
    friend struct ::nijigenerate::commands::SourceCommandConstructor<Type>; \
public:

class ConvertTo2DParamCommand : public ExCommand<>,
        public std::enable_shared_from_this<ConvertTo2DParamCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(ConvertTo2DParamCommand)
    ConvertTo2DParamCommand();
    ConvertTo2DParamCommand(const ConvertTo2DParamCommand&) = delete;
    ConvertTo2DParamCommand& operator=(const ConvertTo2DParamCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class FlipXCommand : public ExCommand<>,
        public std::enable_shared_from_this<FlipXCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(FlipXCommand)
    FlipXCommand();
    FlipXCommand(const FlipXCommand&) = delete;
    FlipXCommand& operator=(const FlipXCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class FlipYCommand : public ExCommand<>,
        public std::enable_shared_from_this<FlipYCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(FlipYCommand)
    FlipYCommand();
    FlipYCommand(const FlipYCommand&) = delete;
    FlipYCommand& operator=(const FlipYCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class Flip1DCommand : public ExCommand<>,
        public std::enable_shared_from_this<Flip1DCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(Flip1DCommand)
    Flip1DCommand();
    Flip1DCommand(const Flip1DCommand&) = delete;
    Flip1DCommand& operator=(const Flip1DCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class MirrorHorizontallyCommand : public ExCommand<>,
        public std::enable_shared_from_this<MirrorHorizontallyCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(MirrorHorizontallyCommand)
    MirrorHorizontallyCommand();
    MirrorHorizontallyCommand(const MirrorHorizontallyCommand&) = delete;
    MirrorHorizontallyCommand& operator=(const MirrorHorizontallyCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class MirrorVerticallyCommand : public ExCommand<>,
        public std::enable_shared_from_this<MirrorVerticallyCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(MirrorVerticallyCommand)
    MirrorVerticallyCommand();
    MirrorVerticallyCommand(const MirrorVerticallyCommand&) = delete;
    MirrorVerticallyCommand& operator=(const MirrorVerticallyCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class MirroredAutoFillDir1Command : public ExCommand<>,
        public std::enable_shared_from_this<MirroredAutoFillDir1Command> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(MirroredAutoFillDir1Command)
    MirroredAutoFillDir1Command();
    MirroredAutoFillDir1Command(const MirroredAutoFillDir1Command&) = delete;
    MirroredAutoFillDir1Command& operator=(const MirroredAutoFillDir1Command&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class MirroredAutoFillDir2Command : public ExCommand<>,
        public std::enable_shared_from_this<MirroredAutoFillDir2Command> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(MirroredAutoFillDir2Command)
    MirroredAutoFillDir2Command();
    MirroredAutoFillDir2Command(const MirroredAutoFillDir2Command&) = delete;
    MirroredAutoFillDir2Command& operator=(const MirroredAutoFillDir2Command&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class MirroredAutoFillDir3Command : public ExCommand<>,
        public std::enable_shared_from_this<MirroredAutoFillDir3Command> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(MirroredAutoFillDir3Command)
    MirroredAutoFillDir3Command();
    MirroredAutoFillDir3Command(const MirroredAutoFillDir3Command&) = delete;
    MirroredAutoFillDir3Command& operator=(const MirroredAutoFillDir3Command&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class MirroredAutoFillDir4Command : public ExCommand<>,
        public std::enable_shared_from_this<MirroredAutoFillDir4Command> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(MirroredAutoFillDir4Command)
    MirroredAutoFillDir4Command();
    MirroredAutoFillDir4Command(const MirroredAutoFillDir4Command&) = delete;
    MirroredAutoFillDir4Command& operator=(const MirroredAutoFillDir4Command&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

} // namespace nijigenerate::commands::parameter::paramedit


namespace nijigenerate::commands {
namespace detail {
struct ParameterStructuralEditCommandTraits {
    static constexpr auto attributes = std::tuple{EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
} // namespace detail

template<> struct CommandTraits<parameter::paramedit::ConvertTo2DParamCommand>
    : detail::ParameterStructuralEditCommandTraits {};
template<> struct CommandTraits<parameter::paramedit::FlipXCommand>
    : detail::ParameterStructuralEditCommandTraits {};
template<> struct CommandTraits<parameter::paramedit::FlipYCommand>
    : detail::ParameterStructuralEditCommandTraits {};
template<> struct CommandTraits<parameter::paramedit::Flip1DCommand>
    : detail::ParameterStructuralEditCommandTraits {};
template<> struct CommandTraits<parameter::paramedit::MirrorHorizontallyCommand>
    : detail::ParameterStructuralEditCommandTraits {};
template<> struct CommandTraits<parameter::paramedit::MirrorVerticallyCommand>
    : detail::ParameterStructuralEditCommandTraits {};
template<> struct CommandTraits<parameter::paramedit::MirroredAutoFillDir1Command>
    : detail::ParameterStructuralEditCommandTraits {};
template<> struct CommandTraits<parameter::paramedit::MirroredAutoFillDir2Command>
    : detail::ParameterStructuralEditCommandTraits {};
template<> struct CommandTraits<parameter::paramedit::MirroredAutoFillDir3Command>
    : detail::ParameterStructuralEditCommandTraits {};
template<> struct CommandTraits<parameter::paramedit::MirroredAutoFillDir4Command>
    : detail::ParameterStructuralEditCommandTraits {};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::parameter::paramedit {

struct LinkToArguments {
    std::shared_ptr<nicxlive::core::param::Parameter> toParam;
    std::int32_t fromAxis = 0;
    std::int32_t toAxis = 0;
};
struct ToggleParameterArmArguments {
    std::int32_t index = 0;
};

} // namespace nijigenerate::commands::parameter::paramedit

namespace nijigenerate::commands {

template<> struct CommandFields<parameter::paramedit::LinkToArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&parameter::paramedit::LinkToArguments::toParam,
            "toParam", "Target parameter to copy", "Parameter">{},
        FieldDescriptor<&parameter::paramedit::LinkToArguments::fromAxis,
            "fromAxis", "axis in source parameter", "int">{},
        FieldDescriptor<&parameter::paramedit::LinkToArguments::toAxis,
            "toAxis", "axis in dest parameter", "int">{}};
};
template<> struct CommandFields<parameter::paramedit::ToggleParameterArmArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&parameter::paramedit::ToggleParameterArmArguments::index,
            "index", "specify the index of the armed parameter in the parent group.", "int">{}};
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::parameter::paramedit {

class CopyParameterCommand : public ExCommand<>,
        public std::enable_shared_from_this<CopyParameterCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(CopyParameterCommand)
    CopyParameterCommand();
    CopyParameterCommand(const CopyParameterCommand&) = delete;
    CopyParameterCommand& operator=(const CopyParameterCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class PasteParameterCommand : public ExCommand<>,
        public std::enable_shared_from_this<PasteParameterCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(PasteParameterCommand)
    PasteParameterCommand();
    PasteParameterCommand(const PasteParameterCommand&) = delete;
    PasteParameterCommand& operator=(const PasteParameterCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
    bool runnable(std::shared_ptr<Context> context) override;
};
class PasteParameterWithFlipCommand : public ExCommand<>,
        public std::enable_shared_from_this<PasteParameterWithFlipCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(PasteParameterWithFlipCommand)
    PasteParameterWithFlipCommand();
    PasteParameterWithFlipCommand(const PasteParameterWithFlipCommand&) = delete;
    PasteParameterWithFlipCommand& operator=(const PasteParameterWithFlipCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
    bool runnable(std::shared_ptr<Context> context) override;
};
class DuplicateParameterCommand : public ::nijigenerate::commands::detail::CovariantCommand<DuplicateParameterCommand> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<DuplicateParameterCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = CreateResult<std::shared_ptr<nicxlive::core::param::Parameter>>;
    NG_PARAMEDIT_DESCRIPTION_DECL(DuplicateParameterCommand)
    DuplicateParameterCommand();
    DuplicateParameterCommand(const DuplicateParameterCommand&) = delete;
    DuplicateParameterCommand& operator=(const DuplicateParameterCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};
class DuplicateParameterWithFlipCommand : public ::nijigenerate::commands::detail::CovariantCommand<DuplicateParameterWithFlipCommand> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<DuplicateParameterWithFlipCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = CreateResult<std::shared_ptr<nicxlive::core::param::Parameter>>;
    NG_PARAMEDIT_DESCRIPTION_DECL(DuplicateParameterWithFlipCommand)
    DuplicateParameterWithFlipCommand();
    DuplicateParameterWithFlipCommand(const DuplicateParameterWithFlipCommand&) = delete;
    DuplicateParameterWithFlipCommand& operator=(const DuplicateParameterWithFlipCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};
class DeleteParameterCommand : public ::nijigenerate::commands::detail::CovariantCommand<DeleteParameterCommand> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<DeleteParameterCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = DeleteResult<std::shared_ptr<nicxlive::core::param::Parameter>>;
    NG_PARAMEDIT_DESCRIPTION_DECL(DeleteParameterCommand)
    DeleteParameterCommand();
    DeleteParameterCommand(const DeleteParameterCommand&) = delete;
    DeleteParameterCommand& operator=(const DeleteParameterCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};
class LinkToCommand : public ExCommand<LinkToArguments>,
        public std::enable_shared_from_this<LinkToCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString description;
        std::shared_ptr<nicxlive::core::param::Parameter> target;
        std::int32_t fromAxis, toAxis;
        static SourceConstructorFrame capture(std::shared_ptr<nicxlive::core::param::Parameter>& target,
            std::int32_t& fromAxis, std::int32_t& toAxis);
    };
    explicit LinkToCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<LinkToCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    LinkToCommand(std::shared_ptr<nicxlive::core::param::Parameter> toParam,
        std::int32_t fromAxis, std::int32_t toAxis);
    LinkToCommand(const LinkToCommand&) = delete;
    LinkToCommand& operator=(const LinkToCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class ToggleParameterArmCommand : public ExCommand<ToggleParameterArmArguments>,
        public std::enable_shared_from_this<ToggleParameterArmCommand> {
    struct SourceConstructorFrame {
        compat::RetainedString description;
        std::int32_t index;
        static SourceConstructorFrame capture(std::int32_t& index);
    };
    explicit ToggleParameterArmCommand(SourceConstructorFrame frame);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ToggleParameterArmCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    explicit ToggleParameterArmCommand(std::int32_t index);
    ToggleParameterArmCommand(const ToggleParameterArmCommand&) = delete;
    ToggleParameterArmCommand& operator=(const ToggleParameterArmCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetParameterKeypointCommand : public ExCommand<>,
        public std::enable_shared_from_this<SetParameterKeypointCommand> {
    explicit SetParameterKeypointCommand(detail::KeypointCommandText text);
    static detail::KeypointCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetParameterKeypointCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    SetParameterKeypointCommand();
    SetParameterKeypointCommand(const SetParameterKeypointCommand&) = delete;
    SetParameterKeypointCommand& operator=(const SetParameterKeypointCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetArmedParameterAndKeypointCommand : public ExCommand<>,
        public std::enable_shared_from_this<SetArmedParameterAndKeypointCommand> {
    explicit SetArmedParameterAndKeypointCommand(detail::KeypointCommandText text);
    static detail::KeypointCommandText sourceConstructorText();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SetArmedParameterAndKeypointCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    SetArmedParameterAndKeypointCommand();
    SetArmedParameterAndKeypointCommand(const SetArmedParameterAndKeypointCommand&) = delete;
    SetArmedParameterAndKeypointCommand& operator=(const SetArmedParameterAndKeypointCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class SetStartingKeyFrameCommand : public ExCommand<>,
        public std::enable_shared_from_this<SetStartingKeyFrameCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    NG_PARAMEDIT_DESCRIPTION_DECL(SetStartingKeyFrameCommand)
    SetStartingKeyFrameCommand();
    SetStartingKeyFrameCommand(const SetStartingKeyFrameCommand&) = delete;
    SetStartingKeyFrameCommand& operator=(const SetStartingKeyFrameCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};

} // namespace nijigenerate::commands::parameter::paramedit

namespace nicxlive::core::d_runtime {
#undef NG_PARAMEDIT_DESCRIPTION_DECL
#define NG_PARAMEDIT_COMMAND_SOURCE_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::commands::parameter::paramedit::Type> { \
        inline static constexpr char className[] = "nijigenerate.commands.parameter.paramedit." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.parameter.paramedit." #Type ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::parameter::paramedit::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.parameter.paramedit." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.parameter.paramedit." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(ConvertTo2DParamCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(FlipXCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(FlipYCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(Flip1DCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(MirrorHorizontallyCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(MirrorVerticallyCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(MirroredAutoFillDir1Command)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(MirroredAutoFillDir2Command)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(MirroredAutoFillDir3Command)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(MirroredAutoFillDir4Command)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(CopyParameterCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(PasteParameterCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(PasteParameterWithFlipCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(DuplicateParameterCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(DuplicateParameterWithFlipCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(DeleteParameterCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(LinkToCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(ToggleParameterArmCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(SetParameterKeypointCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(SetArmedParameterAndKeypointCommand)
NG_PARAMEDIT_COMMAND_SOURCE_NAMES(SetStartingKeyFrameCommand)
#undef NG_PARAMEDIT_COMMAND_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands {

template<> struct CommandTraits<parameter::paramedit::CopyParameterCommand> {
    static constexpr auto attributes = std::tuple{};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<parameter::paramedit::PasteParameterCommand> {
    static constexpr auto attributes = std::tuple{EffectCreate};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<parameter::paramedit::PasteParameterWithFlipCommand> {
    static constexpr auto attributes = std::tuple{EffectCreate};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<parameter::paramedit::DuplicateParameterCommand> {
    static constexpr auto attributes = std::tuple{EffectCreate};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<parameter::paramedit::DuplicateParameterWithFlipCommand> {
    static constexpr auto attributes = std::tuple{EffectCreate};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<parameter::paramedit::DeleteParameterCommand> {
    static constexpr auto attributes = std::tuple{EffectDelete};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<parameter::paramedit::LinkToCommand> {
    static constexpr auto attributes = std::tuple{EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<parameter::paramedit::LinkToArguments>, Command>;
};
template<> struct CommandTraits<parameter::paramedit::ToggleParameterArmCommand> {
    static constexpr auto attributes = std::tuple{EffectStructuralEdit};
    using BaseClasses = CommandTypeList<ExCommand<parameter::paramedit::ToggleParameterArmArguments>, Command>;
};
template<> struct CommandTraits<parameter::paramedit::SetParameterKeypointCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<parameter::paramedit::SetArmedParameterAndKeypointCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<parameter::paramedit::SetStartingKeyFrameCommand> {
    static constexpr auto attributes = std::tuple{EffectKeyframeEdit};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};

#define NG_PARAMEDIT_DESCRIPTION_PROGRAM(Type, BaseBody) \
template<> struct SourceCommandConstructor<parameter::paramedit::Type> { \
    template<class... Args> struct Program { \
        using C = parameter::paramedit::Type; \
        static consteval bool supported() { \
            return sizeof...(Args) == 0 && requires { \
                { C::sourceConstructorDescription() } -> std::same_as<compat::RetainedString>; \
            } && SourceExCommandConstructorProgram<CommandNoArguments>::BaseBody(); \
        } \
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args) \
            requires(supported()) { \
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...); \
        } \
    }; \
};
NG_PARAMEDIT_DESCRIPTION_PROGRAM(ConvertTo2DParamCommand, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(FlipXCommand, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(FlipYCommand, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(Flip1DCommand, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(MirrorHorizontallyCommand, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(MirrorVerticallyCommand, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(MirroredAutoFillDir1Command, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(MirroredAutoFillDir2Command, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(MirroredAutoFillDir3Command, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(MirroredAutoFillDir4Command, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(CopyParameterCommand, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(PasteParameterCommand, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(PasteParameterWithFlipCommand, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(DuplicateParameterCommand, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(DuplicateParameterWithFlipCommand, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(DeleteParameterCommand, descriptionSupported)
NG_PARAMEDIT_DESCRIPTION_PROGRAM(SetStartingKeyFrameCommand, pairSupported)
#undef NG_PARAMEDIT_DESCRIPTION_PROGRAM

#define NG_PARAMEDIT_KEYPOINT_PROGRAM(Type) \
template<> struct SourceCommandConstructor<parameter::paramedit::Type> { \
    template<class... Args> struct Program { \
        using C = parameter::paramedit::Type; \
        static consteval bool supported() { \
            return sizeof...(Args) == 0 && requires { \
                { C::sourceConstructorText() } -> std::same_as<parameter::paramedit::detail::KeypointCommandText>; \
            } && SourceExCommandConstructorProgram<CommandNoArguments>::pairSupported(); \
        } \
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args) \
            requires(supported()) { \
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...); \
        } \
    }; \
};
NG_PARAMEDIT_KEYPOINT_PROGRAM(SetParameterKeypointCommand)
NG_PARAMEDIT_KEYPOINT_PROGRAM(SetArmedParameterAndKeypointCommand)
#undef NG_PARAMEDIT_KEYPOINT_PROGRAM

template<> struct SourceCommandConstructor<parameter::paramedit::LinkToCommand> {
    template<class... Args> struct Program {
        using C = parameter::paramedit::LinkToCommand;
        using Parameter = std::shared_ptr<nicxlive::core::param::Parameter>;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 3) return false;
            else return []<class P, class From, class To>(CommandTypeList<P, From, To>) {
                return ngCommandInitializationSupported<Parameter, P&&>() &&
                    ngCommandInitializationSupported<std::int32_t, From&&>() &&
                    ngCommandInitializationSupported<std::int32_t, To&&>() &&
                    requires(Parameter& parameter, std::int32_t& from, std::int32_t& to) {
                        { C::SourceConstructorFrame::capture(parameter, from, to) } ->
                            std::same_as<typename C::SourceConstructorFrame>;
                    } && SourceExCommandConstructorProgram<parameter::paramedit::LinkToArguments>::
                        preferredSupported<Parameter, std::int32_t, std::int32_t>();
            }(CommandTypeList<Args...>{}) &&
                detail::declaredSourceParametersSupported<std::tuple<Parameter, std::int32_t, std::int32_t>, Args...>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            return detail::constructDeclaredSourceCommand<C, std::tuple<Parameter, std::int32_t, std::int32_t>>(
                allocator, std::forward<Args>(args)...);
        }
    };
};

template<> struct SourceCommandConstructor<parameter::paramedit::ToggleParameterArmCommand> {
    template<class... Args> struct Program {
        using C = parameter::paramedit::ToggleParameterArmCommand;
        static consteval bool supported() {
            if constexpr (sizeof...(Args) != 1) return false;
            else return (ngCommandInitializationSupported<std::int32_t, Args&&>() && ...) &&
                requires(std::int32_t& index) {
                    { C::SourceConstructorFrame::capture(index) } -> std::same_as<typename C::SourceConstructorFrame>;
                } && SourceExCommandConstructorProgram<parameter::paramedit::ToggleParameterArmArguments>::
                    preferredSupported<std::int32_t>() &&
                detail::declaredSourceParametersSupported<std::tuple<std::int32_t>, Args...>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args)
            requires(supported()) {
            return detail::constructDeclaredSourceCommand<C, std::tuple<std::int32_t>>(
                allocator, std::forward<Args>(args)...);
        }
    };
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::parameter::paramedit {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct LinkArguments {
    static auto get() {
        return std::tuple{std::shared_ptr<nicxlive::core::param::Parameter>{},
            std::int32_t{0}, std::int32_t{0}};
    }
};
struct ArmArguments { static auto get() { return std::tuple{std::int32_t{0}}; } };
template<auto Id, class C>
using Optional = ::nijigenerate::commands::detail::OptionalCommandRegistration<Id, C>;
template<auto Id, class C, class Args>
using Required = ::nijigenerate::commands::detail::ConstructedCommandRegistration<Id, C, Args>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Optional<ParameditCommand::ConvertTo2DParam, ConvertTo2DParamCommand>,
    Optional<ParameditCommand::FlipX, FlipXCommand>,
    Optional<ParameditCommand::FlipY, FlipYCommand>,
    Optional<ParameditCommand::Flip1D, Flip1DCommand>,
    Optional<ParameditCommand::MirrorHorizontally, MirrorHorizontallyCommand>,
    Optional<ParameditCommand::MirrorVertically, MirrorVerticallyCommand>,
    Optional<ParameditCommand::MirroredAutoFillDir1, MirroredAutoFillDir1Command>,
    Optional<ParameditCommand::MirroredAutoFillDir2, MirroredAutoFillDir2Command>,
    Optional<ParameditCommand::MirroredAutoFillDir3, MirroredAutoFillDir3Command>,
    Optional<ParameditCommand::MirroredAutoFillDir4, MirroredAutoFillDir4Command>,
    Optional<ParameditCommand::CopyParameter, CopyParameterCommand>,
    Optional<ParameditCommand::PasteParameter, PasteParameterCommand>,
    Optional<ParameditCommand::PasteParameterWithFlip, PasteParameterWithFlipCommand>,
    Optional<ParameditCommand::DuplicateParameter, DuplicateParameterCommand>,
    Optional<ParameditCommand::DuplicateParameterWithFlip, DuplicateParameterWithFlipCommand>,
    Optional<ParameditCommand::DeleteParameter, DeleteParameterCommand>,
    Optional<ParameditCommand::LinkTo, LinkToCommand>,
    Optional<ParameditCommand::ToggleParameterArm, ToggleParameterArmCommand>,
    Optional<ParameditCommand::SetParameterKeypoint, SetParameterKeypointCommand>,
    Optional<ParameditCommand::SetArmedParameterAndKeypoint, SetArmedParameterAndKeypointCommand>,
    Optional<ParameditCommand::SetStartingKeyFrame, SetStartingKeyFrameCommand>,
    Required<ParameditCommand::LinkTo, LinkToCommand, LinkArguments>,
    Required<ParameditCommand::ToggleParameterArm, ToggleParameterArmCommand, ArmArguments>>;
}
template<class T> requires (std::same_as<T, ParameditCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitParameterEditCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<parameter::paramedit::ParameditCommand> {
    static consteval bool supported() {
        return parameter::paramedit::initialization::Program::supported<parameter::paramedit::initialization::Table>();
    }
    template<class ActualKey = parameter::paramedit::ParameditCommand>
        requires (std::same_as<ActualKey, parameter::paramedit::ParameditCommand> && supported())
    static void invoke() { parameter::paramedit::ngInitCommands<ActualKey>(); }
};
}
