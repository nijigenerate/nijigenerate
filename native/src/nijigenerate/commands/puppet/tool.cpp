#include <nijigenerate/commands/puppet/tool.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/concrete_catalog.hpp>
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/tasks.hpp>
#include <nijigenerate/core/actionstack.hpp>
#include <nijigenerate/io/package.hpp>
#include <nijigenerate/io/project_reader.hpp>
#include <nijigenerate/ext/nodes/expart.hpp>
#include <nijigenerate/utils/repair.hpp>
#include <nijigenerate/autorig/detail/json_first_pass_reader.hpp>
#include <nijigenerate/autorig/framework.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <nijigenerate/project.hpp>
#include <render/shared_deform_buffer.hpp>
#include <compat/object_metadata.hpp>
#include <utility>
#include <type_traits>
#include <memory>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <new>
#include <source_location>
#include <stdexcept>

namespace nijigenerate::commands::puppet::tool {
namespace {

// These source __gshared fields are process-shared, unlike the command registry.
// No atomic/mutex protocol or thread-safe callable invocation is invented here.
AutoRigExecuteHandler autoRigExecute;
AutoRigStatusHandler autoRigStatus;
AutoRigResumeHandler autoRigResume;
AutoRigRemoveHandler autoRigRemove;

}

void ngSetAutoRigCommandHandlers(AutoRigExecuteHandler execute, AutoRigStatusHandler status,
        AutoRigResumeHandler resume, AutoRigRemoveHandler remove) {
    autoRigExecute = execute;
    autoRigStatus = status;
    autoRigResume = resume;
    autoRigRemove = remove;
}

using Json=core::settings::SettingsJsonValue;
using Object=core::settings::SettingsJsonObject;
using core::translation::ngTranslate;
using core::translation::ngTranslateRetained;
using ContextRef=std::shared_ptr<Context>;
using Result=std::shared_ptr<CommandResult>;
namespace {
// Source explicit new allocates before evaluating its constructor arguments.
// The allocator keeps no producer state; allocate_shared passes it only to construct.
template<class T> struct NewJsonResultAllocator : nicxlive::core::d_runtime::SourceAllocator<T> {
    template<class U> struct rebind { using other = NewJsonResultAllocator<U>; };
    NewJsonResultAllocator() noexcept = default;
    template<class U> NewJsonResultAllocator(const NewJsonResultAllocator<U>&) noexcept {}
    template<class U, class Payload> void construct(U* pointer, Payload&& payload) {
        auto* memory = const_cast<void*>(static_cast<const volatile void*>(pointer));
        ::new (memory) U(true, std::forward<Payload>(payload)());
    }
    template<class U> bool operator==(const NewJsonResultAllocator<U>&) const noexcept { return true; }
};
template<class Payload> Result newJsonResult(Payload&& payload) {
    using JsonResult = ExCommandResult<Json>;
    return std::allocate_shared<JsonResult>(NewJsonResultAllocator<JsonResult>{},
        std::forward<Payload>(payload));
}

void errorDialog(const char* text) {
    auto title=std::make_shared<const std::string>(ngTranslate("Error"));
    widgets::dialog::ngDialog(widgets::dialog::DialogTag::retained(title->c_str(),title),
        widgets::dialog::DialogText::owned(ngTranslate(text)));
}
Result runIdResult(String id) {
    return newJsonResult([&] {
        Object object;
        object[core::settings::SettingsString(compat::RetainedString::fromStaticLiteral("run_id"))]
            =Json(core::settings::SettingsString(id));
        return Json(object);
    });
}
}
#define NG_TOOL_PREPARE(Type, Base) \
    void Type::prepareSourceClassInfo() { \
        Base::prepareSourceClassInfo(); \
        ::nijigenerate::commands::detail::prepareCommandResultClass<Type>(); \
    }
NG_TOOL_PREPARE(ShowImportSessionDataDialogCommand, ExCommand<>)
NG_TOOL_PREPARE(PremultTextureCommand, ExCommand<>)
NG_TOOL_PREPARE(RebleedTextureCommand, ExCommand<>)
NG_TOOL_PREPARE(RegenerateMipmapsCommand, ExCommand<>)
NG_TOOL_PREPARE(GenerateFakeLayerNameCommand, ExCommand<>)
NG_TOOL_PREPARE(AttemptRepairPuppetCommand, ExCommand<>)
NG_TOOL_PREPARE(RegenerateNodeIDsCommand, ExCommand<>)
NG_TOOL_PREPARE(ModelEditModeCommand, ExCommand<>)
NG_TOOL_PREPARE(AnimEditModeCommand, ExCommand<>)
NG_TOOL_PREPARE(ImportSessionDataCommand, ExCommand<ImportSessionDataArguments>)
NG_TOOL_PREPARE(ExecuteAutoRigCommand, ExCommand<ExecuteAutoRigArguments>)
NG_TOOL_PREPARE(GetAutoRigStatusCommand, ExCommand<GetAutoRigStatusArguments>)
NG_TOOL_PREPARE(GetAutoRigMemoryStatusCommand, ExCommand<GetAutoRigMemoryStatusArguments>)
NG_TOOL_PREPARE(DeleteAutoRigSessionCommand, ExCommand<DeleteAutoRigSessionArguments>)
NG_TOOL_PREPARE(ResumeAutoRigCommand, ExCommand<ResumeAutoRigArguments>)
#undef NG_TOOL_PREPARE
std::array<compat::RetainedString, 2> ShowImportSessionDataDialogCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Import Inochi Session Data")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Shows \"Import Session Data\" dialog."))};
}
ShowImportSessionDataDialogCommand::ShowImportSessionDataDialogCommand()
    : ShowImportSessionDataDialogCommand(sourceConstructorPresentation()) {}
ShowImportSessionDataDialogCommand::ShowImportSessionDataDialogCommand(
        std::array<compat::RetainedString, 2> presentation)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1])) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowImportSessionDataDialogCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.ShowImportSessionDataDialogCommand"));
}
std::array<compat::RetainedString, 2> ImportSessionDataCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Import Inochi Session Data")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Import INP Session Data."))};
}
ImportSessionDataCommand::ImportSessionDataCommand(String path)
    : ImportSessionDataCommand(sourceConstructorPresentation(), path) {}
ImportSessionDataCommand::ImportSessionDataCommand(
        std::array<compat::RetainedString, 2> presentation, String path)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1]), path) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ImportSessionDataCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.ImportSessionDataCommand"));
}
std::array<compat::RetainedString, 2> PremultTextureCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Premultiply textures")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Premultiply texture."))};
}
PremultTextureCommand::PremultTextureCommand()
    : PremultTextureCommand(sourceConstructorPresentation()) {}
PremultTextureCommand::PremultTextureCommand(
        std::array<compat::RetainedString, 2> presentation)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1])) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(PremultTextureCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.PremultTextureCommand"));
}
std::array<compat::RetainedString, 2> RebleedTextureCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Bleed textures...")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Bleed texture."))};
}
RebleedTextureCommand::RebleedTextureCommand()
    : RebleedTextureCommand(sourceConstructorPresentation()) {}
RebleedTextureCommand::RebleedTextureCommand(
        std::array<compat::RetainedString, 2> presentation)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1])) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(RebleedTextureCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.RebleedTextureCommand"));
}
std::array<compat::RetainedString, 2> RegenerateMipmapsCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Generate Mipmaps...")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Generate mipmaps."))};
}
RegenerateMipmapsCommand::RegenerateMipmapsCommand()
    : RegenerateMipmapsCommand(sourceConstructorPresentation()) {}
RegenerateMipmapsCommand::RegenerateMipmapsCommand(
        std::array<compat::RetainedString, 2> presentation)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1])) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(RegenerateMipmapsCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.RegenerateMipmapsCommand"));
}
std::array<compat::RetainedString, 2> GenerateFakeLayerNameCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Generate fake layer name info...")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Generate fake layer name."))};
}
GenerateFakeLayerNameCommand::GenerateFakeLayerNameCommand()
    : GenerateFakeLayerNameCommand(sourceConstructorPresentation()) {}
GenerateFakeLayerNameCommand::GenerateFakeLayerNameCommand(
        std::array<compat::RetainedString, 2> presentation)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1])) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(GenerateFakeLayerNameCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.GenerateFakeLayerNameCommand"));
}
std::array<compat::RetainedString, 2> AttemptRepairPuppetCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Attempt full repair...")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Attempt full repair..."))};
}
AttemptRepairPuppetCommand::AttemptRepairPuppetCommand()
    : AttemptRepairPuppetCommand(sourceConstructorPresentation()) {}
AttemptRepairPuppetCommand::AttemptRepairPuppetCommand(
        std::array<compat::RetainedString, 2> presentation)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1])) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(AttemptRepairPuppetCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.AttemptRepairPuppetCommand"));
}
std::array<compat::RetainedString, 2> RegenerateNodeIDsCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Regenerate Node IDs")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Regenerate Node IDs"))};
}
RegenerateNodeIDsCommand::RegenerateNodeIDsCommand()
    : RegenerateNodeIDsCommand(sourceConstructorPresentation()) {}
RegenerateNodeIDsCommand::RegenerateNodeIDsCommand(
        std::array<compat::RetainedString, 2> presentation)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1])) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(RegenerateNodeIDsCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.RegenerateNodeIDsCommand"));
}
std::array<compat::RetainedString, 2> ModelEditModeCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Edit Puppet")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Switch to model-edit mode"))};
}
ModelEditModeCommand::ModelEditModeCommand()
    : ModelEditModeCommand(sourceConstructorPresentation()) {}
ModelEditModeCommand::ModelEditModeCommand(
        std::array<compat::RetainedString, 2> presentation)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1])) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ModelEditModeCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.ModelEditModeCommand"));
}
std::array<compat::RetainedString, 2> AnimEditModeCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Edit Animation")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Switch to anim-edit mode"))};
}
AnimEditModeCommand::AnimEditModeCommand()
    : AnimEditModeCommand(sourceConstructorPresentation()) {}
AnimEditModeCommand::AnimEditModeCommand(
        std::array<compat::RetainedString, 2> presentation)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1])) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(AnimEditModeCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.AnimEditModeCommand"));
}
std::array<compat::RetainedString, 2> ExecuteAutoRigCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Execute AutoRig")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Run AutoRig on the imported model."))};
}
ExecuteAutoRigCommand::ExecuteAutoRigCommand(String options,String context)
    : ExecuteAutoRigCommand(sourceConstructorPresentation(), options, context) {}
ExecuteAutoRigCommand::ExecuteAutoRigCommand(
        std::array<compat::RetainedString, 2> presentation,
        String options, String context)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1]), options, context) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExecuteAutoRigCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.ExecuteAutoRigCommand"));
}
std::array<compat::RetainedString, 2> GetAutoRigStatusCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("AutoRig Status")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Read AutoRig workflow progress."))};
}
GetAutoRigStatusCommand::GetAutoRigStatusCommand(String runId)
    : GetAutoRigStatusCommand(sourceConstructorPresentation(), runId) {}
GetAutoRigStatusCommand::GetAutoRigStatusCommand(
        std::array<compat::RetainedString, 2> presentation, String runId)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1]), runId) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(GetAutoRigStatusCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.GetAutoRigStatusCommand"));
}
std::array<compat::RetainedString, 2> GetAutoRigMemoryStatusCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("AutoRig memory status")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Read retained artifact sizes and native memory."))};
}
GetAutoRigMemoryStatusCommand::GetAutoRigMemoryStatusCommand(String runId)
    : GetAutoRigMemoryStatusCommand(sourceConstructorPresentation(), runId) {}
GetAutoRigMemoryStatusCommand::GetAutoRigMemoryStatusCommand(
        std::array<compat::RetainedString, 2> presentation,
        String runId)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1]), runId) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(GetAutoRigMemoryStatusCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.GetAutoRigMemoryStatusCommand"));
}
std::array<compat::RetainedString, 2> DeleteAutoRigSessionCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Delete AutoRig session")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral(
                "Delete the session artifacts while keeping the editor model."))};
}
DeleteAutoRigSessionCommand::DeleteAutoRigSessionCommand(String runId)
    : DeleteAutoRigSessionCommand(sourceConstructorPresentation(), runId) {}
DeleteAutoRigSessionCommand::DeleteAutoRigSessionCommand(
        std::array<compat::RetainedString, 2> presentation, String runId)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1]), runId) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(DeleteAutoRigSessionCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.DeleteAutoRigSessionCommand"));
}
std::array<compat::RetainedString, 2> ResumeAutoRigCommand::sourceConstructorPresentation() {
    return {
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Resume AutoRig")),
        ngTranslateRetained(
            compat::RetainedString::fromStaticLiteral("Resume a saved AutoRig workflow."))};
}
ResumeAutoRigCommand::ResumeAutoRigCommand(String runId,String stepId)
    : ResumeAutoRigCommand(sourceConstructorPresentation(), runId, stepId) {}
ResumeAutoRigCommand::ResumeAutoRigCommand(
        std::array<compat::RetainedString, 2> presentation, String runId, String stepId)
    : ExCommand(std::move(presentation[0]),
        std::move(presentation[1]), runId, stepId) {
    nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ResumeAutoRigCommand),
        compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.ResumeAutoRigCommand"));
}

Result ShowImportSessionDataDialogCommand::run(ContextRef context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    compat::Slice<io::TFD_Filter> filters;
    filters.resize(1);
    const compat::Slice<const char*> patterns{"*.inp"};
    filters[0]={{patterns.data(),patterns.size()},"nijilive Puppet (*.inp)"};
    const auto path=io::incShowImportDialog({filters.data(),filters.size()},io::IoString(ngTranslateRetained(
        compat::RetainedString::fromStaticLiteral("Import..."))));
    if(!path.isNull()) {
        const auto entry = commands.find(ToolCommand::ImportSessionData);
        if (entry == commands.end()) throw compat::ngStaticError<compat::RangeError>(
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/puppet/tool.d"), 41);
        auto command=std::dynamic_pointer_cast<ImportSessionDataCommand>(entry->second);
        if(command) {command->path=path;return ngRunCommand(command,context);}
    }
    return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("Import canceled"));
}
Result ImportSessionDataCommand::run(ContextRef context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if(!compat::ngRequireObject(context)->hasPuppet())
        return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("No puppet"));
    if(!path.isNull()) {
        auto puppet=io::project_reader::ngLoadProjectPuppet<ext::ExPuppet>(path);
        const auto found=compat::ngRequireObject(puppet)->extData.find("com.inochi2d.inochi-session.bindings");
        if(found!=puppet->extData.end()) {
            {
                const auto destination=compat::ngRequireObject(context->puppet());
                auto& destinationData=destination->extData;
                const auto source=puppet->extData.find("com.inochi2d.inochi-session.bindings");
                if(source==puppet->extData.end()) throw compat::ngStaticError<compat::RangeError>(
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/commands/puppet/tool.d"), 62);
                const auto sourceBytes=source->second;
                auto copiedBindings=sourceBytes.duplicate();
                destinationData[compat::RetainedString::fromStaticLiteral(
                    "com.inochi2d.inochi-session.bindings")]=std::move(copiedBindings);
            }
            {
                const auto status=core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                    "Successfully overwrote Inochi Session tracking data..."));
                core::tasks::ngSetStatus(compat::RetainedStringSlice(status));
            }
            puppet.reset();return CommandResult::opCall(true);
        }
        {
            const auto title=core::translation::ngTranslateCString("Error");
            const auto message=core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
                "There was no Inochi Session data to import!"));
            const auto messageOwner=message.retentionOwner();
            widgets::dialog::ngDialog(widgets::dialog::DialogTag::retained(title.data(),title.owner),
                messageOwner.use_count()!=0 ? widgets::dialog::DialogText::retained(message.view(),messageOwner)
                    : widgets::dialog::DialogText::borrowed(message.view()));
        }
        puppet.reset();return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("Session data missing"));
    }
    return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("Path not provided"));
}
Result PremultTextureCommand::run(ContextRef context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if(!compat::ngRequireObject(context)->hasPuppet())
        return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("No puppet"));
    utils::repair::ngPremultTextures(context->puppet());return CommandResult::opCall(true);
}
Result RebleedTextureCommand::run(ContextRef) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    project::incRebleedTextures();return CommandResult::opCall(true);
}
Result RegenerateMipmapsCommand::run(ContextRef) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    project::incRegenerateMipmaps();return CommandResult::opCall(true);
}
Result GenerateFakeLayerNameCommand::run(ContextRef context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if(!compat::ngRequireObject(context)->hasPuppet()||!context->puppet())
        return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("No puppet"));
    for(auto& part:compat::ngRequireObject(context->puppet())->getAllParts()) {
        if(auto value=std::dynamic_pointer_cast<ext::nodes::ExPart>(part)) {
            const auto name = part->getName();
            try {
                std::string layerPath;
                layerPath.reserve(1 + name.view().size());
                layerPath.push_back('/');
                if (!name.view().empty()) layerPath.append(name.view());
                value->layerPath=compat::RetainedString(std::move(layerPath));
            } catch (const std::bad_alloc&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            } catch (const std::length_error&) {
                throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
            }
        }
    }
    return CommandResult::opCall(true);
}
Result AttemptRepairPuppetCommand::run(ContextRef context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if(!compat::ngRequireObject(context)->hasPuppet()||!context->puppet())
        return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("No puppet"));
    utils::repair::ngAttemptRepairPuppet(context->puppet());return CommandResult::opCall(true);
}
Result RegenerateNodeIDsCommand::run(ContextRef context) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if(!compat::ngRequireObject(context)->hasPuppet()||!context->puppet())
        return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("No puppet"));
    utils::repair::ngRegenerateNodeIDs(compat::ngRequireObject(context->puppet())->root);
    return CommandResult::opCall(true);
}
Result ModelEditModeCommand::run(ContextRef) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if(project::incEditMode()!=project::EditMode::ModelEdit) project::incSetEditMode(project::EditMode::ModelEdit);
    return CommandResult::opCall(true);
}
Result AnimEditModeCommand::run(ContextRef) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if(project::incEditMode()!=project::EditMode::AnimEdit) project::incSetEditMode(project::EditMode::AnimEdit);
    return CommandResult::opCall(true);
}
Result ExecuteAutoRigCommand::run(ContextRef ctx) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if(!compat::ngRequireObject(ctx)->hasPuppet()||!ctx->puppet())
        return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("No imported model is open"));
    if(!autoRigExecute)
        return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("AutoRig panel is unavailable"));
    auto execute=autoRigExecute;
    auto optionsJson=autorig::json::detail::FirstPassReader(options.empty()?String(compat::RetainedString::fromStaticLiteral("{}")):options).parseRoot();
    auto contextJson=autorig::json::detail::FirstPassReader(context.empty()?String(compat::RetainedString::fromStaticLiteral("{}")):context).parseRoot();
    return runIdResult(execute(optionsJson,contextJson));
}
Result GetAutoRigStatusCommand::run(ContextRef) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if(!autoRigStatus)
        return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("AutoRig panel is unavailable"));
    return newJsonResult([&] {
        const auto status=autoRigStatus;
        return status(runId);
    });
}
Result GetAutoRigMemoryStatusCommand::run(ContextRef) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if (!autoRigStatus)
        return CommandResult::opCall(false,
            compat::RetainedString::fromStaticLiteral("AutoRig panel is unavailable"));
    const auto statusHandler = autoRigStatus;
    auto status = statusHandler(runId);
    const auto key = []<std::size_t N>(const char (&text)[N]) {
        return core::settings::SettingsString(compat::RetainedString::fromStaticLiteral(text));
    };
    status.sourceIndexAssignSlot(key("undo")) = core::actionstack::ngActionHistoryMemoryInfo();

    const auto atlas = nicxlive::core::render::ngSharedRenderAtlasMemoryInfo();
    Object renderAtlas;
    renderAtlas[key("deformation_registrations")] = Json(atlas[0]);
    renderAtlas[key("vertex_registrations")] = Json(atlas[1]);
    renderAtlas[key("uv_registrations")] = Json(atlas[2]);
    renderAtlas[key("deformation_bytes")] = Json(atlas[3]);
    renderAtlas[key("vertex_bytes")] = Json(atlas[4]);
    renderAtlas[key("uv_bytes")] = Json(atlas[5]);
    status.sourceIndexAssignSlot(key("render_atlas")) = Json(renderAtlas);

    const auto nativeBlobs = autorig::framework::ngAutoRigNativeBlobMemoryInfo();
    Object processMemory;
    processMemory[key("native_blob_count")] = Json(nativeBlobs[0]);
    processMemory[key("native_blob_bytes")] = Json(nativeBlobs[1]);
    status.sourceIndexAssignSlot(key("process_memory")) = Json(processMemory);
    return newJsonResult([&] { return status; });
}
Result DeleteAutoRigSessionCommand::run(ContextRef) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if(!autoRigRemove)
        return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("AutoRig panel is unavailable"));
    auto remove=autoRigRemove;remove(runId);return CommandResult::opCall(true);
}
Result ResumeAutoRigCommand::run(ContextRef) {
    [[maybe_unused]] const auto receiver = weak_from_this().lock();
    if(!autoRigResume)
        return CommandResult::opCall(false,compat::RetainedString::fromStaticLiteral("AutoRig panel is unavailable"));
    auto resume=autoRigResume;return runIdResult(resume(runId,stepId));
}
void ngInitToolCommands() {
    initialization::Program::run<initialization::Table>();
}

}
