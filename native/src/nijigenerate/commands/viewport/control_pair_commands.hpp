#pragma once

#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <concepts>
#include <nijigenerate/commands/covariant_command.hpp>
#include <nijigenerate/compat/retained_string_slice.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <memory>

namespace nicxlive::core::nodes { class Node; }

namespace nijigenerate::commands::viewport::control {

struct AddFlipPairArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> left;
    std::shared_ptr<nicxlive::core::nodes::Node> right;
};
struct AutoAddFlipPairsArguments {
    compat::RetainedStringSlice leftPattern;
    compat::RetainedStringSlice rightPattern;
};
struct RemoveFlipPairArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> left;
    std::shared_ptr<nicxlive::core::nodes::Node> right;
};

} // namespace nijigenerate::commands::viewport::control

namespace nijigenerate::commands {

template<> struct CommandFields<viewport::control::AddFlipPairArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&viewport::control::AddFlipPairArguments::left,
            "left", "UUID of the left-side node", "Node">{},
        FieldDescriptor<&viewport::control::AddFlipPairArguments::right,
            "right", "UUID of the right-side node", "Node">{}};
};
template<> struct CommandFields<viewport::control::AutoAddFlipPairsArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&viewport::control::AutoAddFlipPairsArguments::leftPattern,
            "leftPattern", "Pattern to replace on left-side node names, for example ::L or _l", "string">{},
        FieldDescriptor<&viewport::control::AutoAddFlipPairsArguments::rightPattern,
            "rightPattern", "Replacement pattern for right-side node names, for example ::R or _r", "string">{}};
};
template<> struct CommandFields<viewport::control::RemoveFlipPairArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&viewport::control::RemoveFlipPairArguments::left,
            "left", "UUID of one side of the flip pair", "Node">{},
        FieldDescriptor<&viewport::control::RemoveFlipPairArguments::right,
            "right", "UUID of the other side of the flip pair", "Node">{}};
};

} // namespace nijigenerate::commands

namespace nijigenerate::commands::viewport::control {

class ListFlipPairsCommand : public ::nijigenerate::commands::detail::CovariantCommand<ListFlipPairsCommand> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<ListFlipPairsCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = ExCommandResult<::nijigenerate::core::settings::SettingsJsonValue>;
    ListFlipPairsCommand();
    ListFlipPairsCommand(const ListFlipPairsCommand&) = delete;
    ListFlipPairsCommand& operator=(const ListFlipPairsCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context) { return runTyped(context); }
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ListFlipPairsCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit ListFlipPairsCommand(compat::RetainedString description);
};
class AddFlipPairCommand : public ExCommand<AddFlipPairArguments>,
    public std::enable_shared_from_this<AddFlipPairCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    AddFlipPairCommand();
    AddFlipPairCommand(const AddFlipPairCommand&) = delete;
    AddFlipPairCommand& operator=(const AddFlipPairCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<AddFlipPairCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit AddFlipPairCommand(compat::RetainedString description);
};
class AutoAddFlipPairsCommand : public ExCommand<AutoAddFlipPairsArguments>,
    public std::enable_shared_from_this<AutoAddFlipPairsCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    AutoAddFlipPairsCommand();
    AutoAddFlipPairsCommand(const AutoAddFlipPairsCommand&) = delete;
    AutoAddFlipPairsCommand& operator=(const AutoAddFlipPairsCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<AutoAddFlipPairsCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit AutoAddFlipPairsCommand(compat::RetainedString description);
};
class RemoveFlipPairCommand : public ExCommand<RemoveFlipPairArguments>,
    public std::enable_shared_from_this<RemoveFlipPairCommand> {
protected:
    void prepareSourceClassInfo() override;
public:
    RemoveFlipPairCommand();
    RemoveFlipPairCommand(const RemoveFlipPairCommand&) = delete;
    RemoveFlipPairCommand& operator=(const RemoveFlipPairCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
private:
    friend struct ::nijigenerate::commands::SourceCommandConstructor<RemoveFlipPairCommand>;
    static compat::RetainedString sourceConstructorDescription();
    explicit RemoveFlipPairCommand(compat::RetainedString description);
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
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(ListFlipPairsCommand)
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(AddFlipPairCommand)
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(AutoAddFlipPairsCommand)
NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES(RemoveFlipPairCommand)
#undef NG_VIEWPORT_CONTROL_COMMAND_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands {

template<> struct CommandTraits<viewport::control::ListFlipPairsCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden};
    using BaseClasses = CommandTypeList<ExCommand<>, Command>;
};
template<> struct CommandTraits<viewport::control::AddFlipPairCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<viewport::control::AddFlipPairArguments>, Command>;
};
template<> struct CommandTraits<viewport::control::AutoAddFlipPairsCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<viewport::control::AutoAddFlipPairsArguments>, Command>;
};
template<> struct CommandTraits<viewport::control::RemoveFlipPairCommand> {
    static constexpr auto attributes = std::tuple{ShortcutHidden, EffectConfigEdit};
    using BaseClasses = CommandTypeList<ExCommand<viewport::control::RemoveFlipPairArguments>, Command>;
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

NG_CONTROL_DESCRIPTION_CONSTRUCTOR(ListFlipPairsCommand, CommandNoArguments, descriptionSupported)
NG_CONTROL_DESCRIPTION_CONSTRUCTOR(AddFlipPairCommand, viewport::control::AddFlipPairArguments, pairSupported)
NG_CONTROL_DESCRIPTION_CONSTRUCTOR(AutoAddFlipPairsCommand, viewport::control::AutoAddFlipPairsArguments, pairSupported)
NG_CONTROL_DESCRIPTION_CONSTRUCTOR(RemoveFlipPairCommand, viewport::control::RemoveFlipPairArguments, pairSupported)

#undef NG_CONTROL_DESCRIPTION_CONSTRUCTOR

} // namespace nijigenerate::commands
