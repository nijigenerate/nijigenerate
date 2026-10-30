#include "config_detail.hpp"
#include "detail/config_description.hpp"
#include "dynamic.hpp"
#include <compat/source_allocator.hpp>
#include <utility>
namespace nijigenerate::commands::automesh::config {
using namespace detail;
using namespace viewport::vertex;
namespace am=viewport::vertex::automesh;
using core::settings::SettingsJsonValue;
using core::settings::SettingsJsonObject;
using core::settings::SettingsJsonArray;
using core::settings::SettingsString;
void AutoMeshSetSimple_OptimumAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetSimple_OptimumAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetSimple_OptimumAutoMeshProcessorCommand>();
}
void AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand>();
}
void AutoMeshSetPreset_OptimumAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetPreset_OptimumAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetPreset_OptimumAutoMeshProcessorCommand>();
}
void AutoMeshSetSimple_ContourAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetSimple_ContourAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetSimple_ContourAutoMeshProcessorCommand>();
}
void AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetAdvanced_ContourAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand>();
}
void AutoMeshSetPreset_ContourAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetPreset_ContourAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetPreset_ContourAutoMeshProcessorCommand>();
}
void AutoMeshSetSimple_GridAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetSimple_GridAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetSimple_GridAutoMeshProcessorCommand>();
}
void AutoMeshSetAdvanced_GridAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetAdvanced_GridAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetAdvanced_GridAutoMeshProcessorCommand>();
}
void AutoMeshSetPreset_GridAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetPreset_GridAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetPreset_GridAutoMeshProcessorCommand>();
}
void AutoMeshSetSimple_SkeletonExtractorCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetSimple_SkeletonExtractorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetSimple_SkeletonExtractorCommand>();
}
void AutoMeshSetAdvanced_SkeletonExtractorCommand::prepareSourceClassInfo() {
    ExCommand<CommandNoArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetAdvanced_SkeletonExtractorCommand>();
}
void AutoMeshSetPreset_SkeletonExtractorCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetPreset_SkeletonExtractorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetPreset_SkeletonExtractorCommand>();
}
void GetAutoMeshConfig_OptimumAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<GetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<GetAutoMeshConfig_OptimumAutoMeshProcessorCommand>();
}
void SetAutoMeshConfig_OptimumAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<SetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<SetAutoMeshConfig_OptimumAutoMeshProcessorCommand>();
}
void GetAutoMeshConfig_ContourAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<GetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<GetAutoMeshConfig_ContourAutoMeshProcessorCommand>();
}
void SetAutoMeshConfig_ContourAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<SetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<SetAutoMeshConfig_ContourAutoMeshProcessorCommand>();
}
void GetAutoMeshConfig_GridAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<GetAutoMeshConfig_GridAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<GetAutoMeshConfig_GridAutoMeshProcessorCommand>();
}
void SetAutoMeshConfig_GridAutoMeshProcessorCommand::prepareSourceClassInfo() {
    ExCommand<SetAutoMeshConfig_GridAutoMeshProcessorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<SetAutoMeshConfig_GridAutoMeshProcessorCommand>();
}
void GetAutoMeshConfig_SkeletonExtractorCommand::prepareSourceClassInfo() {
    ExCommand<GetAutoMeshConfig_SkeletonExtractorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<GetAutoMeshConfig_SkeletonExtractorCommand>();
}
void SetAutoMeshConfig_SkeletonExtractorCommand::prepareSourceClassInfo() {
    ExCommand<SetAutoMeshConfig_SkeletonExtractorCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<SetAutoMeshConfig_SkeletonExtractorCommand>();
}
void AutoMeshListProcessorsCommand::prepareSourceClassInfo() {
    ExCommand<CommandNoArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshListProcessorsCommand>();
}
void AutoMeshGetActiveCommand::prepareSourceClassInfo() {
    ExCommand<CommandNoArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshGetActiveCommand>();
}
void AutoMeshSetActiveCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetActiveCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetActiveCommand>();
}
void AutoMeshApplyActiveCommand::prepareSourceClassInfo() {
    ExCommand<CommandNoArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshApplyActiveCommand>();
}
void AutoMeshGetSchemaCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshGetSchemaCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshGetSchemaCommand>();
}
void AutoMeshGetValuesCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshGetValuesCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshGetValuesCommand>();
}
void AutoMeshSetPresetCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetPresetCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetPresetCommand>();
}
void AutoMeshSetValuesCommand::prepareSourceClassInfo() {
    ExCommand<AutoMeshSetValuesCommandArguments>::prepareSourceClassInfo();
    ::nijigenerate::commands::detail::prepareCommandResultClass<AutoMeshSetValuesCommand>();
}
detail::ConfigCommandText AutoMeshListProcessorsCommand::sourceConstructorText() {
    return commandText("List AutoMesh Processors", "Show available AutoMesh processors");
}
AutoMeshListProcessorsCommand::AutoMeshListProcessorsCommand()
    : AutoMeshListProcessorsCommand(sourceConstructorText()) {}
AutoMeshListProcessorsCommand::AutoMeshListProcessorsCommand(detail::ConfigCommandText text)
    : ExCommand<CommandNoArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshListProcessorsCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshListProcessorsCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    SettingsJsonArray array;
    { SettingsJsonObject object;
      using Info=viewport::vertex::automesh::meta::AMProcInfo<viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor>;
      object[SettingsString(String::fromStaticLiteral("id"))]=SettingsString(String::fromStaticView(Info::id));
      object[SettingsString(String::fromStaticLiteral("name"))]=SettingsString(String::fromStaticView(Info::name));
      object[SettingsString(String::fromStaticLiteral("order"))]=std::int64_t(Info::order);
      auto instance=std::allocate_shared<viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor>(
          nicxlive::core::d_runtime::SourceAllocator<viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor>{});
      object[SettingsString(String::fromStaticLiteral("icon"))]=SettingsString(instance->icon());
      object[SettingsString(String::fromStaticLiteral("reflectable"))]=bool(std::dynamic_pointer_cast<Reflect>(instance));
      array.append(SettingsJsonValue(object)); }
    { SettingsJsonObject object;
      using Info=viewport::vertex::automesh::meta::AMProcInfo<viewport::vertex::automesh::contours::ContourAutoMeshProcessor>;
      object[SettingsString(String::fromStaticLiteral("id"))]=SettingsString(String::fromStaticView(Info::id));
      object[SettingsString(String::fromStaticLiteral("name"))]=SettingsString(String::fromStaticView(Info::name));
      object[SettingsString(String::fromStaticLiteral("order"))]=std::int64_t(Info::order);
      auto instance=std::allocate_shared<viewport::vertex::automesh::contours::ContourAutoMeshProcessor>(
          nicxlive::core::d_runtime::SourceAllocator<viewport::vertex::automesh::contours::ContourAutoMeshProcessor>{});
      object[SettingsString(String::fromStaticLiteral("icon"))]=SettingsString(instance->icon());
      object[SettingsString(String::fromStaticLiteral("reflectable"))]=bool(std::dynamic_pointer_cast<Reflect>(instance));
      array.append(SettingsJsonValue(object)); }
    { SettingsJsonObject object;
      using Info=viewport::vertex::automesh::meta::AMProcInfo<viewport::vertex::automesh::grid::GridAutoMeshProcessor>;
      object[SettingsString(String::fromStaticLiteral("id"))]=SettingsString(String::fromStaticView(Info::id));
      object[SettingsString(String::fromStaticLiteral("name"))]=SettingsString(String::fromStaticView(Info::name));
      object[SettingsString(String::fromStaticLiteral("order"))]=std::int64_t(Info::order);
      auto instance=std::allocate_shared<viewport::vertex::automesh::grid::GridAutoMeshProcessor>(
          nicxlive::core::d_runtime::SourceAllocator<viewport::vertex::automesh::grid::GridAutoMeshProcessor>{});
      object[SettingsString(String::fromStaticLiteral("icon"))]=SettingsString(instance->icon());
      object[SettingsString(String::fromStaticLiteral("reflectable"))]=bool(std::dynamic_pointer_cast<Reflect>(instance));
      array.append(SettingsJsonValue(object)); }
    { SettingsJsonObject object;
      using Info=viewport::vertex::automesh::meta::AMProcInfo<viewport::vertex::automesh::skeletonize::SkeletonExtractor>;
      object[SettingsString(String::fromStaticLiteral("id"))]=SettingsString(String::fromStaticView(Info::id));
      object[SettingsString(String::fromStaticLiteral("name"))]=SettingsString(String::fromStaticView(Info::name));
      object[SettingsString(String::fromStaticLiteral("order"))]=std::int64_t(Info::order);
      auto instance=std::allocate_shared<viewport::vertex::automesh::skeletonize::SkeletonExtractor>(
          nicxlive::core::d_runtime::SourceAllocator<viewport::vertex::automesh::skeletonize::SkeletonExtractor>{});
      object[SettingsString(String::fromStaticLiteral("icon"))]=SettingsString(instance->icon());
      object[SettingsString(String::fromStaticLiteral("reflectable"))]=bool(std::dynamic_pointer_cast<Reflect>(instance));
      array.append(SettingsJsonValue(object)); }
    dialog("AutoMesh Processors", [&] { return compat::ngJsonStringify(SettingsJsonValue(array)); });
    return result(true);
}
detail::ConfigCommandText AutoMeshGetActiveCommand::sourceConstructorText() {
    return commandText("Get Active AutoMesh", "Show active AutoMesh processor");
}
AutoMeshGetActiveCommand::AutoMeshGetActiveCommand()
    : AutoMeshGetActiveCommand(sourceConstructorText()) {}
AutoMeshGetActiveCommand::AutoMeshGetActiveCommand(detail::ConfigCommandText text)
    : ExCommand<CommandNoArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshGetActiveCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshGetActiveCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=ngActiveAutoMeshProcessor();
    if (!processor) return result(false,"No active AutoMesh processor");
    SettingsJsonObject object;
    object[SettingsString(String::fromStaticLiteral("id"))]=SettingsString(processor->procId());
    object[SettingsString(String::fromStaticLiteral("name"))]=SettingsString(processor->displayName());
    object[SettingsString(String::fromStaticLiteral("icon"))]=SettingsString(processor->icon());
    object[SettingsString(String::fromStaticLiteral("reflectable"))]=bool(std::dynamic_pointer_cast<Reflect>(processor));
    dialog("Active AutoMesh", [&] { return compat::ngJsonStringify(SettingsJsonValue(object)); });
    return result(true);
}
detail::ConfigCommandText AutoMeshSetActiveCommand::sourceConstructorText() {
    return commandText("Set Active AutoMesh", "Activate AutoMesh processor");
}
AutoMeshSetActiveCommand::AutoMeshSetActiveCommand(compat::RetainedString processorId)
    : AutoMeshSetActiveCommand(sourceConstructorText(), processorId) {}
AutoMeshSetActiveCommand::AutoMeshSetActiveCommand(detail::ConfigCommandText text, compat::RetainedString processorId)
    : ExCommand<AutoMeshSetActiveCommandArguments>(std::move(text.label), std::move(text.description), processorId) {}
bool AutoMeshSetActiveCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetActiveCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    for (auto processor:ngAutoMeshProcessors()) if (compat::ngRequireObject(processor)->procId()==processorId) { ngActiveAutoMeshProcessor(processor); return result(true); }
    dialog("Error", [&] { return format("Processor not found: %s",processorId); });
    return result(false,"Processor not found");
}
detail::ConfigCommandText AutoMeshApplyActiveCommand::sourceConstructorText() {
    return commandText("Apply Active AutoMesh", "Apply active AutoMesh to targets");
}
AutoMeshApplyActiveCommand::AutoMeshApplyActiveCommand()
    : AutoMeshApplyActiveCommand(sourceConstructorText()) {}
AutoMeshApplyActiveCommand::AutoMeshApplyActiveCommand(detail::ConfigCommandText text)
    : ExCommand<CommandNoArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshApplyActiveCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshApplyActiveCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=ngActiveAutoMeshProcessor();
    if (!processor) return result(false,"No active AutoMesh processor");
    auto command=dynamic::ngEnsureApplyAutoMeshCommand(compat::RetainedStringSlice(processor->procId()));
    if (!command) return result(false,"Apply command missing");
    return ngRunCommand(command,context);
}
detail::ConfigCommandText AutoMeshGetSchemaCommand::sourceConstructorText() {
    return commandText("Get AutoMesh Schema", "Show AutoMesh reflection schema");
}
AutoMeshGetSchemaCommand::AutoMeshGetSchemaCommand(compat::RetainedString processorId)
    : AutoMeshGetSchemaCommand(sourceConstructorText(), processorId) {}
AutoMeshGetSchemaCommand::AutoMeshGetSchemaCommand(detail::ConfigCommandText text, compat::RetainedString processorId)
    : ExCommand<AutoMeshGetSchemaCommandArguments>(std::move(text.label), std::move(text.description), processorId) {}
bool AutoMeshGetSchemaCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshGetSchemaCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolve(processorId);
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    const auto schema=reflect->schema();
    dialog("Schema", [&] { return schema; });
    return result(true);
}
detail::ConfigCommandText AutoMeshGetValuesCommand::sourceConstructorText() {
    return commandText("Get AutoMesh Values", "Show AutoMesh config values");
}
AutoMeshGetValuesCommand::AutoMeshGetValuesCommand(compat::RetainedString processorId, compat::RetainedString level)
    : AutoMeshGetValuesCommand(sourceConstructorText(), processorId, level) {}
AutoMeshGetValuesCommand::AutoMeshGetValuesCommand(detail::ConfigCommandText text, compat::RetainedString processorId, compat::RetainedString level)
    : ExCommand<AutoMeshGetValuesCommandArguments>(std::move(text.label), std::move(text.description), processorId, level) {}
bool AutoMeshGetValuesCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshGetValuesCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolve(processorId);
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    const auto values=reflect->values(level.empty()?String::fromStaticLiteral("Simple"):level);
    dialog("Values", [&] { return values; });
    return result(true);
}
detail::ConfigCommandText AutoMeshSetPresetCommand::sourceConstructorText() {
    return commandText("Set AutoMesh Preset", "Apply AutoMesh preset");
}
AutoMeshSetPresetCommand::AutoMeshSetPresetCommand(compat::RetainedString processorId, compat::RetainedString preset)
    : AutoMeshSetPresetCommand(sourceConstructorText(), processorId, preset) {}
AutoMeshSetPresetCommand::AutoMeshSetPresetCommand(detail::ConfigCommandText text, compat::RetainedString processorId, compat::RetainedString preset)
    : ExCommand<AutoMeshSetPresetCommandArguments>(std::move(text.label), std::move(text.description), processorId, preset) {}
bool AutoMeshSetPresetCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetPresetCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolve(processorId);
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    auto oldState=snapshot(reflect);
    if(!reflect->applyPreset(preset)) { dialog("Error", [&] { return format("Preset not found: %s",preset); }); return result(false,"Preset not found"); }
    push(processor,reflect,oldState);
    return result(true);
}
detail::ConfigCommandText AutoMeshSetValuesCommand::sourceConstructorText() {
    return commandText("Set AutoMesh Values", "Update AutoMesh config values");
}
AutoMeshSetValuesCommand::AutoMeshSetValuesCommand(compat::RetainedString processorId, compat::RetainedString level, compat::RetainedString updates)
    : AutoMeshSetValuesCommand(sourceConstructorText(), processorId, level, updates) {}
AutoMeshSetValuesCommand::AutoMeshSetValuesCommand(detail::ConfigCommandText text, compat::RetainedString processorId, compat::RetainedString level, compat::RetainedString updates)
    : ExCommand<AutoMeshSetValuesCommandArguments>(std::move(text.label), std::move(text.description), processorId, level, updates) {}
bool AutoMeshSetValuesCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return !updates.empty();
}
std::shared_ptr<CommandResult> AutoMeshSetValuesCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolve(processorId);
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    auto oldState=snapshot(reflect);
    if(!reflect->writeValues(level.empty()?String::fromStaticLiteral("Simple"):level,updates)) { dialog("Error", [&] { return tr("No values applied"); }); return result(false,"No values applied"); }
    push(processor,reflect,oldState);
    return result(true);
}
detail::ConfigCommandText GetAutoMeshConfig_OptimumAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Get Config (%s)",
        am::meta::AMProcInfo<am::optimum::OptimumAutoMeshProcessor>::name,
        "Get AutoMesh config (per processor)");
}
GetAutoMeshConfig_OptimumAutoMeshProcessorCommand::GetAutoMeshConfig_OptimumAutoMeshProcessorCommand(compat::RetainedString level)
    : GetAutoMeshConfig_OptimumAutoMeshProcessorCommand(sourceConstructorText(), level) {}
GetAutoMeshConfig_OptimumAutoMeshProcessorCommand::GetAutoMeshConfig_OptimumAutoMeshProcessorCommand(detail::ConfigCommandText text, compat::RetainedString level)
    : ExCommand<GetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description), level) {}
bool GetAutoMeshConfig_OptimumAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> GetAutoMeshConfig_OptimumAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolveInstance<viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor>();
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    const auto values=reflect->values(level.empty()?String::fromStaticLiteral("Simple"):level);
    dialog("Values", [&] { return values; });
    return result(true);
}
detail::ConfigCommandText SetAutoMeshConfig_OptimumAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Set Config (%s)",
        am::meta::AMProcInfo<am::optimum::OptimumAutoMeshProcessor>::name,
        "Set AutoMesh config (per processor)");
}
SetAutoMeshConfig_OptimumAutoMeshProcessorCommand::SetAutoMeshConfig_OptimumAutoMeshProcessorCommand(compat::RetainedString level, compat::RetainedString updates)
    : SetAutoMeshConfig_OptimumAutoMeshProcessorCommand(sourceConstructorText(), level, updates) {}
SetAutoMeshConfig_OptimumAutoMeshProcessorCommand::SetAutoMeshConfig_OptimumAutoMeshProcessorCommand(detail::ConfigCommandText text, compat::RetainedString level, compat::RetainedString updates)
    : ExCommand<SetAutoMeshConfig_OptimumAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description), level, updates) {}
bool SetAutoMeshConfig_OptimumAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return !updates.empty();
}
std::shared_ptr<CommandResult> SetAutoMeshConfig_OptimumAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolveInstance<viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor>();
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    auto oldState=snapshot(reflect);
    if(!reflect->writeValues(level.empty()?String::fromStaticLiteral("Simple"):level,updates)) { dialog("Error", [&] { return tr("No values applied"); }); return result(false,"No values applied"); }
    push(processor,reflect,oldState);
    return result(true);
}
detail::ConfigCommandText AutoMeshSetSimple_OptimumAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Set Simple (%s)",
        am::meta::AMProcInfo<am::optimum::OptimumAutoMeshProcessor>::name,
        configSetDescription<am::optimum::OptimumAutoMeshProcessor,am::meta::AutoMeshLevel::Simple>());
}
AutoMeshSetSimple_OptimumAutoMeshProcessorCommand::AutoMeshSetSimple_OptimumAutoMeshProcessorCommand()
    : AutoMeshSetSimple_OptimumAutoMeshProcessorCommand(sourceConstructorText()) {}
AutoMeshSetSimple_OptimumAutoMeshProcessorCommand::AutoMeshSetSimple_OptimumAutoMeshProcessorCommand(detail::ConfigCommandText text)
    : ExCommand<AutoMeshSetSimple_OptimumAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshSetSimple_OptimumAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetSimple_OptimumAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    return runConfigLevel<viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor>([&](const auto& processor) {
        writeConfigArray<am::optimum::OptimumAutoMeshProcessor,6>(*processor,scales);
        writeConfigFloat<am::optimum::OptimumAutoMeshProcessor,7>(*processor,min_distance,1,200);
        writeConfigFloat<am::optimum::OptimumAutoMeshProcessor,8>(*processor,mask_threshold,1,200);
        writeConfigFloat<am::optimum::OptimumAutoMeshProcessor,9>(*processor,div_per_part,4,64);
    });
}
detail::ConfigCommandText AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Set Advanced (%s)",
        am::meta::AMProcInfo<am::optimum::OptimumAutoMeshProcessor>::name,
        configSetDescription<am::optimum::OptimumAutoMeshProcessor,am::meta::AutoMeshLevel::Advanced>());
}
AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand::AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand()
    : AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand(sourceConstructorText()) {}
AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand::AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand(detail::ConfigCommandText text)
    : ExCommand<AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    return runConfigLevel<viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor>([&](const auto& processor) {
        writeConfigFloat<am::optimum::OptimumAutoMeshProcessor,0>(*processor,large_threshold,50,2000);
        writeConfigFloat<am::optimum::OptimumAutoMeshProcessor,1>(*processor,length_threshold,20,2000);
        writeConfigFloat<am::optimum::OptimumAutoMeshProcessor,2>(*processor,ratio_threshold,0.01,1.0);
        writeConfigFloat<am::optimum::OptimumAutoMeshProcessor,3>(*processor,sharp_expand,0,0.2);
        writeConfigFloat<am::optimum::OptimumAutoMeshProcessor,4>(*processor,unsharp_expand,0,0.5);
        writeConfigFloat<am::optimum::OptimumAutoMeshProcessor,5>(*processor,unsharp_contract,0,0.5);
    });
}
detail::ConfigCommandText AutoMeshSetPreset_OptimumAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Set Preset (%s)",
        am::meta::AMProcInfo<am::optimum::OptimumAutoMeshProcessor>::name,
        "Apply preset");
}
AutoMeshSetPreset_OptimumAutoMeshProcessorCommand::AutoMeshSetPreset_OptimumAutoMeshProcessorCommand()
    : AutoMeshSetPreset_OptimumAutoMeshProcessorCommand(sourceConstructorText()) {}
AutoMeshSetPreset_OptimumAutoMeshProcessorCommand::AutoMeshSetPreset_OptimumAutoMeshProcessorCommand(detail::ConfigCommandText text)
    : ExCommand<AutoMeshSetPreset_OptimumAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshSetPreset_OptimumAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetPreset_OptimumAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolveInstance<viewport::vertex::automesh::optimum::OptimumAutoMeshProcessor>();
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    auto oldState=snapshot(reflect);
    if(!reflect->applyPreset(preset)) { dialog("Error", [&] { return format("Preset not found: %s",preset); }); return result(false,"Preset not found"); }
    push(processor,reflect,oldState);
    return result(true);
}
detail::ConfigCommandText GetAutoMeshConfig_ContourAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Get Config (%s)",
        am::meta::AMProcInfo<am::contours::ContourAutoMeshProcessor>::name,
        "Get AutoMesh config (per processor)");
}
GetAutoMeshConfig_ContourAutoMeshProcessorCommand::GetAutoMeshConfig_ContourAutoMeshProcessorCommand(compat::RetainedString level)
    : GetAutoMeshConfig_ContourAutoMeshProcessorCommand(sourceConstructorText(), level) {}
GetAutoMeshConfig_ContourAutoMeshProcessorCommand::GetAutoMeshConfig_ContourAutoMeshProcessorCommand(detail::ConfigCommandText text, compat::RetainedString level)
    : ExCommand<GetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description), level) {}
bool GetAutoMeshConfig_ContourAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> GetAutoMeshConfig_ContourAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolveInstance<viewport::vertex::automesh::contours::ContourAutoMeshProcessor>();
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    const auto values=reflect->values(level.empty()?String::fromStaticLiteral("Simple"):level);
    dialog("Values", [&] { return values; });
    return result(true);
}
detail::ConfigCommandText SetAutoMeshConfig_ContourAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Set Config (%s)",
        am::meta::AMProcInfo<am::contours::ContourAutoMeshProcessor>::name,
        "Set AutoMesh config (per processor)");
}
SetAutoMeshConfig_ContourAutoMeshProcessorCommand::SetAutoMeshConfig_ContourAutoMeshProcessorCommand(compat::RetainedString level, compat::RetainedString updates)
    : SetAutoMeshConfig_ContourAutoMeshProcessorCommand(sourceConstructorText(), level, updates) {}
SetAutoMeshConfig_ContourAutoMeshProcessorCommand::SetAutoMeshConfig_ContourAutoMeshProcessorCommand(detail::ConfigCommandText text, compat::RetainedString level, compat::RetainedString updates)
    : ExCommand<SetAutoMeshConfig_ContourAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description), level, updates) {}
bool SetAutoMeshConfig_ContourAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return !updates.empty();
}
std::shared_ptr<CommandResult> SetAutoMeshConfig_ContourAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolveInstance<viewport::vertex::automesh::contours::ContourAutoMeshProcessor>();
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    auto oldState=snapshot(reflect);
    if(!reflect->writeValues(level.empty()?String::fromStaticLiteral("Simple"):level,updates)) { dialog("Error", [&] { return tr("No values applied"); }); return result(false,"No values applied"); }
    push(processor,reflect,oldState);
    return result(true);
}
detail::ConfigCommandText AutoMeshSetSimple_ContourAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Set Simple (%s)",
        am::meta::AMProcInfo<am::contours::ContourAutoMeshProcessor>::name,
        configSetDescription<am::contours::ContourAutoMeshProcessor,am::meta::AutoMeshLevel::Simple>());
}
AutoMeshSetSimple_ContourAutoMeshProcessorCommand::AutoMeshSetSimple_ContourAutoMeshProcessorCommand()
    : AutoMeshSetSimple_ContourAutoMeshProcessorCommand(sourceConstructorText()) {}
AutoMeshSetSimple_ContourAutoMeshProcessorCommand::AutoMeshSetSimple_ContourAutoMeshProcessorCommand(detail::ConfigCommandText text)
    : ExCommand<AutoMeshSetSimple_ContourAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshSetSimple_ContourAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetSimple_ContourAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    return runConfigLevel<viewport::vertex::automesh::contours::ContourAutoMeshProcessor>([&](const auto& processor) {
        writeConfigFloat<am::contours::ContourAutoMeshProcessor,0>(*processor,sampling_step,1,200);
        writeConfigFloat<am::contours::ContourAutoMeshProcessor,1>(*processor,mask_threshold,1,200);
    });
}
detail::ConfigCommandText AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Set Advanced (%s)",
        am::meta::AMProcInfo<am::contours::ContourAutoMeshProcessor>::name,
        configSetDescription<am::contours::ContourAutoMeshProcessor,am::meta::AutoMeshLevel::Advanced>());
}
AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand::AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand()
    : AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand(sourceConstructorText()) {}
AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand::AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand(detail::ConfigCommandText text)
    : ExCommand<AutoMeshSetAdvanced_ContourAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    return runConfigLevel<viewport::vertex::automesh::contours::ContourAutoMeshProcessor>([&](const auto& processor) {
        writeConfigFloat<am::contours::ContourAutoMeshProcessor,2>(*processor,min_distance,1,200);
        writeConfigFloat<am::contours::ContourAutoMeshProcessor,3>(*processor,max_distance,-1,200);
        writeConfigArray<am::contours::ContourAutoMeshProcessor,4>(*processor,scales);
    });
}
detail::ConfigCommandText AutoMeshSetPreset_ContourAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Set Preset (%s)",
        am::meta::AMProcInfo<am::contours::ContourAutoMeshProcessor>::name,
        "Apply preset");
}
AutoMeshSetPreset_ContourAutoMeshProcessorCommand::AutoMeshSetPreset_ContourAutoMeshProcessorCommand()
    : AutoMeshSetPreset_ContourAutoMeshProcessorCommand(sourceConstructorText()) {}
AutoMeshSetPreset_ContourAutoMeshProcessorCommand::AutoMeshSetPreset_ContourAutoMeshProcessorCommand(detail::ConfigCommandText text)
    : ExCommand<AutoMeshSetPreset_ContourAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshSetPreset_ContourAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetPreset_ContourAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolveInstance<viewport::vertex::automesh::contours::ContourAutoMeshProcessor>();
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    auto oldState=snapshot(reflect);
    if(!reflect->applyPreset(preset)) { dialog("Error", [&] { return format("Preset not found: %s",preset); }); return result(false,"Preset not found"); }
    push(processor,reflect,oldState);
    return result(true);
}
detail::ConfigCommandText GetAutoMeshConfig_GridAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Get Config (%s)",
        am::meta::AMProcInfo<am::grid::GridAutoMeshProcessor>::name,
        "Get AutoMesh config (per processor)");
}
GetAutoMeshConfig_GridAutoMeshProcessorCommand::GetAutoMeshConfig_GridAutoMeshProcessorCommand(compat::RetainedString level)
    : GetAutoMeshConfig_GridAutoMeshProcessorCommand(sourceConstructorText(), level) {}
GetAutoMeshConfig_GridAutoMeshProcessorCommand::GetAutoMeshConfig_GridAutoMeshProcessorCommand(detail::ConfigCommandText text, compat::RetainedString level)
    : ExCommand<GetAutoMeshConfig_GridAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description), level) {}
bool GetAutoMeshConfig_GridAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> GetAutoMeshConfig_GridAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolveInstance<viewport::vertex::automesh::grid::GridAutoMeshProcessor>();
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    const auto values=reflect->values(level.empty()?String::fromStaticLiteral("Simple"):level);
    dialog("Values", [&] { return values; });
    return result(true);
}
detail::ConfigCommandText SetAutoMeshConfig_GridAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Set Config (%s)",
        am::meta::AMProcInfo<am::grid::GridAutoMeshProcessor>::name,
        "Set AutoMesh config (per processor)");
}
SetAutoMeshConfig_GridAutoMeshProcessorCommand::SetAutoMeshConfig_GridAutoMeshProcessorCommand(compat::RetainedString level, compat::RetainedString updates)
    : SetAutoMeshConfig_GridAutoMeshProcessorCommand(sourceConstructorText(), level, updates) {}
SetAutoMeshConfig_GridAutoMeshProcessorCommand::SetAutoMeshConfig_GridAutoMeshProcessorCommand(detail::ConfigCommandText text, compat::RetainedString level, compat::RetainedString updates)
    : ExCommand<SetAutoMeshConfig_GridAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description), level, updates) {}
bool SetAutoMeshConfig_GridAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return !updates.empty();
}
std::shared_ptr<CommandResult> SetAutoMeshConfig_GridAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolveInstance<viewport::vertex::automesh::grid::GridAutoMeshProcessor>();
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    auto oldState=snapshot(reflect);
    if(!reflect->writeValues(level.empty()?String::fromStaticLiteral("Simple"):level,updates)) { dialog("Error", [&] { return tr("No values applied"); }); return result(false,"No values applied"); }
    push(processor,reflect,oldState);
    return result(true);
}
detail::ConfigCommandText AutoMeshSetSimple_GridAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Set Simple (%s)",
        am::meta::AMProcInfo<am::grid::GridAutoMeshProcessor>::name,
        configSetDescription<am::grid::GridAutoMeshProcessor,am::meta::AutoMeshLevel::Simple>());
}
AutoMeshSetSimple_GridAutoMeshProcessorCommand::AutoMeshSetSimple_GridAutoMeshProcessorCommand()
    : AutoMeshSetSimple_GridAutoMeshProcessorCommand(sourceConstructorText()) {}
AutoMeshSetSimple_GridAutoMeshProcessorCommand::AutoMeshSetSimple_GridAutoMeshProcessorCommand(detail::ConfigCommandText text)
    : ExCommand<AutoMeshSetSimple_GridAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshSetSimple_GridAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetSimple_GridAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    return runConfigLevel<viewport::vertex::automesh::grid::GridAutoMeshProcessor>([&](const auto& processor) {
        writeConfigFloat<am::grid::GridAutoMeshProcessor,2>(*processor,mask_threshold,1,200);
        writeConfigFloat<am::grid::GridAutoMeshProcessor,3>(*processor,x_segments,2,20);
        writeConfigFloat<am::grid::GridAutoMeshProcessor,4>(*processor,y_segments,2,20);
        writeConfigFloat<am::grid::GridAutoMeshProcessor,5>(*processor,margin,0,1);
    });
}
detail::ConfigCommandText AutoMeshSetAdvanced_GridAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Set Advanced (%s)",
        am::meta::AMProcInfo<am::grid::GridAutoMeshProcessor>::name,
        configSetDescription<am::grid::GridAutoMeshProcessor,am::meta::AutoMeshLevel::Advanced>());
}
AutoMeshSetAdvanced_GridAutoMeshProcessorCommand::AutoMeshSetAdvanced_GridAutoMeshProcessorCommand()
    : AutoMeshSetAdvanced_GridAutoMeshProcessorCommand(sourceConstructorText()) {}
AutoMeshSetAdvanced_GridAutoMeshProcessorCommand::AutoMeshSetAdvanced_GridAutoMeshProcessorCommand(detail::ConfigCommandText text)
    : ExCommand<AutoMeshSetAdvanced_GridAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshSetAdvanced_GridAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetAdvanced_GridAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    return runConfigLevel<viewport::vertex::automesh::grid::GridAutoMeshProcessor>([&](const auto& processor) {
        writeConfigArray<am::grid::GridAutoMeshProcessor,0>(*processor,scale_x);
        writeConfigArray<am::grid::GridAutoMeshProcessor,1>(*processor,scale_y);
    });
}
detail::ConfigCommandText AutoMeshSetPreset_GridAutoMeshProcessorCommand::sourceConstructorText() {
    return formattedCommandText("Set Preset (%s)",
        am::meta::AMProcInfo<am::grid::GridAutoMeshProcessor>::name,
        "Apply preset");
}
AutoMeshSetPreset_GridAutoMeshProcessorCommand::AutoMeshSetPreset_GridAutoMeshProcessorCommand()
    : AutoMeshSetPreset_GridAutoMeshProcessorCommand(sourceConstructorText()) {}
AutoMeshSetPreset_GridAutoMeshProcessorCommand::AutoMeshSetPreset_GridAutoMeshProcessorCommand(detail::ConfigCommandText text)
    : ExCommand<AutoMeshSetPreset_GridAutoMeshProcessorCommandArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshSetPreset_GridAutoMeshProcessorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetPreset_GridAutoMeshProcessorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolveInstance<viewport::vertex::automesh::grid::GridAutoMeshProcessor>();
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    auto oldState=snapshot(reflect);
    if(!reflect->applyPreset(preset)) { dialog("Error", [&] { return format("Preset not found: %s",preset); }); return result(false,"Preset not found"); }
    push(processor,reflect,oldState);
    return result(true);
}
detail::ConfigCommandText GetAutoMeshConfig_SkeletonExtractorCommand::sourceConstructorText() {
    return formattedCommandText("Get Config (%s)",
        am::meta::AMProcInfo<am::skeletonize::SkeletonExtractor>::name,
        "Get AutoMesh config (per processor)");
}
GetAutoMeshConfig_SkeletonExtractorCommand::GetAutoMeshConfig_SkeletonExtractorCommand(compat::RetainedString level)
    : GetAutoMeshConfig_SkeletonExtractorCommand(sourceConstructorText(), level) {}
GetAutoMeshConfig_SkeletonExtractorCommand::GetAutoMeshConfig_SkeletonExtractorCommand(detail::ConfigCommandText text, compat::RetainedString level)
    : ExCommand<GetAutoMeshConfig_SkeletonExtractorCommandArguments>(std::move(text.label), std::move(text.description), level) {}
bool GetAutoMeshConfig_SkeletonExtractorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> GetAutoMeshConfig_SkeletonExtractorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolveInstance<viewport::vertex::automesh::skeletonize::SkeletonExtractor>();
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    const auto values=reflect->values(level.empty()?String::fromStaticLiteral("Simple"):level);
    dialog("Values", [&] { return values; });
    return result(true);
}
detail::ConfigCommandText SetAutoMeshConfig_SkeletonExtractorCommand::sourceConstructorText() {
    return formattedCommandText("Set Config (%s)",
        am::meta::AMProcInfo<am::skeletonize::SkeletonExtractor>::name,
        "Set AutoMesh config (per processor)");
}
SetAutoMeshConfig_SkeletonExtractorCommand::SetAutoMeshConfig_SkeletonExtractorCommand(compat::RetainedString level, compat::RetainedString updates)
    : SetAutoMeshConfig_SkeletonExtractorCommand(sourceConstructorText(), level, updates) {}
SetAutoMeshConfig_SkeletonExtractorCommand::SetAutoMeshConfig_SkeletonExtractorCommand(detail::ConfigCommandText text, compat::RetainedString level, compat::RetainedString updates)
    : ExCommand<SetAutoMeshConfig_SkeletonExtractorCommandArguments>(std::move(text.label), std::move(text.description), level, updates) {}
bool SetAutoMeshConfig_SkeletonExtractorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return !updates.empty();
}
std::shared_ptr<CommandResult> SetAutoMeshConfig_SkeletonExtractorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolveInstance<viewport::vertex::automesh::skeletonize::SkeletonExtractor>();
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    auto oldState=snapshot(reflect);
    if(!reflect->writeValues(level.empty()?String::fromStaticLiteral("Simple"):level,updates)) { dialog("Error", [&] { return tr("No values applied"); }); return result(false,"No values applied"); }
    push(processor,reflect,oldState);
    return result(true);
}
detail::ConfigCommandText AutoMeshSetSimple_SkeletonExtractorCommand::sourceConstructorText() {
    return formattedCommandText("Set Simple (%s)",
        am::meta::AMProcInfo<am::skeletonize::SkeletonExtractor>::name,
        configSetDescription<am::skeletonize::SkeletonExtractor,am::meta::AutoMeshLevel::Simple>());
}
AutoMeshSetSimple_SkeletonExtractorCommand::AutoMeshSetSimple_SkeletonExtractorCommand()
    : AutoMeshSetSimple_SkeletonExtractorCommand(sourceConstructorText()) {}
AutoMeshSetSimple_SkeletonExtractorCommand::AutoMeshSetSimple_SkeletonExtractorCommand(detail::ConfigCommandText text)
    : ExCommand<AutoMeshSetSimple_SkeletonExtractorCommandArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshSetSimple_SkeletonExtractorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetSimple_SkeletonExtractorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    return runConfigLevel<viewport::vertex::automesh::skeletonize::SkeletonExtractor>([&](const auto& processor) {
        writeConfigFloat<am::skeletonize::SkeletonExtractor,0>(*processor,mask_threshold,1,200);
        writeConfigFloat<am::skeletonize::SkeletonExtractor,1>(*processor,target_point_count,2,128);
    });
}
detail::ConfigCommandText AutoMeshSetAdvanced_SkeletonExtractorCommand::sourceConstructorText() {
    return formattedCommandText("Set Advanced (%s)",
        am::meta::AMProcInfo<am::skeletonize::SkeletonExtractor>::name,
        configSetDescription<am::skeletonize::SkeletonExtractor,am::meta::AutoMeshLevel::Advanced>());
}
AutoMeshSetAdvanced_SkeletonExtractorCommand::AutoMeshSetAdvanced_SkeletonExtractorCommand()
    : AutoMeshSetAdvanced_SkeletonExtractorCommand(sourceConstructorText()) {}
AutoMeshSetAdvanced_SkeletonExtractorCommand::AutoMeshSetAdvanced_SkeletonExtractorCommand(detail::ConfigCommandText text)
    : ExCommand<CommandNoArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshSetAdvanced_SkeletonExtractorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetAdvanced_SkeletonExtractorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    return runConfigLevel<viewport::vertex::automesh::skeletonize::SkeletonExtractor>([](const auto&) {});
}
detail::ConfigCommandText AutoMeshSetPreset_SkeletonExtractorCommand::sourceConstructorText() {
    return formattedCommandText("Set Preset (%s)",
        am::meta::AMProcInfo<am::skeletonize::SkeletonExtractor>::name,
        "Apply preset");
}
AutoMeshSetPreset_SkeletonExtractorCommand::AutoMeshSetPreset_SkeletonExtractorCommand()
    : AutoMeshSetPreset_SkeletonExtractorCommand(sourceConstructorText()) {}
AutoMeshSetPreset_SkeletonExtractorCommand::AutoMeshSetPreset_SkeletonExtractorCommand(detail::ConfigCommandText text)
    : ExCommand<AutoMeshSetPreset_SkeletonExtractorCommandArguments>(std::move(text.label), std::move(text.description)) {}
bool AutoMeshSetPreset_SkeletonExtractorCommand::runnable(std::shared_ptr<Context>) {
    const auto receiver = weak_from_this().lock();
    return true;
}
std::shared_ptr<CommandResult> AutoMeshSetPreset_SkeletonExtractorCommand::run(std::shared_ptr<Context> context) {
    const auto receiver = weak_from_this().lock();
    auto processor=resolveInstance<viewport::vertex::automesh::skeletonize::SkeletonExtractor>();
    if (!processor) return result(false,"Processor not found");
    auto reflect=std::dynamic_pointer_cast<Reflect>(processor);
    if (!reflect) { dialog("Error", [&] { return tr("Processor not reflectable"); }); return result(false,"Not reflectable"); }
    auto oldState=snapshot(reflect);
    if(!reflect->applyPreset(preset)) { dialog("Error", [&] { return format("Preset not found: %s",preset); }); return result(false,"Preset not found"); }
    push(processor,reflect,oldState);
    return result(true);
}
}
