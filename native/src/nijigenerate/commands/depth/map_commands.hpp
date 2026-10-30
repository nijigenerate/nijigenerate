#pragma once
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/covariant_command.hpp>
#include <memory>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include "map.hpp"
#include <nijigenerate/commands/detail/registered_dispatch.hpp>
#include <nijigenerate/core/settings_value.hpp>
#include <limits>
#include <tuple>
#include <utility>
namespace nijigenerate::commands::depth::map {
struct ListDepthsArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> target{};
};
struct SetDepthsArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> target{};
    compat::Slice<float> depths{};
};
struct ClearDepthsArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> target{};
};
struct ListDepthOpsArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> target{};
};
struct SetDepthOpsArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> target{};
    core::settings::SettingsJsonValue operations{};
};
struct AddDepthOpArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> target{};
    core::settings::SettingsJsonValue operation{};
    std::int32_t index{};
};
struct UpdateDepthOpArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> target{};
    std::int32_t index{};
    core::settings::SettingsJsonValue operation{};
};
struct RemoveDepthOpArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> target{};
    std::int32_t index{};
};
struct MoveDepthOpArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> target{};
    std::int32_t fromIndex{};
    std::int32_t toIndex{};
};
struct ClearDepthOpsArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> target{};
};
struct ApplyDepthOpsArguments {
    std::shared_ptr<nicxlive::core::nodes::Node> target{};
};
struct ImportPSDDepthsArguments {
    compat::RetainedString path{};
    bool invert{};
    float backDepth = std::numeric_limits<float>::quiet_NaN();
    float frontDepth = std::numeric_limits<float>::quiet_NaN();
    float depthScale = std::numeric_limits<float>::quiet_NaN();
    compat::RetainedString convolution{};
    compat::RetainedString channel{};
    std::int32_t customRadius{};
    float alphaThreshold = std::numeric_limits<float>::quiet_NaN();
    compat::RetainedString missingPolicy{};
    bool matchDirectGridName{};
};
}
namespace nijigenerate::commands {
template<> struct CommandFields<depth::map::ListDepthsArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::map::ListDepthsArguments::target, "target", "DepthMapped target node", "Node">{}};
};
template<> struct CommandFields<depth::map::SetDepthsArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::map::SetDepthsArguments::target, "target", "DepthMapped target node", "Node">{},
        FieldDescriptor<&depth::map::SetDepthsArguments::depths, "depths", "Per-vertex depth values; length must match target vertices", "float[]">{}};
};
template<> struct CommandFields<depth::map::ClearDepthsArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::map::ClearDepthsArguments::target, "target", "DepthMapped target node", "Node">{}};
};
template<> struct CommandFields<depth::map::ListDepthOpsArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::map::ListDepthOpsArguments::target, "target", "Depth operation target node", "Node">{}};
};
template<> struct CommandFields<depth::map::SetDepthOpsArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::map::SetDepthOpsArguments::target, "target", "Depth operation target node", "Node">{},
        FieldDescriptor<&depth::map::SetDepthOpsArguments::operations, "operations", "Depth operations array", "JSONValue", true, false, CommandJsonSchema::depthOperations>{}};
};
template<> struct CommandFields<depth::map::AddDepthOpArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::map::AddDepthOpArguments::target, "target", "Depth operation target node", "Node">{},
        FieldDescriptor<&depth::map::AddDepthOpArguments::operation, "operation", "Depth operation object", "JSONValue", true, false, CommandJsonSchema::depthOperation>{},
        FieldDescriptor<&depth::map::AddDepthOpArguments::index, "index", "Insertion index, or -1 to append", "int">{}};
};
template<> struct CommandFields<depth::map::UpdateDepthOpArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::map::UpdateDepthOpArguments::target, "target", "Depth operation target node", "Node">{},
        FieldDescriptor<&depth::map::UpdateDepthOpArguments::index, "index", "Operation index", "int">{},
        FieldDescriptor<&depth::map::UpdateDepthOpArguments::operation, "operation", "Replacement depth operation object", "JSONValue", true, false, CommandJsonSchema::depthOperation>{}};
};
template<> struct CommandFields<depth::map::RemoveDepthOpArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::map::RemoveDepthOpArguments::target, "target", "Depth operation target node", "Node">{},
        FieldDescriptor<&depth::map::RemoveDepthOpArguments::index, "index", "Operation index", "int">{}};
};
template<> struct CommandFields<depth::map::MoveDepthOpArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::map::MoveDepthOpArguments::target, "target", "Depth operation target node", "Node">{},
        FieldDescriptor<&depth::map::MoveDepthOpArguments::fromIndex, "fromIndex", "Current operation index", "int">{},
        FieldDescriptor<&depth::map::MoveDepthOpArguments::toIndex, "toIndex", "Destination operation index", "int">{}};
};
template<> struct CommandFields<depth::map::ClearDepthOpsArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::map::ClearDepthOpsArguments::target, "target", "Depth operation target node", "Node">{}};
};
template<> struct CommandFields<depth::map::ApplyDepthOpsArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::map::ApplyDepthOpsArguments::target, "target", "Depth operation target node", "Node">{}};
};
template<> struct CommandFields<depth::map::ImportPSDDepthsArguments> {
    static constexpr auto descriptors = std::tuple{
        FieldDescriptor<&depth::map::ImportPSDDepthsArguments::path, "path", "Path to PSD or PNG depth source file", "string">{},
        FieldDescriptor<&depth::map::ImportPSDDepthsArguments::invert, "invert", "Invert depth values; default is white as front", "bool">{},
        FieldDescriptor<&depth::map::ImportPSDDepthsArguments::backDepth, "backDepth", "Depth value for black/back", "float">{},
        FieldDescriptor<&depth::map::ImportPSDDepthsArguments::frontDepth, "frontDepth", "Depth value for white/front", "float">{},
        FieldDescriptor<&depth::map::ImportPSDDepthsArguments::depthScale, "depthScale", "Multiplier applied to imported depth values", "float">{},
        FieldDescriptor<&depth::map::ImportPSDDepthsArguments::convolution, "convolution", "Sampling convolution mode", "string">{},
        FieldDescriptor<&depth::map::ImportPSDDepthsArguments::channel, "channel", "Depth channel", "string">{},
        FieldDescriptor<&depth::map::ImportPSDDepthsArguments::customRadius, "customRadius", "Custom convolution radius", "int">{},
        FieldDescriptor<&depth::map::ImportPSDDepthsArguments::alphaThreshold, "alphaThreshold", "Minimum alpha for valid depth pixels", "float">{},
        FieldDescriptor<&depth::map::ImportPSDDepthsArguments::missingPolicy, "missingPolicy", "Policy for vertices with no sampled depth", "string">{},
        FieldDescriptor<&depth::map::ImportPSDDepthsArguments::matchDirectGridName, "matchDirectGridName", "Allow direct PSD layer name to GridDeformer name matching", "bool">{}};
};
}
namespace nijigenerate::commands::depth::map {
using DepthMapConstructorText = std::pair<compat::RetainedString, compat::RetainedString>;
#define NG_DEPTH_MAP_CONSTRUCTOR_DECL(Class) \
    static DepthMapConstructorText sourceConstructorTexts(); \
    explicit Class(DepthMapConstructorText texts); \
    friend struct ::nijigenerate::commands::SourceCommandConstructor<Class>;
class ListDepthsCommand : public ::nijigenerate::commands::detail::CovariantCommand<ListDepthsCommand, ListDepthsArguments> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<ListDepthsCommand, ListDepthsArguments>;
    NG_DEPTH_MAP_CONSTRUCTOR_DECL(ListDepthsCommand)
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = ExCommandResult<core::settings::SettingsJsonValue>;
    ListDepthsCommand();
    ListDepthsCommand(const ListDepthsCommand&) = delete;
    ListDepthsCommand& operator=(const ListDepthsCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context) { return runTyped(context); }
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};
class SetDepthsCommand : public ExCommand<SetDepthsArguments>, public std::enable_shared_from_this<SetDepthsCommand> {
    NG_DEPTH_MAP_CONSTRUCTOR_DECL(SetDepthsCommand)
protected:
    void prepareSourceClassInfo() override;
public:
    SetDepthsCommand();
    SetDepthsCommand(const SetDepthsCommand&) = delete;
    SetDepthsCommand& operator=(const SetDepthsCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class ClearDepthsCommand : public ExCommand<ClearDepthsArguments>, public std::enable_shared_from_this<ClearDepthsCommand> {
    NG_DEPTH_MAP_CONSTRUCTOR_DECL(ClearDepthsCommand)
protected:
    void prepareSourceClassInfo() override;
public:
    ClearDepthsCommand();
    ClearDepthsCommand(const ClearDepthsCommand&) = delete;
    ClearDepthsCommand& operator=(const ClearDepthsCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class ListDepthOpsCommand : public ::nijigenerate::commands::detail::CovariantCommand<ListDepthOpsCommand, ListDepthOpsArguments> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<ListDepthOpsCommand, ListDepthOpsArguments>;
    NG_DEPTH_MAP_CONSTRUCTOR_DECL(ListDepthOpsCommand)
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = ExCommandResult<core::settings::SettingsJsonValue>;
    ListDepthOpsCommand();
    ListDepthOpsCommand(const ListDepthOpsCommand&) = delete;
    ListDepthOpsCommand& operator=(const ListDepthOpsCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context) { return runTyped(context); }
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};
class SetDepthOpsCommand : public ExCommand<SetDepthOpsArguments>, public std::enable_shared_from_this<SetDepthOpsCommand> {
    NG_DEPTH_MAP_CONSTRUCTOR_DECL(SetDepthOpsCommand)
protected:
    void prepareSourceClassInfo() override;
public:
    SetDepthOpsCommand();
    SetDepthOpsCommand(const SetDepthOpsCommand&) = delete;
    SetDepthOpsCommand& operator=(const SetDepthOpsCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class AddDepthOpCommand : public ExCommand<AddDepthOpArguments>, public std::enable_shared_from_this<AddDepthOpCommand> {
    NG_DEPTH_MAP_CONSTRUCTOR_DECL(AddDepthOpCommand)
protected:
    void prepareSourceClassInfo() override;
public:
    AddDepthOpCommand();
    AddDepthOpCommand(const AddDepthOpCommand&) = delete;
    AddDepthOpCommand& operator=(const AddDepthOpCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class UpdateDepthOpCommand : public ExCommand<UpdateDepthOpArguments>, public std::enable_shared_from_this<UpdateDepthOpCommand> {
    NG_DEPTH_MAP_CONSTRUCTOR_DECL(UpdateDepthOpCommand)
protected:
    void prepareSourceClassInfo() override;
public:
    UpdateDepthOpCommand();
    UpdateDepthOpCommand(const UpdateDepthOpCommand&) = delete;
    UpdateDepthOpCommand& operator=(const UpdateDepthOpCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class RemoveDepthOpCommand : public ExCommand<RemoveDepthOpArguments>, public std::enable_shared_from_this<RemoveDepthOpCommand> {
    NG_DEPTH_MAP_CONSTRUCTOR_DECL(RemoveDepthOpCommand)
protected:
    void prepareSourceClassInfo() override;
public:
    RemoveDepthOpCommand();
    RemoveDepthOpCommand(const RemoveDepthOpCommand&) = delete;
    RemoveDepthOpCommand& operator=(const RemoveDepthOpCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class MoveDepthOpCommand : public ExCommand<MoveDepthOpArguments>, public std::enable_shared_from_this<MoveDepthOpCommand> {
    NG_DEPTH_MAP_CONSTRUCTOR_DECL(MoveDepthOpCommand)
protected:
    void prepareSourceClassInfo() override;
public:
    MoveDepthOpCommand();
    MoveDepthOpCommand(const MoveDepthOpCommand&) = delete;
    MoveDepthOpCommand& operator=(const MoveDepthOpCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class ClearDepthOpsCommand : public ExCommand<ClearDepthOpsArguments>, public std::enable_shared_from_this<ClearDepthOpsCommand> {
    NG_DEPTH_MAP_CONSTRUCTOR_DECL(ClearDepthOpsCommand)
protected:
    void prepareSourceClassInfo() override;
public:
    ClearDepthOpsCommand();
    ClearDepthOpsCommand(const ClearDepthOpsCommand&) = delete;
    ClearDepthOpsCommand& operator=(const ClearDepthOpsCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class ApplyDepthOpsCommand : public ExCommand<ApplyDepthOpsArguments>, public std::enable_shared_from_this<ApplyDepthOpsCommand> {
    NG_DEPTH_MAP_CONSTRUCTOR_DECL(ApplyDepthOpsCommand)
protected:
    void prepareSourceClassInfo() override;
public:
    ApplyDepthOpsCommand();
    ApplyDepthOpsCommand(const ApplyDepthOpsCommand&) = delete;
    ApplyDepthOpsCommand& operator=(const ApplyDepthOpsCommand&) = delete;
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> context) override;
};
class ImportPSDDepthsCommand : public ::nijigenerate::commands::detail::CovariantCommand<ImportPSDDepthsCommand, ImportPSDDepthsArguments> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<ImportPSDDepthsCommand, ImportPSDDepthsArguments>;
    static DepthMapConstructorText sourceConstructorTexts();
    ImportPSDDepthsCommand(DepthMapConstructorText texts, compat::RetainedString path, bool invert,
        float backDepth, float frontDepth, float depthScale, compat::RetainedString convolution,
        compat::RetainedString channel, std::int32_t customRadius, float alphaThreshold,
        compat::RetainedString missingPolicy, bool matchDirectGridName);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ImportPSDDepthsCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    using Result = ExCommandResult<core::settings::SettingsJsonValue>;
    ImportPSDDepthsCommand(compat::RetainedString path, bool invert = false, float backDepth = -1.0f,
        float frontDepth = 1.0f, float depthScale = 1.0f,
        compat::RetainedString convolution = compat::RetainedString::fromStaticLiteral("Gaussian3x3"),
        compat::RetainedString channel = compat::RetainedString::fromStaticLiteral("AverageRGB"),
        std::int32_t customRadius = 3, float alphaThreshold = 0.01f,
        compat::RetainedString missingPolicy = compat::RetainedString::fromStaticLiteral("KeepExisting"),
        bool matchDirectGridName = true);
    ImportPSDDepthsCommand(const ImportPSDDepthsCommand&) = delete;
    ImportPSDDepthsCommand& operator=(const ImportPSDDepthsCommand&) = delete;
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& context) { return runTyped(context); }
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> context);
};
#undef NG_DEPTH_MAP_CONSTRUCTOR_DECL
void ngInitDepthMapCommands();
template<DepthMapCommand id, class... Args>
std::shared_ptr<CommandResult> ngRunDepthMapCommand(std::shared_ptr<Context> context, Args&&... args) {
    static_assert(id == DepthMapCommand::SetDepthOps || id == DepthMapCommand::SetDepths || id == DepthMapCommand::ApplyDepthOps);
    using C = std::conditional_t<id == DepthMapCommand::SetDepthOps, SetDepthOpsCommand,
        std::conditional_t<id == DepthMapCommand::SetDepths, SetDepthsCommand, ApplyDepthOpsCommand>>;
    constexpr CommandDispatchSource source = [] {
        if constexpr (id == DepthMapCommand::SetDepthOps)
            return CommandDispatchSource{"DepthMapCommand.SetDepthOps", "DepthMapCommand", "SetDepthOpsCommand"};
        else if constexpr (id == DepthMapCommand::SetDepths)
            return CommandDispatchSource{"DepthMapCommand.SetDepths", "DepthMapCommand", "SetDepthsCommand"};
        else
            return CommandDispatchSource{"DepthMapCommand.ApplyDepthOps", "DepthMapCommand", "ApplyDepthOpsCommand"};
    }();
    return ngDispatchRegisteredCommand<C>(commands, id, context, source, std::forward<Args>(args)...);
}
}
namespace nijigenerate::commands {
template<> struct CommandTraits<depth::map::ListDepthsCommand> {
    static constexpr auto attributes = std::tuple{EffectApply};
    using BaseClasses = CommandTypeList<ExCommand<depth::map::ListDepthsArguments>, Command>;
};
template<> struct CommandTraits<depth::map::SetDepthsCommand> {
    static constexpr auto attributes = std::tuple{EffectApply};
    using BaseClasses = CommandTypeList<ExCommand<depth::map::SetDepthsArguments>, Command>;
};
template<> struct CommandTraits<depth::map::ClearDepthsCommand> {
    static constexpr auto attributes = std::tuple{EffectApply};
    using BaseClasses = CommandTypeList<ExCommand<depth::map::ClearDepthsArguments>, Command>;
};
template<> struct CommandTraits<depth::map::ListDepthOpsCommand> {
    static constexpr auto attributes = std::tuple{EffectApply};
    using BaseClasses = CommandTypeList<ExCommand<depth::map::ListDepthOpsArguments>, Command>;
};
template<> struct CommandTraits<depth::map::SetDepthOpsCommand> {
    static constexpr auto attributes = std::tuple{EffectApply};
    using BaseClasses = CommandTypeList<ExCommand<depth::map::SetDepthOpsArguments>, Command>;
};
template<> struct CommandTraits<depth::map::AddDepthOpCommand> {
    static constexpr auto attributes = std::tuple{EffectApply};
    using BaseClasses = CommandTypeList<ExCommand<depth::map::AddDepthOpArguments>, Command>;
};
template<> struct CommandTraits<depth::map::UpdateDepthOpCommand> {
    static constexpr auto attributes = std::tuple{EffectApply};
    using BaseClasses = CommandTypeList<ExCommand<depth::map::UpdateDepthOpArguments>, Command>;
};
template<> struct CommandTraits<depth::map::RemoveDepthOpCommand> {
    static constexpr auto attributes = std::tuple{EffectApply};
    using BaseClasses = CommandTypeList<ExCommand<depth::map::RemoveDepthOpArguments>, Command>;
};
template<> struct CommandTraits<depth::map::MoveDepthOpCommand> {
    static constexpr auto attributes = std::tuple{EffectApply};
    using BaseClasses = CommandTypeList<ExCommand<depth::map::MoveDepthOpArguments>, Command>;
};
template<> struct CommandTraits<depth::map::ClearDepthOpsCommand> {
    static constexpr auto attributes = std::tuple{EffectApply};
    using BaseClasses = CommandTypeList<ExCommand<depth::map::ClearDepthOpsArguments>, Command>;
};
template<> struct CommandTraits<depth::map::ApplyDepthOpsCommand> {
    static constexpr auto attributes = std::tuple{EffectApply};
    using BaseClasses = CommandTypeList<ExCommand<depth::map::ApplyDepthOpsArguments>, Command>;
};
template<> struct CommandTraits<depth::map::ImportPSDDepthsCommand> {
    static constexpr auto attributes = std::tuple{EffectApply};
    using BaseClasses = CommandTypeList<ExCommand<depth::map::ImportPSDDepthsArguments>, Command>;
};

#define NG_DEPTH_MAP_CONSTRUCTOR_PROGRAM(Class, Storage) \
template<> struct SourceCommandConstructor<depth::map::Class> { \
    template<class... Args> struct Program { \
        using C = depth::map::Class; \
        static consteval bool supported() { \
            return sizeof...(Args) == 0 && requires { \
                { C::sourceConstructorTexts() } -> std::same_as<depth::map::DepthMapConstructorText>; \
            } && SourceExCommandConstructorProgram<depth::map::Storage>::pairSupported(); \
        } \
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args) \
            requires(supported()) { \
            return std::allocate_shared<C>(allocator, std::forward<Args>(args)...); \
        } \
    }; \
};
NG_DEPTH_MAP_CONSTRUCTOR_PROGRAM(ListDepthsCommand, ListDepthsArguments)
NG_DEPTH_MAP_CONSTRUCTOR_PROGRAM(SetDepthsCommand, SetDepthsArguments)
NG_DEPTH_MAP_CONSTRUCTOR_PROGRAM(ClearDepthsCommand, ClearDepthsArguments)
NG_DEPTH_MAP_CONSTRUCTOR_PROGRAM(ListDepthOpsCommand, ListDepthOpsArguments)
NG_DEPTH_MAP_CONSTRUCTOR_PROGRAM(SetDepthOpsCommand, SetDepthOpsArguments)
NG_DEPTH_MAP_CONSTRUCTOR_PROGRAM(AddDepthOpCommand, AddDepthOpArguments)
NG_DEPTH_MAP_CONSTRUCTOR_PROGRAM(UpdateDepthOpCommand, UpdateDepthOpArguments)
NG_DEPTH_MAP_CONSTRUCTOR_PROGRAM(RemoveDepthOpCommand, RemoveDepthOpArguments)
NG_DEPTH_MAP_CONSTRUCTOR_PROGRAM(MoveDepthOpCommand, MoveDepthOpArguments)
NG_DEPTH_MAP_CONSTRUCTOR_PROGRAM(ClearDepthOpsCommand, ClearDepthOpsArguments)
NG_DEPTH_MAP_CONSTRUCTOR_PROGRAM(ApplyDepthOpsCommand, ApplyDepthOpsArguments)
#undef NG_DEPTH_MAP_CONSTRUCTOR_PROGRAM

template<> struct SourceCommandConstructor<depth::map::ImportPSDDepthsCommand> {
    template<class... Args> struct Program {
        using C = depth::map::ImportPSDDepthsCommand;
        using String = compat::RetainedString;
        using Parameters = std::tuple<String, bool, float, float, float, String, String,
            std::int32_t, float, String, bool>;
        template<std::size_t... I> static consteval bool parametersSupported(std::index_sequence<I...>) {
            return detail::declaredSourceParametersSupported<
                std::tuple<std::tuple_element_t<I, Parameters>...>, Args...>();
        }
        static consteval bool supported() {
            if constexpr (sizeof...(Args) == 0 || sizeof...(Args) > 11) return false;
            else return parametersSupported(std::index_sequence_for<Args...>{}) && requires {
                { C::sourceConstructorTexts() } -> std::same_as<depth::map::DepthMapConstructorText>;
            } && SourceExCommandConstructorProgram<depth::map::ImportPSDDepthsArguments>::
                preferredSupported<String&, bool&, float&, float&, float&, String&, String&,
                    std::int32_t&, float&, String&, bool&>();
        }
        static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator,
            Args&&... args) requires(supported()) {
            return [&]<std::size_t... I>(std::index_sequence<I...>) {
                using Targets = std::tuple<std::tuple_element_t<I, Parameters>...>;
                return detail::constructDeclaredSourceCommand<C, Targets>(allocator, std::forward<Args>(args)...);
            }(std::index_sequence_for<Args...>{});
        }
    };
};
}

namespace nijigenerate::commands::depth::map {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct ImportArguments {
    static auto get() {
        return std::tuple{compat::RetainedString::emptyLiteral(), false, -1.0f, 1.0f, 1.0f,
            compat::RetainedString::fromStaticLiteral("Gaussian3x3"),
            compat::RetainedString::fromStaticLiteral("AverageRGB"), 3, 0.01f,
            compat::RetainedString::fromStaticLiteral("KeepExisting"), true};
    }
};
template<auto Id, class C>
using Required = ::nijigenerate::commands::detail::DefaultCommandRegistration<Id, C>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Required<DepthMapCommand::ListDepths, ListDepthsCommand>,
    Required<DepthMapCommand::SetDepths, SetDepthsCommand>,
    Required<DepthMapCommand::ClearDepths, ClearDepthsCommand>,
    Required<DepthMapCommand::ListDepthOps, ListDepthOpsCommand>,
    Required<DepthMapCommand::SetDepthOps, SetDepthOpsCommand>,
    Required<DepthMapCommand::AddDepthOp, AddDepthOpCommand>,
    Required<DepthMapCommand::UpdateDepthOp, UpdateDepthOpCommand>,
    Required<DepthMapCommand::RemoveDepthOp, RemoveDepthOpCommand>,
    Required<DepthMapCommand::MoveDepthOp, MoveDepthOpCommand>,
    Required<DepthMapCommand::ClearDepthOps, ClearDepthOpsCommand>,
    Required<DepthMapCommand::ApplyDepthOps, ApplyDepthOpsCommand>,
    ::nijigenerate::commands::detail::ConstructedCommandRegistration<
        DepthMapCommand::ImportPSDDepths, ImportPSDDepthsCommand, ImportArguments>>;
}
template<class T> requires (std::same_as<T, DepthMapCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitDepthMapCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<depth::map::DepthMapCommand> {
    static consteval bool supported() {
        return depth::map::initialization::Program::supported<depth::map::initialization::Table>();
    }
    template<class ActualKey = depth::map::DepthMapCommand>
        requires (std::same_as<ActualKey, depth::map::DepthMapCommand> && supported())
    static void invoke() { depth::map::ngInitCommands<ActualKey>(); }
};
}

namespace nicxlive::core::d_runtime {
// These are the twelve concrete declarations in the original map module.
#define NG_DEPTH_MAP_COMMAND_ORIGIN(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::commands::depth::map::Type> { \
        inline static constexpr char className[] = "nijigenerate.commands.depth.map." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.depth.map." #Type ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::depth::map::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.depth.map." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.depth.map." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    }
NG_DEPTH_MAP_COMMAND_ORIGIN(ListDepthsCommand);
NG_DEPTH_MAP_COMMAND_ORIGIN(SetDepthsCommand);
NG_DEPTH_MAP_COMMAND_ORIGIN(ClearDepthsCommand);
NG_DEPTH_MAP_COMMAND_ORIGIN(ListDepthOpsCommand);
NG_DEPTH_MAP_COMMAND_ORIGIN(SetDepthOpsCommand);
NG_DEPTH_MAP_COMMAND_ORIGIN(AddDepthOpCommand);
NG_DEPTH_MAP_COMMAND_ORIGIN(UpdateDepthOpCommand);
NG_DEPTH_MAP_COMMAND_ORIGIN(RemoveDepthOpCommand);
NG_DEPTH_MAP_COMMAND_ORIGIN(MoveDepthOpCommand);
NG_DEPTH_MAP_COMMAND_ORIGIN(ClearDepthOpsCommand);
NG_DEPTH_MAP_COMMAND_ORIGIN(ApplyDepthOpsCommand);
NG_DEPTH_MAP_COMMAND_ORIGIN(ImportPSDDepthsCommand);
#undef NG_DEPTH_MAP_COMMAND_ORIGIN
}

