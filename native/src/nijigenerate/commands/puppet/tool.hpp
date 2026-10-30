#pragma once
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include "tool_handlers.hpp"
#include "tool_types.hpp"
#include "tool_table.hpp"
#include <nijigenerate/commands/command.hpp>
#include <array>
#include <concepts>
#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
namespace nijigenerate::commands::puppet::tool {
using String=compat::RetainedStringSlice;
struct ImportSessionDataArguments {
    String path{};
};
struct ExecuteAutoRigArguments {
    String options{};
    String context{};
};
struct GetAutoRigStatusArguments {
    String runId{};
};
struct GetAutoRigMemoryStatusArguments {
    String runId{};
};
struct DeleteAutoRigSessionArguments {
    String runId{};
};
struct ResumeAutoRigArguments {
    String runId{};
    String stepId{};
};
}
namespace nijigenerate::commands {
template<> struct CommandFields<puppet::tool::ImportSessionDataArguments> {
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&puppet::tool::ImportSessionDataArguments::path,"path","file path of INP file.","string">{}};
};
template<> struct CommandFields<puppet::tool::ExecuteAutoRigArguments> {
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&puppet::tool::ExecuteAutoRigArguments::options,"options","AutoRig options JSON; defaults to an empty object.","string">{},
        FieldDescriptor<&puppet::tool::ExecuteAutoRigArguments::context,"context","Named session values: {name: {kind: Path|Json|Blob|FileName, value: ...}}.","string">{}};
};
template<> struct CommandFields<puppet::tool::GetAutoRigStatusArguments> {
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&puppet::tool::GetAutoRigStatusArguments::runId,"runId","AutoRig run UUID.","string">{}};
};
template<> struct CommandFields<puppet::tool::GetAutoRigMemoryStatusArguments> {
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&puppet::tool::GetAutoRigMemoryStatusArguments::runId,"runId","AutoRig run UUID; empty measures all panel sessions.","string">{}};
};
template<> struct CommandFields<puppet::tool::DeleteAutoRigSessionArguments> {
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&puppet::tool::DeleteAutoRigSessionArguments::runId,"runId","Stopped AutoRig session UUID.","string">{}};
};
template<> struct CommandFields<puppet::tool::ResumeAutoRigArguments> {
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&puppet::tool::ResumeAutoRigArguments::runId,"runId","Saved AutoRig run UUID.","string">{},
        FieldDescriptor<&puppet::tool::ResumeAutoRigArguments::stepId,"stepId","Optional workflow step to retry; empty resumes the workflow.","string">{}};
};
}
namespace nijigenerate::commands::puppet::tool {
class ShowImportSessionDataDialogCommand : public ExCommand<>,
    public std::enable_shared_from_this<ShowImportSessionDataDialogCommand> {
    explicit ShowImportSessionDataDialogCommand(std::array<compat::RetainedString, 2> presentation);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowImportSessionDataDialogCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    ShowImportSessionDataDialogCommand(const ShowImportSessionDataDialogCommand&) = delete;
    ShowImportSessionDataDialogCommand& operator=(const ShowImportSessionDataDialogCommand&) = delete;
    ShowImportSessionDataDialogCommand(ShowImportSessionDataDialogCommand&&) = delete;
    ShowImportSessionDataDialogCommand& operator=(ShowImportSessionDataDialogCommand&&) = delete;
    ShowImportSessionDataDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class PremultTextureCommand : public ExCommand<>, public std::enable_shared_from_this<PremultTextureCommand> {
    explicit PremultTextureCommand(std::array<compat::RetainedString, 2> presentation);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<PremultTextureCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    PremultTextureCommand(const PremultTextureCommand&) = delete;
    PremultTextureCommand& operator=(const PremultTextureCommand&) = delete;
    PremultTextureCommand(PremultTextureCommand&&) = delete;
    PremultTextureCommand& operator=(PremultTextureCommand&&) = delete;
    PremultTextureCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class RebleedTextureCommand : public ExCommand<>, public std::enable_shared_from_this<RebleedTextureCommand> {
    explicit RebleedTextureCommand(std::array<compat::RetainedString, 2> presentation);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<RebleedTextureCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    RebleedTextureCommand(const RebleedTextureCommand&) = delete;
    RebleedTextureCommand& operator=(const RebleedTextureCommand&) = delete;
    RebleedTextureCommand(RebleedTextureCommand&&) = delete;
    RebleedTextureCommand& operator=(RebleedTextureCommand&&) = delete;
    RebleedTextureCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class RegenerateMipmapsCommand : public ExCommand<>, public std::enable_shared_from_this<RegenerateMipmapsCommand> {
    explicit RegenerateMipmapsCommand(std::array<compat::RetainedString, 2> presentation);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<RegenerateMipmapsCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    RegenerateMipmapsCommand(const RegenerateMipmapsCommand&) = delete;
    RegenerateMipmapsCommand& operator=(const RegenerateMipmapsCommand&) = delete;
    RegenerateMipmapsCommand(RegenerateMipmapsCommand&&) = delete;
    RegenerateMipmapsCommand& operator=(RegenerateMipmapsCommand&&) = delete;
    RegenerateMipmapsCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class GenerateFakeLayerNameCommand : public ExCommand<>,
    public std::enable_shared_from_this<GenerateFakeLayerNameCommand> {
    explicit GenerateFakeLayerNameCommand(std::array<compat::RetainedString, 2> presentation);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<GenerateFakeLayerNameCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    GenerateFakeLayerNameCommand(const GenerateFakeLayerNameCommand&) = delete;
    GenerateFakeLayerNameCommand& operator=(const GenerateFakeLayerNameCommand&) = delete;
    GenerateFakeLayerNameCommand(GenerateFakeLayerNameCommand&&) = delete;
    GenerateFakeLayerNameCommand& operator=(GenerateFakeLayerNameCommand&&) = delete;
    GenerateFakeLayerNameCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AttemptRepairPuppetCommand : public ExCommand<>, public std::enable_shared_from_this<AttemptRepairPuppetCommand> {
    explicit AttemptRepairPuppetCommand(std::array<compat::RetainedString, 2> presentation);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<AttemptRepairPuppetCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    AttemptRepairPuppetCommand(const AttemptRepairPuppetCommand&) = delete;
    AttemptRepairPuppetCommand& operator=(const AttemptRepairPuppetCommand&) = delete;
    AttemptRepairPuppetCommand(AttemptRepairPuppetCommand&&) = delete;
    AttemptRepairPuppetCommand& operator=(AttemptRepairPuppetCommand&&) = delete;
    AttemptRepairPuppetCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class RegenerateNodeIDsCommand : public ExCommand<>, public std::enable_shared_from_this<RegenerateNodeIDsCommand> {
    explicit RegenerateNodeIDsCommand(std::array<compat::RetainedString, 2> presentation);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<RegenerateNodeIDsCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    RegenerateNodeIDsCommand(const RegenerateNodeIDsCommand&) = delete;
    RegenerateNodeIDsCommand& operator=(const RegenerateNodeIDsCommand&) = delete;
    RegenerateNodeIDsCommand(RegenerateNodeIDsCommand&&) = delete;
    RegenerateNodeIDsCommand& operator=(RegenerateNodeIDsCommand&&) = delete;
    RegenerateNodeIDsCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class ModelEditModeCommand : public ExCommand<>, public std::enable_shared_from_this<ModelEditModeCommand> {
    explicit ModelEditModeCommand(std::array<compat::RetainedString, 2> presentation);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ModelEditModeCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    ModelEditModeCommand(const ModelEditModeCommand&) = delete;
    ModelEditModeCommand& operator=(const ModelEditModeCommand&) = delete;
    ModelEditModeCommand(ModelEditModeCommand&&) = delete;
    ModelEditModeCommand& operator=(ModelEditModeCommand&&) = delete;
    ModelEditModeCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class AnimEditModeCommand : public ExCommand<>, public std::enable_shared_from_this<AnimEditModeCommand> {
    explicit AnimEditModeCommand(std::array<compat::RetainedString, 2> presentation);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<AnimEditModeCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    AnimEditModeCommand(const AnimEditModeCommand&) = delete;
    AnimEditModeCommand& operator=(const AnimEditModeCommand&) = delete;
    AnimEditModeCommand(AnimEditModeCommand&&) = delete;
    AnimEditModeCommand& operator=(AnimEditModeCommand&&) = delete;
    AnimEditModeCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class ImportSessionDataCommand : public ExCommand<ImportSessionDataArguments>,
    public std::enable_shared_from_this<ImportSessionDataCommand> {
    ImportSessionDataCommand(std::array<compat::RetainedString, 2> presentation, String path);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ImportSessionDataCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    ImportSessionDataCommand(const ImportSessionDataCommand&) = delete;
    ImportSessionDataCommand& operator=(const ImportSessionDataCommand&) = delete;
    ImportSessionDataCommand(ImportSessionDataCommand&&) = delete;
    ImportSessionDataCommand& operator=(ImportSessionDataCommand&&) = delete;
    ImportSessionDataCommand(String path);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class ExecuteAutoRigCommand : public ExCommand<ExecuteAutoRigArguments>,
    public std::enable_shared_from_this<ExecuteAutoRigCommand> {
    ExecuteAutoRigCommand(std::array<compat::RetainedString, 2> presentation, String options, String context);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ExecuteAutoRigCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    ExecuteAutoRigCommand(const ExecuteAutoRigCommand&) = delete;
    ExecuteAutoRigCommand& operator=(const ExecuteAutoRigCommand&) = delete;
    ExecuteAutoRigCommand(ExecuteAutoRigCommand&&) = delete;
    ExecuteAutoRigCommand& operator=(ExecuteAutoRigCommand&&) = delete;
    ExecuteAutoRigCommand(String options=compat::RetainedString::fromStaticLiteral("{}"), String context=compat::RetainedString::fromStaticLiteral("{}"));
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class GetAutoRigStatusCommand : public ExCommand<GetAutoRigStatusArguments>,
    public std::enable_shared_from_this<GetAutoRigStatusCommand> {
    GetAutoRigStatusCommand(std::array<compat::RetainedString, 2> presentation, String runId);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<GetAutoRigStatusCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    GetAutoRigStatusCommand(const GetAutoRigStatusCommand&) = delete;
    GetAutoRigStatusCommand& operator=(const GetAutoRigStatusCommand&) = delete;
    GetAutoRigStatusCommand(GetAutoRigStatusCommand&&) = delete;
    GetAutoRigStatusCommand& operator=(GetAutoRigStatusCommand&&) = delete;
    GetAutoRigStatusCommand(String runId=compat::RetainedString::fromStaticLiteral(""));
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class GetAutoRigMemoryStatusCommand : public ExCommand<GetAutoRigMemoryStatusArguments>,
    public std::enable_shared_from_this<GetAutoRigMemoryStatusCommand> {
    GetAutoRigMemoryStatusCommand(std::array<compat::RetainedString, 2> presentation, String runId);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<GetAutoRigMemoryStatusCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    GetAutoRigMemoryStatusCommand(const GetAutoRigMemoryStatusCommand&) = delete;
    GetAutoRigMemoryStatusCommand& operator=(const GetAutoRigMemoryStatusCommand&) = delete;
    GetAutoRigMemoryStatusCommand(GetAutoRigMemoryStatusCommand&&) = delete;
    GetAutoRigMemoryStatusCommand& operator=(GetAutoRigMemoryStatusCommand&&) = delete;
    GetAutoRigMemoryStatusCommand(String runId=compat::RetainedString::fromStaticLiteral(""));
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class DeleteAutoRigSessionCommand : public ExCommand<DeleteAutoRigSessionArguments>,
    public std::enable_shared_from_this<DeleteAutoRigSessionCommand> {
    DeleteAutoRigSessionCommand(std::array<compat::RetainedString, 2> presentation, String runId);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<DeleteAutoRigSessionCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    DeleteAutoRigSessionCommand(const DeleteAutoRigSessionCommand&) = delete;
    DeleteAutoRigSessionCommand& operator=(const DeleteAutoRigSessionCommand&) = delete;
    DeleteAutoRigSessionCommand(DeleteAutoRigSessionCommand&&) = delete;
    DeleteAutoRigSessionCommand& operator=(DeleteAutoRigSessionCommand&&) = delete;
    DeleteAutoRigSessionCommand(String runId=compat::RetainedString::fromStaticLiteral(""));
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
class ResumeAutoRigCommand : public ExCommand<ResumeAutoRigArguments>,
    public std::enable_shared_from_this<ResumeAutoRigCommand> {
    ResumeAutoRigCommand(std::array<compat::RetainedString, 2> presentation, String runId, String stepId);
    static std::array<compat::RetainedString, 2> sourceConstructorPresentation();
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ResumeAutoRigCommand>;
protected:
    void prepareSourceClassInfo() override;
public:
    ResumeAutoRigCommand(const ResumeAutoRigCommand&) = delete;
    ResumeAutoRigCommand& operator=(const ResumeAutoRigCommand&) = delete;
    ResumeAutoRigCommand(ResumeAutoRigCommand&&) = delete;
    ResumeAutoRigCommand& operator=(ResumeAutoRigCommand&&) = delete;
    ResumeAutoRigCommand(String runId=compat::RetainedString::fromStaticLiteral(""), String stepId=compat::RetainedString::fromStaticLiteral(""));
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context>) override;
};
void ngInitToolCommands();
}

namespace nicxlive::core::d_runtime {
#define NG_TOOL_COMMAND_SOURCE_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::commands::puppet::tool::Type> { \
        inline static constexpr char className[] = "nijigenerate.commands.puppet.tool." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.puppet.tool." #Type ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::puppet::tool::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.puppet.tool." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.puppet.tool." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_TOOL_COMMAND_SOURCE_NAMES(ShowImportSessionDataDialogCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(ImportSessionDataCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(PremultTextureCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(RebleedTextureCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(RegenerateMipmapsCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(GenerateFakeLayerNameCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(AttemptRepairPuppetCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(RegenerateNodeIDsCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(ModelEditModeCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(AnimEditModeCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(ExecuteAutoRigCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(GetAutoRigStatusCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(GetAutoRigMemoryStatusCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(DeleteAutoRigSessionCommand)
NG_TOOL_COMMAND_SOURCE_NAMES(ResumeAutoRigCommand)
#undef NG_TOOL_COMMAND_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime
namespace nijigenerate::commands {
template<> struct CommandTraits<puppet::tool::ShowImportSessionDataDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden, GuiDialog};
    using BaseClasses=CommandTypeList<ExCommand<>,Command>;
};
template<> struct CommandTraits<puppet::tool::PremultTextureCommand> {
    static constexpr auto attributes=std::tuple{EffectTextureRegenerate};
    using BaseClasses=CommandTypeList<ExCommand<>,Command>;
};
template<> struct CommandTraits<puppet::tool::RebleedTextureCommand> {
    static constexpr auto attributes=std::tuple{EffectTextureRegenerate};
    using BaseClasses=CommandTypeList<ExCommand<>,Command>;
};
template<> struct CommandTraits<puppet::tool::RegenerateMipmapsCommand> {
    static constexpr auto attributes=std::tuple{EffectTextureRegenerate};
    using BaseClasses=CommandTypeList<ExCommand<>,Command>;
};
template<> struct CommandTraits<puppet::tool::GenerateFakeLayerNameCommand> {
    static constexpr auto attributes=std::tuple{EffectStructuralEdit};
    using BaseClasses=CommandTypeList<ExCommand<>,Command>;
};
template<> struct CommandTraits<puppet::tool::AttemptRepairPuppetCommand> {
    static constexpr auto attributes=std::tuple{EffectRepair};
    using BaseClasses=CommandTypeList<ExCommand<>,Command>;
};
template<> struct CommandTraits<puppet::tool::RegenerateNodeIDsCommand> {
    static constexpr auto attributes=std::tuple{EffectRepair};
    using BaseClasses=CommandTypeList<ExCommand<>,Command>;
};
template<> struct CommandTraits<puppet::tool::ModelEditModeCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<ExCommand<>,Command>;
};
template<> struct CommandTraits<puppet::tool::AnimEditModeCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<ExCommand<>,Command>;
};
template<> struct CommandTraits<puppet::tool::ImportSessionDataCommand> {
    static constexpr auto attributes=std::tuple{EffectImport};
    using BaseClasses=CommandTypeList<ExCommand<puppet::tool::ImportSessionDataArguments>,Command>;
};
template<> struct CommandTraits<puppet::tool::ExecuteAutoRigCommand> {
    static constexpr auto attributes=std::tuple{EffectStructuralEdit};
    using BaseClasses=CommandTypeList<ExCommand<puppet::tool::ExecuteAutoRigArguments>,Command>;
};
template<> struct CommandTraits<puppet::tool::GetAutoRigStatusCommand> {
    static constexpr auto attributes=std::tuple{PresentationOnly{}};
    using BaseClasses=CommandTypeList<ExCommand<puppet::tool::GetAutoRigStatusArguments>,Command>;
};
template<> struct CommandTraits<puppet::tool::GetAutoRigMemoryStatusCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<ExCommand<puppet::tool::GetAutoRigMemoryStatusArguments>,Command>;
};
template<> struct CommandTraits<puppet::tool::DeleteAutoRigSessionCommand> {
    static constexpr auto attributes=std::tuple{};
    using BaseClasses=CommandTypeList<ExCommand<puppet::tool::DeleteAutoRigSessionArguments>,Command>;
};
template<> struct CommandTraits<puppet::tool::ResumeAutoRigCommand> {
    static constexpr auto attributes=std::tuple{EffectStructuralEdit};
    using BaseClasses=CommandTypeList<ExCommand<puppet::tool::ResumeAutoRigArguments>,Command>;
};
}

namespace nijigenerate::commands::puppet::tool::constructor_detail {

template<class> inline constexpr bool missingStringParameter = false;

template<class Argument> consteval bool parameterSupported() {
    using Value = std::remove_cvref_t<Argument>;
    if constexpr (std::same_as<Value, std::nullptr_t> ||
        ::nijigenerate::commands::detail::commandStringDescriptor<Value>)
        return ::nijigenerate::commands::detail::declaredSourceParametersSupported<std::tuple<String>, Argument>();
    else if constexpr (std::is_arithmetic_v<Value> || std::is_enum_v<Value>) return false;
    else static_assert(missingStringParameter<Argument>,
        "Missing defining Source string constructor argument program");
}

template<std::size_t> using StringValue = String;

template<class C, class... Args>
std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args) {
    return [&]<std::size_t... Index>(std::index_sequence<Index...>) {
        return ::nijigenerate::commands::detail::constructDeclaredSourceCommand<C,
            std::tuple<StringValue<Index>...>>(
                allocator, std::forward<Args>(args)...);
    }(std::index_sequence_for<Args...>{});
}

template<std::size_t> using StringReference = String&;

template<class Storage> consteval bool baseSupported() {
    using Base = SourceExCommandConstructorProgram<Storage>;
    constexpr auto count = Base::argumentCount;
    if constexpr (count == 0) return Base::pairSupported();
    else return []<std::size_t... Index>(std::index_sequence<Index...>) {
        return Base::template preferredSupported<StringReference<Index>...>();
    }(std::make_index_sequence<count>{});
}

} // namespace nijigenerate::commands::puppet::tool::constructor_detail

namespace nijigenerate::commands {

// These are defining records for the actual declarations above, rather than a
// fallback for an arbitrary native class. Every factory uses that same C.
#define NG_TOOL_CONSTRUCTOR_PROGRAM(Type, Storage, Minimum, Maximum) \
    template<> struct SourceCommandConstructor<puppet::tool::Type> { \
        template<class... Args> struct Program { \
            using C = puppet::tool::Type; \
            static consteval bool supported() { \
                if constexpr (sizeof...(Args) < Minimum || sizeof...(Args) > Maximum) return false; \
                else return (puppet::tool::constructor_detail::parameterSupported<Args&&>() && ...) && \
                    requires { \
                        { C::sourceConstructorPresentation() } -> \
                            std::same_as<std::array<compat::RetainedString, 2>>; \
                    } && puppet::tool::constructor_detail::baseSupported<Storage>(); \
            } \
            static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, \
                Args&&... args) requires(supported()) { \
                return puppet::tool::constructor_detail::construct<C>(allocator, std::forward<Args>(args)...); \
            } \
        }; \
    };

NG_TOOL_CONSTRUCTOR_PROGRAM(ShowImportSessionDataDialogCommand, CommandNoArguments, 0, 0)
NG_TOOL_CONSTRUCTOR_PROGRAM(PremultTextureCommand, CommandNoArguments, 0, 0)
NG_TOOL_CONSTRUCTOR_PROGRAM(RebleedTextureCommand, CommandNoArguments, 0, 0)
NG_TOOL_CONSTRUCTOR_PROGRAM(RegenerateMipmapsCommand, CommandNoArguments, 0, 0)
NG_TOOL_CONSTRUCTOR_PROGRAM(GenerateFakeLayerNameCommand, CommandNoArguments, 0, 0)
NG_TOOL_CONSTRUCTOR_PROGRAM(AttemptRepairPuppetCommand, CommandNoArguments, 0, 0)
NG_TOOL_CONSTRUCTOR_PROGRAM(RegenerateNodeIDsCommand, CommandNoArguments, 0, 0)
NG_TOOL_CONSTRUCTOR_PROGRAM(ModelEditModeCommand, CommandNoArguments, 0, 0)
NG_TOOL_CONSTRUCTOR_PROGRAM(AnimEditModeCommand, CommandNoArguments, 0, 0)
NG_TOOL_CONSTRUCTOR_PROGRAM(ImportSessionDataCommand, puppet::tool::ImportSessionDataArguments, 1, 1)
NG_TOOL_CONSTRUCTOR_PROGRAM(ExecuteAutoRigCommand, puppet::tool::ExecuteAutoRigArguments, 0, 2)
NG_TOOL_CONSTRUCTOR_PROGRAM(GetAutoRigStatusCommand, puppet::tool::GetAutoRigStatusArguments, 0, 1)
NG_TOOL_CONSTRUCTOR_PROGRAM(GetAutoRigMemoryStatusCommand, puppet::tool::GetAutoRigMemoryStatusArguments, 0, 1)
NG_TOOL_CONSTRUCTOR_PROGRAM(DeleteAutoRigSessionCommand, puppet::tool::DeleteAutoRigSessionArguments, 0, 1)
NG_TOOL_CONSTRUCTOR_PROGRAM(ResumeAutoRigCommand, puppet::tool::ResumeAutoRigArguments, 0, 2)

#undef NG_TOOL_CONSTRUCTOR_PROGRAM

} // namespace nijigenerate::commands


namespace nijigenerate::commands::puppet::tool {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct ImportArguments {
    static auto get() { return std::tuple{String(compat::RetainedString::emptyLiteral())}; }
};
template<auto Id, class C>
using Optional = ::nijigenerate::commands::detail::OptionalCommandRegistration<Id, C>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Optional<ToolCommand::ShowImportSessionDataDialog, ShowImportSessionDataDialogCommand>,
    Optional<ToolCommand::ImportSessionData, ImportSessionDataCommand>,
    Optional<ToolCommand::PremultTexture, PremultTextureCommand>,
    Optional<ToolCommand::RebleedTexture, RebleedTextureCommand>,
    Optional<ToolCommand::RegenerateMipmaps, RegenerateMipmapsCommand>,
    Optional<ToolCommand::GenerateFakeLayerName, GenerateFakeLayerNameCommand>,
    Optional<ToolCommand::AttemptRepairPuppet, AttemptRepairPuppetCommand>,
    Optional<ToolCommand::RegenerateNodeIDs, RegenerateNodeIDsCommand>,
    Optional<ToolCommand::ModelEditMode, ModelEditModeCommand>,
    Optional<ToolCommand::AnimEditMode, AnimEditModeCommand>,
    Optional<ToolCommand::ExecuteAutoRig, ExecuteAutoRigCommand>,
    Optional<ToolCommand::GetAutoRigStatus, GetAutoRigStatusCommand>,
    Optional<ToolCommand::GetAutoRigMemoryStatus, GetAutoRigMemoryStatusCommand>,
    Optional<ToolCommand::DeleteAutoRigSession, DeleteAutoRigSessionCommand>,
    Optional<ToolCommand::ResumeAutoRig, ResumeAutoRigCommand>,
    ::nijigenerate::commands::detail::ConstructedCommandRegistration<
        ToolCommand::ImportSessionData, ImportSessionDataCommand, ImportArguments>>;
}
template<class T> requires (std::same_as<T, ToolCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitToolCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<puppet::tool::ToolCommand> {
    static consteval bool supported() {
        return puppet::tool::initialization::Program::supported<puppet::tool::initialization::Table>();
    }
    template<class ActualKey = puppet::tool::ToolCommand>
        requires (std::same_as<ActualKey, puppet::tool::ToolCommand> && supported())
    static void invoke() { puppet::tool::ngInitCommands<ActualKey>(); }
};
}
