#pragma once
#include "command_browser_reflection.hpp"
#include <nijigenerate/commands/concrete_catalog.hpp>
#include <nijigenerate/commands/automesh/dynamic.hpp>
#include <nijigenerate/commands/view/toggle_panel_command.hpp>
namespace nijigenerate::windows::command_browser::detail {
struct BrowserDeclaredCommandResult {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
struct BrowserDeclaredPuppetLoadResult {
    static void apply(CommandInfo& info) {
        info.outputType = staticText("LoadResult!(Puppet)");
        info.isResource = true;
        info.resourceType = staticText("Puppet");
        info.resourceChange = staticText("Loaded");
    }
};
struct BrowserDeclaredNodeCreateResult {
    static void apply(CommandInfo& info) {
        info.outputType = staticText("CreateResult!(Node)");
        info.isResource = true;
        info.resourceType = staticText("Node");
        info.resourceChange = staticText("Created");
    }
};
struct BrowserDeclaredJsonResult {
    static void apply(CommandInfo& info) {
        info.outputType = staticText("ExCommandResultImpl!(JSONValue)");
        info.isExResult = true;
        info.exResultType = staticText("JSONValue");
    }
};
#define NG_BROWSER_DECLARED_RESULT(Class, Metadata) \
    template<> struct BrowserSourceResultTraits<Class> : Metadata {}

// Actual return declarations: commands/puppet/file.d:452-1225.
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::NewFileCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowOpenFileDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::OpenFileCommand, BrowserDeclaredPuppetLoadResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowSaveFileDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::SaveFileCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowSaveFileAsDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowImportPSDDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowImportPSDDepthMapDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::OpenPSDDepthMapDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ImportPSDCommand, BrowserDeclaredPuppetLoadResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowImportKRADialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ImportKRACommand, BrowserDeclaredPuppetLoadResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowImportINPDialogCommand, BrowserDeclaredPuppetLoadResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ImportINPCommand, BrowserDeclaredPuppetLoadResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowImportImageFolderDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ImportImageFolderCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowMergePSDDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::MergePSDCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowMergeKRADialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::MergeKRACommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowMergeImageFileDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::MergeImageFilesCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowMergeINPDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::MergeINPCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowExportToINPDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ExportINPCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowExportToPNGDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ExportPNGCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowExportToJpegDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ExportJPEGCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowExportToTGADialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ExportTGACommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ShowExportToVideoDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::ExportVideoCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::file::CloseProjectCommand, BrowserDeclaredCommandResult);

// Actual CommandResult declarations and generated bodies in automesh/config.d.
NG_BROWSER_DECLARED_RESULT(commands::automesh::config::AutoMeshListProcessorsCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::automesh::config::AutoMeshGetActiveCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::automesh::config::AutoMeshSetActiveCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::automesh::config::AutoMeshApplyActiveCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::automesh::config::AutoMeshGetSchemaCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::automesh::config::AutoMeshGetValuesCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::automesh::config::AutoMeshSetPresetCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::automesh::config::AutoMeshSetValuesCommand, BrowserDeclaredCommandResult);
#define NG_BROWSER_PROCESSOR_RESULTS(Processor) \
    NG_BROWSER_DECLARED_RESULT(commands::automesh::config::GetAutoMeshConfig_##Processor##Command, BrowserDeclaredCommandResult); \
    NG_BROWSER_DECLARED_RESULT(commands::automesh::config::SetAutoMeshConfig_##Processor##Command, BrowserDeclaredCommandResult); \
    NG_BROWSER_DECLARED_RESULT(commands::automesh::config::AutoMeshSetSimple_##Processor##Command, BrowserDeclaredCommandResult); \
    NG_BROWSER_DECLARED_RESULT(commands::automesh::config::AutoMeshSetAdvanced_##Processor##Command, BrowserDeclaredCommandResult); \
    NG_BROWSER_DECLARED_RESULT(commands::automesh::config::AutoMeshSetPreset_##Processor##Command, BrowserDeclaredCommandResult)
NG_BROWSER_PROCESSOR_RESULTS(OptimumAutoMeshProcessor);
NG_BROWSER_PROCESSOR_RESULTS(ContourAutoMeshProcessor);
NG_BROWSER_PROCESSOR_RESULTS(GridAutoMeshProcessor);
NG_BROWSER_PROCESSOR_RESULTS(SkeletonExtractor);
#undef NG_BROWSER_PROCESSOR_RESULTS
// dynamic.d:162, open in the same genuine processor type parameter.
template<class PT> struct BrowserSourceResultTraits<commands::automesh::dynamic::ApplyAutoMeshPT<PT>>
    : BrowserDeclaredCommandResult {};
// view/panel.d:29.
NG_BROWSER_DECLARED_RESULT(commands::view::panel::TogglePanelVisibilityCommand, BrowserDeclaredCommandResult);
// Actual node/node.d:88-324 declarations.
NG_BROWSER_DECLARED_RESULT(commands::node::node::SetNodeNameCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::node::DeleteNodeCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::node::MoveNodeCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::node::CutNodeCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::node::CopyNodeCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::node::PasteNodeCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::node::ReloadNodeCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::node::VertexModeCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::node::ToggleVisibilityCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::node::CentralizeNodeCommand, BrowserDeclaredCommandResult);
// puppet/tool.d:35-262 declares CommandResult even for dynamic JSON results.
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::ShowImportSessionDataDialogCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::ImportSessionDataCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::PremultTextureCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::RebleedTextureCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::RegenerateMipmapsCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::GenerateFakeLayerNameCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::AttemptRepairPuppetCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::RegenerateNodeIDsCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::ModelEditModeCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::AnimEditModeCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::ExecuteAutoRigCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::GetAutoRigStatusCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::GetAutoRigMemoryStatusCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::DeleteAutoRigSessionCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::puppet::tool::ResumeAutoRigCommand, BrowserDeclaredCommandResult);
// node/simplephysics.d:319, the actual float-command emission body.
NG_BROWSER_DECLARED_RESULT(commands::node::simplephysics::SetSimplePhysicsGravityCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::simplephysics::SetSimplePhysicsLengthCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::simplephysics::SetSimplePhysicsFrequencyCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::simplephysics::SetSimplePhysicsAngleDampingCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::simplephysics::SetSimplePhysicsLengthDampingCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::simplephysics::SetSimplePhysicsOutputScaleXCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::node::simplephysics::SetSimplePhysicsOutputScaleYCommand, BrowserDeclaredCommandResult);
// Actual depth/bone.d:3818-4771 run declarations.
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::CreateDepthRigRootCommand, BrowserDeclaredNodeCreateResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::AddDepthBoneCommand, BrowserDeclaredNodeCreateResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::AddStandardDepthSkeletonCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::AddStandardDepthParametersCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::FitDepthRigRootZToDepthCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::FitDepthBoneZToDepthCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::SetDepthBoneRestCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::SetDepthBoneConstraintCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::ListDepthBonesCommand, BrowserDeclaredJsonResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::AddDepthBoneSourceCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::RemoveDepthBoneSourceCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::ListDepthBoneSourcesCommand, BrowserDeclaredJsonResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::SetDepthBoneSourceSettingsCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::SetDepthBoneInfluenceRuleCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::GetDepthBoneInfluenceRuleCommand, BrowserDeclaredJsonResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::PreviewDepthBoneInfluenceCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::PreviewDepthBoneDeformCommand, BrowserDeclaredCommandResult);
NG_BROWSER_DECLARED_RESULT(commands::depth::bone::ApplyDepthBoneDeformCommand, BrowserDeclaredCommandResult);
#undef NG_BROWSER_DECLARED_RESULT

// inspector/apply_node.d:60/166 and DefApply:342-348 retain these run declarations.
template<class I, class A, commands::CommandLiteral Target, commands::CommandLiteral Value>
struct BrowserSourceResultTraits<commands::inspector::apply_node::ApplyInspectorPropCommand<I,A,Target,Value>>
    : BrowserDeclaredCommandResult {};
template<class I, class A, commands::CommandLiteral Target>
struct BrowserSourceResultTraits<commands::inspector::apply_node::ToggleInspectorPropCommand<I,A,Target>>
    : BrowserDeclaredCommandResult {};
template<commands::CommandLiteral Id, class I, class A,
    commands::CommandLiteral Target, commands::CommandLiteral Value>
struct BrowserSourceResultTraits<commands::inspector::apply_node::DefinedInspectorApplyCommand<Id,I,A,Target,Value>>
    : BrowserDeclaredCommandResult {};
template<commands::CommandLiteral Id, class I, class A, commands::CommandLiteral Target>
struct BrowserSourceResultTraits<commands::inspector::apply_node::DefinedInspectorToggleCommand<Id,I,A,Target>>
    : BrowserDeclaredCommandResult {};

template<> struct BrowserSourceResultTraits<commands::Command> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/animedit.d
template<> struct BrowserSourceResultTraits<commands::parameter::animedit::AddAnimationKeyFrameCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
template<> struct BrowserSourceResultTraits<commands::parameter::animedit::CopyAnimationKeyFrameCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
template<> struct BrowserSourceResultTraits<commands::parameter::animedit::PasteAnimationKeyFrameCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/node/node.d and node/dynamic.d.
template<bool E> struct BrowserSourceResultTraits<commands::node::node::AddNodeCommandT<E>> {
    static void apply(CommandInfo& info) {
        info.outputType = staticText("CreateResult!(Node)"); info.isResource = true;
        info.resourceType = staticText("Node"); info.resourceChange = staticText("Created");
    }
};
template<bool E> struct BrowserSourceResultTraits<commands::node::node::InsertNodeCommandT<E>> {
    static void apply(CommandInfo& info) {
        info.outputType = staticText("CreateResult!(Node)"); info.isResource = true;
        info.resourceType = staticText("Node"); info.resourceChange = staticText("Created");
    }
};
template<bool E> struct BrowserSourceResultTraits<commands::node::node::ConvertToCommandT<E>> {
    static void apply(CommandInfo& info) {
        info.outputType = staticText("CreateResult!(Node)"); info.isResource = true;
        info.resourceType = staticText("Node"); info.resourceChange = staticText("Created");
    }
};
template<bool E> struct BrowserSourceResultTraits<commands::node::dynamic::ConvertNodeToCommand<E>> {
    static void apply(CommandInfo& info) {
        info.outputType = staticText("CreateResult!(Node)"); info.isResource = true;
        info.resourceType = staticText("Node"); info.resourceChange = staticText("Created");
    }
};
// Source: source/nijigenerate/commands/command_scope.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::GetCurrentCommandScopeCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("ExCommandResultImpl!(JSONValue)"); info.isExResult = true; info.exResultType = staticText("JSONValue"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::CopyBindingCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::FlipDeformCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::InvertKeyFrameCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::MirrorKeyFrameHorizontallyCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::MirrorKeyFrameVerticallyCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::PasteBindingCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CreateResult!(ParameterBinding)"); info.isResource = true; info.resourceType = staticText("ParameterBinding"); info.resourceChange = staticText("Created"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::RemoveBindingCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("DeleteResult!(ParameterBinding)"); info.isResource = true; info.resourceType = staticText("ParameterBinding"); info.resourceChange = staticText("Deleted"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::ResetKeyFrameCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::SetFrom1DMirrorCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::SetFromDiagonalMirrorCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::SetFromHorizontalMirrorCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::SetFromVerticalMirrorCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::SetInterpolationCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::SetKeyFrameCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::SymmetrizeDeformCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/binding/binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::binding::UnsetKeyFrameCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/editor.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::editor::AddEditorDepthOpCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/editor.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::editor::RemoveEditorDepthOpCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/editor.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::editor::UpdateEditorDepthOpCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/map.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::map::AddDepthOpCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/map.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::map::ApplyDepthOpsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/map.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::map::ClearDepthOpsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/map.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::map::ClearDepthsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/map.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::map::ImportPSDDepthsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("ExCommandResultImpl!(JSONValue)"); info.isExResult = true; info.exResultType = staticText("JSONValue"); }
};
// Source: source/nijigenerate/commands/depth/map.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::map::ListDepthOpsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("ExCommandResultImpl!(JSONValue)"); info.isExResult = true; info.exResultType = staticText("JSONValue"); }
};
// Source: source/nijigenerate/commands/depth/map.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::map::ListDepthsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("ExCommandResultImpl!(JSONValue)"); info.isExResult = true; info.exResultType = staticText("JSONValue"); }
};
// Source: source/nijigenerate/commands/depth/map.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::map::MoveDepthOpCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/map.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::map::RemoveDepthOpCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/map.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::map::SetDepthOpsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/map.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::map::SetDepthsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/map.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::map::UpdateDepthOpCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::ApplyPsdDepthDialogCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::CancelPsdDepthDialogCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::FillPsdDepthDialogAlphaDepthGapsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::GetPsdDepthDialogPartDataCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("ExCommandResultImpl!(JSONValue)"); info.isExResult = true; info.exResultType = staticText("JSONValue"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::InspectPsdDepthDialogCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("ExCommandResultImpl!(JSONValue)"); info.isExResult = true; info.exResultType = staticText("JSONValue"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::ResetPsdDepthDialogLayerDepthTransformCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogAlphaThresholdCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogBackDepthCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogChannelCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogColorSourceCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogContourRepairCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogCustomRadiusCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogDepthScaleCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogDirectGridMatchCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogFrontDepthCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogGpuCompositionCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogInvertCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerDepthEnabledCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerDepthInvertedCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerDepthOffsetCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerDepthScaleCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerEnabledCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerMappingAutoCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerMappingIgnoredCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerMappingTargetCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerVisibleCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogMissingPolicyCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogProblemFilterCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogSamplingCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogSurfaceSmoothingCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/depth/psd_dialog.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogTargetEnabledCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/inspector/apply_node.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::inspector::apply_node::MeshGroupDynamicCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/inspector/apply_node.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::inspector::apply_node::MeshGroupTranslateChildrenCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/inspector/apply_node.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::inspector::apply_node::OffsetXCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/inspector/apply_node.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::inspector::apply_node::OffsetYCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/inspector/apply_node.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::inspector::apply_node::ViewportOriginCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/model/set_deform_binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::model::set_deform_binding::SetDeformBindingCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/model/set_deform_binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::model::set_deform_binding::SetRotationXBindingCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/model/set_deform_binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::model::set_deform_binding::SetRotationYBindingCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/model/set_deform_binding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::model::set_deform_binding::SetTRSBindingCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/node/mask.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::node::mask::AddMaskCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/node/mask.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::node::mask::ChangeMaskModeCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/node/mask.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::node::mask::RemoveMaskCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/node/simplephysics.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::node::simplephysics::ClearSimplePhysicsParameterCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/node/simplephysics.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::node::simplephysics::SetSimplePhysicsLocalOnlyCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/node/simplephysics.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::node::simplephysics::SetSimplePhysicsMapModeCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/node/simplephysics.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::node::simplephysics::SetSimplePhysicsModelTypeCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/node/simplephysics.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::node::simplephysics::SetSimplePhysicsParameterCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/node/welding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::node::welding::AddWeldingCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/node/welding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::node::welding::ChangeWeldingWeightCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/node/welding.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::node::welding::RemoveWeldingCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/group.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::group::ChangeGroupColorCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/group.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::group::CreateParamGroupCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CreateResult!(ExParameterGroup)"); info.isResource = true; info.resourceType = staticText("ExParameterGroup"); info.resourceChange = staticText("Created"); }
};
// Source: source/nijigenerate/commands/parameter/group.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::group::DeleteParamGroupCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("DeleteResult!(ExParameterGroup)"); info.isResource = true; info.resourceType = staticText("ExParameterGroup"); info.resourceChange = staticText("Deleted"); }
};
// Source: source/nijigenerate/commands/parameter/group.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::group::MoveParameterCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/param.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::param::Add1DParameterCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CreateResult!(Parameter)"); info.isResource = true; info.resourceType = staticText("Parameter"); info.resourceChange = staticText("Created"); }
};
// Source: source/nijigenerate/commands/parameter/param.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::param::Add2DParameterCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CreateResult!(Parameter)"); info.isResource = true; info.resourceType = staticText("Parameter"); info.resourceChange = staticText("Created"); }
};
// Source: source/nijigenerate/commands/parameter/param.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::param::AddMouthParameterCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CreateResult!(Parameter)"); info.isResource = true; info.resourceType = staticText("Parameter"); info.resourceChange = staticText("Created"); }
};
// Source: source/nijigenerate/commands/parameter/param.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::param::RemoveParameterCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("DeleteResult!(Parameter)"); info.isResource = true; info.resourceType = staticText("Parameter"); info.resourceChange = staticText("Deleted"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::ConvertTo2DParamCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::CopyParameterCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::DeleteParameterCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("DeleteResult!(Parameter)"); info.isResource = true; info.resourceType = staticText("Parameter"); info.resourceChange = staticText("Deleted"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::DuplicateParameterCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CreateResult!(Parameter)"); info.isResource = true; info.resourceType = staticText("Parameter"); info.resourceChange = staticText("Created"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::DuplicateParameterWithFlipCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CreateResult!(Parameter)"); info.isResource = true; info.resourceType = staticText("Parameter"); info.resourceChange = staticText("Created"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::Flip1DCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::FlipXCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::FlipYCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::LinkToCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::MirrorHorizontallyCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::MirrorVerticallyCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::MirroredAutoFillDir1Command> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::MirroredAutoFillDir2Command> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::MirroredAutoFillDir3Command> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::MirroredAutoFillDir4Command> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::PasteParameterCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::PasteParameterWithFlipCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::SetArmedParameterAndKeypointCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::SetParameterKeypointCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::SetStartingKeyFrameCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/paramedit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::paramedit::ToggleParameterArmCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/prop.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::prop::ApplyParameterPropsAxesCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/parameter/prop.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::parameter::prop::SetParameterNameCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/puppet/edit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::puppet::edit::RedoCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/puppet/edit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::puppet::edit::ShowCommandBrowserWindowCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/puppet/edit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::puppet::edit::ShowSettingsWindowCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/puppet/edit.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::puppet::edit::UndoCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/puppet/view.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::puppet::view::SetDefaultLayoutCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
template<> struct BrowserSourceResultTraits<nijigenerate::commands::puppet::view::ShowSaveScreenshotDialogCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
template<> struct BrowserSourceResultTraits<nijigenerate::commands::puppet::view::SaveScreenshotCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
template<> struct BrowserSourceResultTraits<nijigenerate::commands::puppet::view::CaptureLiveScreenshotCommand> {
    static void apply(CommandInfo& info) {
        info.outputType = staticText("ExCommandResultImpl!(JSONValue)");
        info.isExResult = true; info.exResultType = staticText("JSONValue");
    }
};
template<> struct BrowserSourceResultTraits<nijigenerate::commands::puppet::view::ShowStatusForNerdsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
template<> struct BrowserSourceResultTraits<nijigenerate::commands::puppet::view::ToggleDifferenceAggregationCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/vertex/define_mesh.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::vertex::define_mesh::DefineGridCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/vertex/define_mesh.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::vertex::define_mesh::DefineMeshCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/vertex/define_mesh.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::vertex::define_mesh::DefineVerticesCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::AddFlipPairCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::AutoAddFlipPairsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::FitViewportToModelCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::ListFlipPairsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("ExCommandResultImpl!(JSONValue)"); info.isExResult = true; info.exResultType = staticText("JSONValue"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::OpenAutomeshBatchingCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::OpenFlipPairWindowCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::RemoveFlipPairCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::ResetParametersCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::ResetPhysicsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::ResetViewportPositionCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::ResetViewportZoomCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::ToggleMirrorViewCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::ToggleOnionSliceCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::TogglePhysicsCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/control.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::control::TogglePostProcessCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
// Source: source/nijigenerate/commands/viewport/palette.d
template<> struct BrowserSourceResultTraits<nijigenerate::commands::viewport::palette::ListCommandCommand> {
    static void apply(CommandInfo& info) { info.outputType = staticText("CommandResult"); }
};
}
