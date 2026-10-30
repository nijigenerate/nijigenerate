#pragma once
#include "all_maps.hpp"
#include "node/dynamic.hpp"
#include <stdexcept>
#include <compat/object_metadata.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/commands/automesh/config.hpp>
#include <nijigenerate/commands/binding/binding_commands.hpp>
#include <nijigenerate/commands/command_scope_command.hpp>
#include <nijigenerate/commands/depth/bone_commands.hpp>
#include <nijigenerate/commands/depth/editor_commands.hpp>
#include <nijigenerate/commands/depth/map_commands.hpp>
#include <nijigenerate/commands/depth/psd_dialog_commands.hpp>
#include <nijigenerate/commands/inspector/camera_command.hpp>
#include <nijigenerate/commands/inspector/composite_commands.hpp>
#include <nijigenerate/commands/inspector/drawable_commands.hpp>
#include <nijigenerate/commands/inspector/griddeform_commands.hpp>
#include <nijigenerate/commands/inspector/meshgroup_commands.hpp>
#include <nijigenerate/commands/inspector/node_commands.hpp>
#include <nijigenerate/commands/inspector/part_commands.hpp>
#include <nijigenerate/commands/inspector/path_commands.hpp>
#include <nijigenerate/commands/inspector/simplephysics_commands.hpp>
#include <nijigenerate/commands/model/set_deform_binding_commands.hpp>
#include <nijigenerate/commands/node/mask_commands.hpp>
#include <nijigenerate/commands/node/node_commands.hpp>
#include <nijigenerate/commands/node/simplephysics_complete.hpp>
#include <nijigenerate/commands/node/simplephysics_config_commands.hpp>
#include <nijigenerate/commands/node/simplephysics_parameter_commands.hpp>
#include <nijigenerate/commands/node/welding_commands.hpp>
#include <nijigenerate/commands/parameter/animedit_commands.hpp>
#include <nijigenerate/commands/parameter/group_commands.hpp>
#include <nijigenerate/commands/parameter/param_commands.hpp>
#include <nijigenerate/commands/parameter/paramedit_commands.hpp>
#include <nijigenerate/commands/parameter/prop_commands.hpp>
#include <nijigenerate/commands/puppet/edit_commands.hpp>
#include <nijigenerate/commands/puppet/file_commands.hpp>
#include <nijigenerate/commands/puppet/tool.hpp>
#include <nijigenerate/commands/puppet/view_simple_commands.hpp>
#include <nijigenerate/commands/vertex/define_mesh.hpp>
#include <nijigenerate/commands/viewport/control_commands.hpp>
#include <nijigenerate/commands/viewport/control_pair_commands.hpp>
#include <nijigenerate/commands/viewport/palette_command.hpp>
namespace nijigenerate::commands {
// One source enum-id to concrete-class relation serves static and dynamic dispatch.
template<auto Id> struct CommandClassFor;
template<depth::bone::DepthBoneCommand Id> struct CommandClassFor<Id> {
    using type = typename depth::bone::DepthBoneCommandType<Id>::Type;
};
template<> struct CommandClassFor<CommandScopeCommand::GetCurrentCommandScope> { using type = nijigenerate::commands::GetCurrentCommandScopeCommand; };
template<> struct CommandClassFor<binding::BindingCommand::UnsetKeyFrame> { using type = nijigenerate::commands::binding::UnsetKeyFrameCommand; };
template<> struct CommandClassFor<binding::BindingCommand::SetKeyFrame> { using type = nijigenerate::commands::binding::SetKeyFrameCommand; };
template<> struct CommandClassFor<binding::BindingCommand::ResetKeyFrame> { using type = nijigenerate::commands::binding::ResetKeyFrameCommand; };
template<> struct CommandClassFor<binding::BindingCommand::InvertKeyFrame> { using type = nijigenerate::commands::binding::InvertKeyFrameCommand; };
template<> struct CommandClassFor<binding::BindingCommand::MirrorKeyFrameHorizontally> { using type = nijigenerate::commands::binding::MirrorKeyFrameHorizontallyCommand; };
template<> struct CommandClassFor<binding::BindingCommand::MirrorKeyFrameVertically> { using type = nijigenerate::commands::binding::MirrorKeyFrameVerticallyCommand; };
template<> struct CommandClassFor<binding::BindingCommand::FlipDeform> { using type = nijigenerate::commands::binding::FlipDeformCommand; };
template<> struct CommandClassFor<binding::BindingCommand::SymmetrizeDeform> { using type = nijigenerate::commands::binding::SymmetrizeDeformCommand; };
template<> struct CommandClassFor<binding::BindingCommand::SetFromHorizontalMirror> { using type = nijigenerate::commands::binding::SetFromHorizontalMirrorCommand; };
template<> struct CommandClassFor<binding::BindingCommand::SetFromVerticalMirror> { using type = nijigenerate::commands::binding::SetFromVerticalMirrorCommand; };
template<> struct CommandClassFor<binding::BindingCommand::SetFromDiagonalMirror> { using type = nijigenerate::commands::binding::SetFromDiagonalMirrorCommand; };
template<> struct CommandClassFor<binding::BindingCommand::SetFrom1DMirror> { using type = nijigenerate::commands::binding::SetFrom1DMirrorCommand; };
template<> struct CommandClassFor<binding::BindingCommand::CopyBinding> { using type = nijigenerate::commands::binding::CopyBindingCommand; };
template<> struct CommandClassFor<binding::BindingCommand::PasteBinding> { using type = nijigenerate::commands::binding::PasteBindingCommand; };
template<> struct CommandClassFor<binding::BindingCommand::RemoveBinding> { using type = nijigenerate::commands::binding::RemoveBindingCommand; };
template<> struct CommandClassFor<binding::BindingCommand::SetInterpolation> { using type = nijigenerate::commands::binding::SetInterpolationCommand; };
template<> struct CommandClassFor<node::node::NodeCommand::AddNode> { using type = node::node::AddNodeCommand; };
template<> struct CommandClassFor<node::node::NodeCommand::InsertNode> { using type = node::node::InsertNodeCommand; };
template<> struct CommandClassFor<node::node::NodeCommand::SetNodeName> { using type = nijigenerate::commands::node::node::SetNodeNameCommand; };
template<> struct CommandClassFor<node::node::NodeCommand::DeleteNode> { using type = nijigenerate::commands::node::node::DeleteNodeCommand; };
template<> struct CommandClassFor<node::node::NodeCommand::MoveNode> { using type = nijigenerate::commands::node::node::MoveNodeCommand; };
template<> struct CommandClassFor<node::node::NodeCommand::ConvertTo> { using type = node::node::ConvertToCommand; };
template<> struct CommandClassFor<node::node::NodeCommand::CutNode> { using type = nijigenerate::commands::node::node::CutNodeCommand; };
template<> struct CommandClassFor<node::node::NodeCommand::CopyNode> { using type = nijigenerate::commands::node::node::CopyNodeCommand; };
template<> struct CommandClassFor<node::node::NodeCommand::PasteNode> { using type = nijigenerate::commands::node::node::PasteNodeCommand; };
template<> struct CommandClassFor<node::node::NodeCommand::ReloadNode> { using type = nijigenerate::commands::node::node::ReloadNodeCommand; };
template<> struct CommandClassFor<node::node::NodeCommand::VertexMode> { using type = nijigenerate::commands::node::node::VertexModeCommand; };
template<> struct CommandClassFor<node::node::NodeCommand::ToggleVisibility> { using type = nijigenerate::commands::node::node::ToggleVisibilityCommand; };
template<> struct CommandClassFor<node::node::NodeCommand::CentralizeNode> { using type = nijigenerate::commands::node::node::CentralizeNodeCommand; };
template<> struct CommandClassFor<parameter::animedit::AnimeditCommand::AddAnimationKeyFrame> { using type = nijigenerate::commands::parameter::animedit::AddAnimationKeyFrameCommand; };
template<> struct CommandClassFor<parameter::animedit::AnimeditCommand::CopyAnimationKeyFrame> { using type = nijigenerate::commands::parameter::animedit::CopyAnimationKeyFrameCommand; };
template<> struct CommandClassFor<parameter::animedit::AnimeditCommand::PasteAnimationKeyFrame> { using type = nijigenerate::commands::parameter::animedit::PasteAnimationKeyFrameCommand; };
template<> struct CommandClassFor<parameter::group::GroupCommand::MoveParameter> { using type = nijigenerate::commands::parameter::group::MoveParameterCommand; };
template<> struct CommandClassFor<parameter::group::GroupCommand::CreateParamGroup> { using type = nijigenerate::commands::parameter::group::CreateParamGroupCommand; };
template<> struct CommandClassFor<parameter::group::GroupCommand::ChangeGroupColor> { using type = nijigenerate::commands::parameter::group::ChangeGroupColorCommand; };
template<> struct CommandClassFor<parameter::group::GroupCommand::DeleteParamGroup> { using type = nijigenerate::commands::parameter::group::DeleteParamGroupCommand; };
template<> struct CommandClassFor<parameter::param::ParamCommand::Add1DParameter> { using type = nijigenerate::commands::parameter::param::Add1DParameterCommand; };
template<> struct CommandClassFor<parameter::param::ParamCommand::Add2DParameter> { using type = nijigenerate::commands::parameter::param::Add2DParameterCommand; };
template<> struct CommandClassFor<parameter::param::ParamCommand::AddMouthParameter> { using type = nijigenerate::commands::parameter::param::AddMouthParameterCommand; };
template<> struct CommandClassFor<parameter::param::ParamCommand::RemoveParameter> { using type = nijigenerate::commands::parameter::param::RemoveParameterCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::ConvertTo2DParam> { using type = nijigenerate::commands::parameter::paramedit::ConvertTo2DParamCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::FlipX> { using type = nijigenerate::commands::parameter::paramedit::FlipXCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::FlipY> { using type = nijigenerate::commands::parameter::paramedit::FlipYCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::Flip1D> { using type = nijigenerate::commands::parameter::paramedit::Flip1DCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::MirrorHorizontally> { using type = nijigenerate::commands::parameter::paramedit::MirrorHorizontallyCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::MirrorVertically> { using type = nijigenerate::commands::parameter::paramedit::MirrorVerticallyCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::MirroredAutoFillDir1> { using type = nijigenerate::commands::parameter::paramedit::MirroredAutoFillDir1Command; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::MirroredAutoFillDir2> { using type = nijigenerate::commands::parameter::paramedit::MirroredAutoFillDir2Command; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::MirroredAutoFillDir3> { using type = nijigenerate::commands::parameter::paramedit::MirroredAutoFillDir3Command; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::MirroredAutoFillDir4> { using type = nijigenerate::commands::parameter::paramedit::MirroredAutoFillDir4Command; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::CopyParameter> { using type = nijigenerate::commands::parameter::paramedit::CopyParameterCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::PasteParameter> { using type = nijigenerate::commands::parameter::paramedit::PasteParameterCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::PasteParameterWithFlip> { using type = nijigenerate::commands::parameter::paramedit::PasteParameterWithFlipCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::DuplicateParameter> { using type = nijigenerate::commands::parameter::paramedit::DuplicateParameterCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::DuplicateParameterWithFlip> { using type = nijigenerate::commands::parameter::paramedit::DuplicateParameterWithFlipCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::DeleteParameter> { using type = nijigenerate::commands::parameter::paramedit::DeleteParameterCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::LinkTo> { using type = nijigenerate::commands::parameter::paramedit::LinkToCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::ToggleParameterArm> { using type = nijigenerate::commands::parameter::paramedit::ToggleParameterArmCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::SetParameterKeypoint> { using type = nijigenerate::commands::parameter::paramedit::SetParameterKeypointCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::SetArmedParameterAndKeypoint> { using type = nijigenerate::commands::parameter::paramedit::SetArmedParameterAndKeypointCommand; };
template<> struct CommandClassFor<parameter::paramedit::ParameditCommand::SetStartingKeyFrame> { using type = nijigenerate::commands::parameter::paramedit::SetStartingKeyFrameCommand; };
template<> struct CommandClassFor<parameter::prop::ParamPropCommand::SetParameterName> { using type = nijigenerate::commands::parameter::prop::SetParameterNameCommand; };
template<> struct CommandClassFor<parameter::prop::ParamPropCommand::ApplyParameterPropsAxes> { using type = nijigenerate::commands::parameter::prop::ApplyParameterPropsAxesCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::NewFile> { using type = nijigenerate::commands::puppet::file::NewFileCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowOpenFileDialog> { using type = nijigenerate::commands::puppet::file::ShowOpenFileDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::OpenFile> { using type = nijigenerate::commands::puppet::file::OpenFileCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowSaveFileDialog> { using type = nijigenerate::commands::puppet::file::ShowSaveFileDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::SaveFile> { using type = nijigenerate::commands::puppet::file::SaveFileCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowSaveFileAsDialog> { using type = nijigenerate::commands::puppet::file::ShowSaveFileAsDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowImportPSDDialog> { using type = nijigenerate::commands::puppet::file::ShowImportPSDDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowImportPSDDepthMapDialog> { using type = nijigenerate::commands::puppet::file::ShowImportPSDDepthMapDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::OpenPSDDepthMapDialog> { using type = nijigenerate::commands::puppet::file::OpenPSDDepthMapDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ImportPSD> { using type = nijigenerate::commands::puppet::file::ImportPSDCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowImportKRADialog> { using type = nijigenerate::commands::puppet::file::ShowImportKRADialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ImportKRA> { using type = nijigenerate::commands::puppet::file::ImportKRACommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowImportINPDialog> { using type = nijigenerate::commands::puppet::file::ShowImportINPDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ImportINP> { using type = nijigenerate::commands::puppet::file::ImportINPCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowImportImageFolderDialog> { using type = nijigenerate::commands::puppet::file::ShowImportImageFolderDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ImportImageFolder> { using type = nijigenerate::commands::puppet::file::ImportImageFolderCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowMergePSDDialog> { using type = nijigenerate::commands::puppet::file::ShowMergePSDDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::MergePSD> { using type = nijigenerate::commands::puppet::file::MergePSDCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowMergeKRADialog> { using type = nijigenerate::commands::puppet::file::ShowMergeKRADialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::MergeKRA> { using type = nijigenerate::commands::puppet::file::MergeKRACommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowMergeImageFileDialog> { using type = nijigenerate::commands::puppet::file::ShowMergeImageFileDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::MergeImageFiles> { using type = nijigenerate::commands::puppet::file::MergeImageFilesCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowMergeINPDialog> { using type = nijigenerate::commands::puppet::file::ShowMergeINPDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::MergeINP> { using type = nijigenerate::commands::puppet::file::MergeINPCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowExportToINPDialog> { using type = nijigenerate::commands::puppet::file::ShowExportToINPDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ExportINP> { using type = nijigenerate::commands::puppet::file::ExportINPCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowExportToPNGDialog> { using type = nijigenerate::commands::puppet::file::ShowExportToPNGDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ExportPNG> { using type = nijigenerate::commands::puppet::file::ExportPNGCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowExportToJpegDialog> { using type = nijigenerate::commands::puppet::file::ShowExportToJpegDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ExportJPEG> { using type = nijigenerate::commands::puppet::file::ExportJPEGCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowExportToTGADialog> { using type = nijigenerate::commands::puppet::file::ShowExportToTGADialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ExportTGA> { using type = nijigenerate::commands::puppet::file::ExportTGACommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ShowExportToVideoDialog> { using type = nijigenerate::commands::puppet::file::ShowExportToVideoDialogCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::ExportVideo> { using type = nijigenerate::commands::puppet::file::ExportVideoCommand; };
template<> struct CommandClassFor<puppet::file::FileCommand::CloseProject> { using type = nijigenerate::commands::puppet::file::CloseProjectCommand; };
template<> struct CommandClassFor<puppet::edit::EditCommand::Undo> { using type = nijigenerate::commands::puppet::edit::UndoCommand; };
template<> struct CommandClassFor<puppet::edit::EditCommand::Redo> { using type = nijigenerate::commands::puppet::edit::RedoCommand; };
template<> struct CommandClassFor<puppet::edit::EditCommand::ShowSettingsWindow> { using type = nijigenerate::commands::puppet::edit::ShowSettingsWindowCommand; };
template<> struct CommandClassFor<puppet::edit::EditCommand::ShowCommandBrowserWindow> { using type = nijigenerate::commands::puppet::edit::ShowCommandBrowserWindowCommand; };
template<> struct CommandClassFor<puppet::view::ViewCommand::SetDefaultLayout> { using type = nijigenerate::commands::puppet::view::SetDefaultLayoutCommand; };
template<> struct CommandClassFor<puppet::view::ViewCommand::ShowSaveScreenshotDialog> { using type = nijigenerate::commands::puppet::view::ShowSaveScreenshotDialogCommand; };
template<> struct CommandClassFor<puppet::view::ViewCommand::SaveScreenshot> { using type = nijigenerate::commands::puppet::view::SaveScreenshotCommand; };
template<> struct CommandClassFor<puppet::view::ViewCommand::CaptureLiveScreenshot> { using type = nijigenerate::commands::puppet::view::CaptureLiveScreenshotCommand; };
template<> struct CommandClassFor<puppet::view::ViewCommand::ShowStatusForNerds> { using type = nijigenerate::commands::puppet::view::ShowStatusForNerdsCommand; };
template<> struct CommandClassFor<puppet::view::ViewCommand::ToggleDifferenceAggregation> { using type = nijigenerate::commands::puppet::view::ToggleDifferenceAggregationCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::ShowImportSessionDataDialog> { using type = nijigenerate::commands::puppet::tool::ShowImportSessionDataDialogCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::ImportSessionData> { using type = nijigenerate::commands::puppet::tool::ImportSessionDataCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::PremultTexture> { using type = nijigenerate::commands::puppet::tool::PremultTextureCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::RebleedTexture> { using type = nijigenerate::commands::puppet::tool::RebleedTextureCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::RegenerateMipmaps> { using type = nijigenerate::commands::puppet::tool::RegenerateMipmapsCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::GenerateFakeLayerName> { using type = nijigenerate::commands::puppet::tool::GenerateFakeLayerNameCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::AttemptRepairPuppet> { using type = nijigenerate::commands::puppet::tool::AttemptRepairPuppetCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::RegenerateNodeIDs> { using type = nijigenerate::commands::puppet::tool::RegenerateNodeIDsCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::ModelEditMode> { using type = nijigenerate::commands::puppet::tool::ModelEditModeCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::AnimEditMode> { using type = nijigenerate::commands::puppet::tool::AnimEditModeCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::ExecuteAutoRig> { using type = nijigenerate::commands::puppet::tool::ExecuteAutoRigCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::GetAutoRigStatus> { using type = nijigenerate::commands::puppet::tool::GetAutoRigStatusCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::GetAutoRigMemoryStatus> { using type = nijigenerate::commands::puppet::tool::GetAutoRigMemoryStatusCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::DeleteAutoRigSession> { using type = nijigenerate::commands::puppet::tool::DeleteAutoRigSessionCommand; };
template<> struct CommandClassFor<puppet::tool::ToolCommand::ResumeAutoRig> { using type = nijigenerate::commands::puppet::tool::ResumeAutoRigCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::ToggleMirrorView> { using type = nijigenerate::commands::viewport::control::ToggleMirrorViewCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::ToggleOnionSlice> { using type = nijigenerate::commands::viewport::control::ToggleOnionSliceCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::TogglePhysics> { using type = nijigenerate::commands::viewport::control::TogglePhysicsCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::TogglePostProcess> { using type = nijigenerate::commands::viewport::control::TogglePostProcessCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::ResetPhysics> { using type = nijigenerate::commands::viewport::control::ResetPhysicsCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::ResetParameters> { using type = nijigenerate::commands::viewport::control::ResetParametersCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::ListFlipPairs> { using type = nijigenerate::commands::viewport::control::ListFlipPairsCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::AddFlipPair> { using type = nijigenerate::commands::viewport::control::AddFlipPairCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::AutoAddFlipPairs> { using type = nijigenerate::commands::viewport::control::AutoAddFlipPairsCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::RemoveFlipPair> { using type = nijigenerate::commands::viewport::control::RemoveFlipPairCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::OpenFlipPairWindow> { using type = nijigenerate::commands::viewport::control::OpenFlipPairWindowCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::OpenAutomeshBatching> { using type = nijigenerate::commands::viewport::control::OpenAutomeshBatchingCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::ResetViewportZoom> { using type = nijigenerate::commands::viewport::control::ResetViewportZoomCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::ResetViewportPosition> { using type = nijigenerate::commands::viewport::control::ResetViewportPositionCommand; };
template<> struct CommandClassFor<viewport::control::ViewportCommand::FitViewportToModel> { using type = nijigenerate::commands::viewport::control::FitViewportToModelCommand; };
template<> struct CommandClassFor<viewport::palette::PaletteCommand::ListCommand> { using type = nijigenerate::commands::viewport::palette::ListCommandCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TranslationX> { using type = nijigenerate::commands::inspector::apply_node::TranslationXCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TranslationY> { using type = nijigenerate::commands::inspector::apply_node::TranslationYCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TranslationZ> { using type = nijigenerate::commands::inspector::apply_node::TranslationZCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::RotationX> { using type = nijigenerate::commands::inspector::apply_node::RotationXCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::RotationY> { using type = nijigenerate::commands::inspector::apply_node::RotationYCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::RotationZ> { using type = nijigenerate::commands::inspector::apply_node::RotationZCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ScaleX> { using type = nijigenerate::commands::inspector::apply_node::ScaleXCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ScaleY> { using type = nijigenerate::commands::inspector::apply_node::ScaleYCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PixelSnap> { using type = nijigenerate::commands::inspector::apply_node::PixelSnapCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TogglePixelSnap> { using type = nijigenerate::commands::inspector::apply_node::TogglePixelSnapCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ZSort> { using type = nijigenerate::commands::inspector::apply_node::ZSortCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::LockToRoot> { using type = nijigenerate::commands::inspector::apply_node::LockToRootCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ToggleLockToRoot> { using type = nijigenerate::commands::inspector::apply_node::ToggleLockToRootCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PinToMesh> { using type = nijigenerate::commands::inspector::apply_node::PinToMeshCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TogglePinToMesh> { using type = nijigenerate::commands::inspector::apply_node::TogglePinToMeshCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::OffsetX> { using type = nijigenerate::commands::inspector::apply_node::OffsetXCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::OffsetY> { using type = nijigenerate::commands::inspector::apply_node::OffsetYCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ViewportOrigin> { using type = nijigenerate::commands::inspector::apply_node::ViewportOriginCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::CompositeTint> { using type = nijigenerate::commands::inspector::apply_node::CompositeTintCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::CompositeScreenTint> { using type = nijigenerate::commands::inspector::apply_node::CompositeScreenTintCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::CompositeBlendingMode> { using type = nijigenerate::commands::inspector::apply_node::CompositeBlendingModeCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::CompositeOpacity> { using type = nijigenerate::commands::inspector::apply_node::CompositeOpacityCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::CompositeThreshold> { using type = nijigenerate::commands::inspector::apply_node::CompositeThresholdCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartTint> { using type = nijigenerate::commands::inspector::apply_node::PartTintCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartScreenTint> { using type = nijigenerate::commands::inspector::apply_node::PartScreenTintCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartEmissionStrength> { using type = nijigenerate::commands::inspector::apply_node::PartEmissionStrengthCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartBlendingMode> { using type = nijigenerate::commands::inspector::apply_node::PartBlendingModeCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartOpacity> { using type = nijigenerate::commands::inspector::apply_node::PartOpacityCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartMaskAlphaThreshold> { using type = nijigenerate::commands::inspector::apply_node::PartMaskAlphaThresholdCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartAutoResizedMesh> { using type = nijigenerate::commands::inspector::apply_node::PartAutoResizedMeshCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TogglePartAutoResizedMesh> { using type = nijigenerate::commands::inspector::apply_node::TogglePartAutoResizedMeshCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::MeshGroupDynamic> { using type = nijigenerate::commands::inspector::apply_node::MeshGroupDynamicCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ToggleMeshGroupDynamic> { using type = nijigenerate::commands::inspector::apply_node::ToggleMeshGroupDynamicCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::MeshGroupTranslateChildren> { using type = nijigenerate::commands::inspector::apply_node::MeshGroupTranslateChildrenCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ToggleMeshGroupTranslateChildren> { using type = nijigenerate::commands::inspector::apply_node::ToggleMeshGroupTranslateChildrenCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::GridDeformDynamic> { using type = nijigenerate::commands::inspector::apply_node::GridDeformDynamicCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ToggleGridDeformDynamic> { using type = nijigenerate::commands::inspector::apply_node::ToggleGridDeformDynamicCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformDynamic> { using type = nijigenerate::commands::inspector::apply_node::PathDeformDynamicCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TogglePathDeformDynamic> { using type = nijigenerate::commands::inspector::apply_node::TogglePathDeformDynamicCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformPhysicsEnabled> { using type = nijigenerate::commands::inspector::apply_node::PathDeformPhysicsEnabledCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TogglePathDeformPhysicsEnabled> { using type = nijigenerate::commands::inspector::apply_node::TogglePathDeformPhysicsEnabledCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformCurveType> { using type = nijigenerate::commands::inspector::apply_node::PathDeformCurveTypeCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformGravity> { using type = nijigenerate::commands::inspector::apply_node::PathDeformGravityCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformRestoreConstant> { using type = nijigenerate::commands::inspector::apply_node::PathDeformRestoreConstantCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformDamping> { using type = nijigenerate::commands::inspector::apply_node::PathDeformDampingCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformInputScale> { using type = nijigenerate::commands::inspector::apply_node::PathDeformInputScaleCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformPropagateScale> { using type = nijigenerate::commands::inspector::apply_node::PathDeformPropagateScaleCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsModelType> { using type = nijigenerate::commands::inspector::apply_node::SimplePhysicsModelTypeCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsMapMode> { using type = nijigenerate::commands::inspector::apply_node::SimplePhysicsMapModeCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsLocalOnly> { using type = nijigenerate::commands::inspector::apply_node::SimplePhysicsLocalOnlyCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ToggleSimplePhysicsLocalOnly> { using type = nijigenerate::commands::inspector::apply_node::ToggleSimplePhysicsLocalOnlyCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsGravity> { using type = nijigenerate::commands::inspector::apply_node::SimplePhysicsGravityCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsLength> { using type = nijigenerate::commands::inspector::apply_node::SimplePhysicsLengthCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsFrequency> { using type = nijigenerate::commands::inspector::apply_node::SimplePhysicsFrequencyCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsAngleDamping> { using type = nijigenerate::commands::inspector::apply_node::SimplePhysicsAngleDampingCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsLengthDamping> { using type = nijigenerate::commands::inspector::apply_node::SimplePhysicsLengthDampingCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsOutputScaleX> { using type = nijigenerate::commands::inspector::apply_node::SimplePhysicsOutputScaleXCommand; };
template<> struct CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsOutputScaleY> { using type = nijigenerate::commands::inspector::apply_node::SimplePhysicsOutputScaleYCommand; };
template<> struct CommandClassFor<node::mask::NodeMaskCommand::AddMask> { using type = nijigenerate::commands::node::mask::AddMaskCommand; };
template<> struct CommandClassFor<node::mask::NodeMaskCommand::RemoveMask> { using type = nijigenerate::commands::node::mask::RemoveMaskCommand; };
template<> struct CommandClassFor<node::mask::NodeMaskCommand::ChangeMaskMode> { using type = nijigenerate::commands::node::mask::ChangeMaskModeCommand; };
template<> struct CommandClassFor<node::welding::NodeWeldingCommand::AddWelding> { using type = nijigenerate::commands::node::welding::AddWeldingCommand; };
template<> struct CommandClassFor<node::welding::NodeWeldingCommand::RemoveWelding> { using type = nijigenerate::commands::node::welding::RemoveWeldingCommand; };
template<> struct CommandClassFor<node::welding::NodeWeldingCommand::ChangeWeldingWeight> { using type = nijigenerate::commands::node::welding::ChangeWeldingWeightCommand; };
template<> struct CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsParameter> { using type = nijigenerate::commands::node::simplephysics::SetSimplePhysicsParameterCommand; };
template<> struct CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::ClearSimplePhysicsParameter> { using type = nijigenerate::commands::node::simplephysics::ClearSimplePhysicsParameterCommand; };
template<> struct CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsModelType> { using type = nijigenerate::commands::node::simplephysics::SetSimplePhysicsModelTypeCommand; };
template<> struct CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsMapMode> { using type = nijigenerate::commands::node::simplephysics::SetSimplePhysicsMapModeCommand; };
template<> struct CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsLocalOnly> { using type = nijigenerate::commands::node::simplephysics::SetSimplePhysicsLocalOnlyCommand; };
template<> struct CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsGravity> { using type = nijigenerate::commands::node::simplephysics::SetSimplePhysicsGravityCommand; };
template<> struct CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsLength> { using type = nijigenerate::commands::node::simplephysics::SetSimplePhysicsLengthCommand; };
template<> struct CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsFrequency> { using type = nijigenerate::commands::node::simplephysics::SetSimplePhysicsFrequencyCommand; };
template<> struct CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsAngleDamping> { using type = nijigenerate::commands::node::simplephysics::SetSimplePhysicsAngleDampingCommand; };
template<> struct CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsLengthDamping> { using type = nijigenerate::commands::node::simplephysics::SetSimplePhysicsLengthDampingCommand; };
template<> struct CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsOutputScaleX> { using type = nijigenerate::commands::node::simplephysics::SetSimplePhysicsOutputScaleXCommand; };
template<> struct CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsOutputScaleY> { using type = nijigenerate::commands::node::simplephysics::SetSimplePhysicsOutputScaleYCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshListProcessors> { using type = nijigenerate::commands::automesh::config::AutoMeshListProcessorsCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshGetActive> { using type = nijigenerate::commands::automesh::config::AutoMeshGetActiveCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshSetActive> { using type = nijigenerate::commands::automesh::config::AutoMeshSetActiveCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshApplyActive> { using type = nijigenerate::commands::automesh::config::AutoMeshApplyActiveCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshGetSchema> { using type = nijigenerate::commands::automesh::config::AutoMeshGetSchemaCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshGetValues> { using type = nijigenerate::commands::automesh::config::AutoMeshGetValuesCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshSetPreset> { using type = nijigenerate::commands::automesh::config::AutoMeshSetPresetCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshSetValues> { using type = nijigenerate::commands::automesh::config::AutoMeshSetValuesCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetSimple_OptimumAutoMeshProcessor> { using type = nijigenerate::commands::automesh::config::AutoMeshSetSimple_OptimumAutoMeshProcessorCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetAdvanced_OptimumAutoMeshProcessor> { using type = nijigenerate::commands::automesh::config::AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetPreset_OptimumAutoMeshProcessor> { using type = nijigenerate::commands::automesh::config::AutoMeshSetPreset_OptimumAutoMeshProcessorCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetSimple_ContourAutoMeshProcessor> { using type = nijigenerate::commands::automesh::config::AutoMeshSetSimple_ContourAutoMeshProcessorCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetAdvanced_ContourAutoMeshProcessor> { using type = nijigenerate::commands::automesh::config::AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetPreset_ContourAutoMeshProcessor> { using type = nijigenerate::commands::automesh::config::AutoMeshSetPreset_ContourAutoMeshProcessorCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetSimple_GridAutoMeshProcessor> { using type = nijigenerate::commands::automesh::config::AutoMeshSetSimple_GridAutoMeshProcessorCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetAdvanced_GridAutoMeshProcessor> { using type = nijigenerate::commands::automesh::config::AutoMeshSetAdvanced_GridAutoMeshProcessorCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetPreset_GridAutoMeshProcessor> { using type = nijigenerate::commands::automesh::config::AutoMeshSetPreset_GridAutoMeshProcessorCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetSimple_SkeletonExtractor> { using type = nijigenerate::commands::automesh::config::AutoMeshSetSimple_SkeletonExtractorCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetAdvanced_SkeletonExtractor> { using type = nijigenerate::commands::automesh::config::AutoMeshSetAdvanced_SkeletonExtractorCommand; };
template<> struct CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetPreset_SkeletonExtractor> { using type = nijigenerate::commands::automesh::config::AutoMeshSetPreset_SkeletonExtractorCommand; };
template<> struct CommandClassFor<vertex::define_mesh::VertexCommand::DefineMesh> { using type = nijigenerate::commands::vertex::define_mesh::DefineMeshCommand; };
template<> struct CommandClassFor<vertex::define_mesh::VertexCommand::DefineVertices> { using type = nijigenerate::commands::vertex::define_mesh::DefineVerticesCommand; };
template<> struct CommandClassFor<vertex::define_mesh::VertexCommand::DefineGrid> { using type = nijigenerate::commands::vertex::define_mesh::DefineGridCommand; };
template<> struct CommandClassFor<model::set_deform_binding::ModelCommand::SetDeformBinding> { using type = nijigenerate::commands::model::set_deform_binding::SetDeformBindingCommand; };
template<> struct CommandClassFor<model::set_deform_binding::ModelCommand::SetTRSBinding> { using type = nijigenerate::commands::model::set_deform_binding::SetTRSBindingCommand; };
template<> struct CommandClassFor<model::set_deform_binding::ModelCommand::SetRotationYBinding> { using type = nijigenerate::commands::model::set_deform_binding::SetRotationYBindingCommand; };
template<> struct CommandClassFor<model::set_deform_binding::ModelCommand::SetRotationXBinding> { using type = nijigenerate::commands::model::set_deform_binding::SetRotationXBindingCommand; };
template<> struct CommandClassFor<depth::bone::DepthBoneCommand::AddStandardDepthParameters> { using type = nijigenerate::commands::depth::bone::AddStandardDepthParametersCommand; };
template<> struct CommandClassFor<depth::editor::DepthEditorOperationCommand::AddEditorDepthOp> { using type = nijigenerate::commands::depth::editor::AddEditorDepthOpCommand; };
template<> struct CommandClassFor<depth::editor::DepthEditorOperationCommand::UpdateEditorDepthOp> { using type = nijigenerate::commands::depth::editor::UpdateEditorDepthOpCommand; };
template<> struct CommandClassFor<depth::editor::DepthEditorOperationCommand::RemoveEditorDepthOp> { using type = nijigenerate::commands::depth::editor::RemoveEditorDepthOpCommand; };
template<> struct CommandClassFor<depth::map::DepthMapCommand::ListDepths> { using type = nijigenerate::commands::depth::map::ListDepthsCommand; };
template<> struct CommandClassFor<depth::map::DepthMapCommand::SetDepths> { using type = nijigenerate::commands::depth::map::SetDepthsCommand; };
template<> struct CommandClassFor<depth::map::DepthMapCommand::ClearDepths> { using type = nijigenerate::commands::depth::map::ClearDepthsCommand; };
template<> struct CommandClassFor<depth::map::DepthMapCommand::ListDepthOps> { using type = nijigenerate::commands::depth::map::ListDepthOpsCommand; };
template<> struct CommandClassFor<depth::map::DepthMapCommand::SetDepthOps> { using type = nijigenerate::commands::depth::map::SetDepthOpsCommand; };
template<> struct CommandClassFor<depth::map::DepthMapCommand::AddDepthOp> { using type = nijigenerate::commands::depth::map::AddDepthOpCommand; };
template<> struct CommandClassFor<depth::map::DepthMapCommand::UpdateDepthOp> { using type = nijigenerate::commands::depth::map::UpdateDepthOpCommand; };
template<> struct CommandClassFor<depth::map::DepthMapCommand::RemoveDepthOp> { using type = nijigenerate::commands::depth::map::RemoveDepthOpCommand; };
template<> struct CommandClassFor<depth::map::DepthMapCommand::MoveDepthOp> { using type = nijigenerate::commands::depth::map::MoveDepthOpCommand; };
template<> struct CommandClassFor<depth::map::DepthMapCommand::ClearDepthOps> { using type = nijigenerate::commands::depth::map::ClearDepthOpsCommand; };
template<> struct CommandClassFor<depth::map::DepthMapCommand::ApplyDepthOps> { using type = nijigenerate::commands::depth::map::ApplyDepthOpsCommand; };
template<> struct CommandClassFor<depth::map::DepthMapCommand::ImportPSDDepths> { using type = nijigenerate::commands::depth::map::ImportPSDDepthsCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::InspectPsdDepthDialog> { using type = nijigenerate::commands::depth::psd_dialog::InspectPsdDepthDialogCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::GetPsdDepthDialogPartData> { using type = nijigenerate::commands::depth::psd_dialog::GetPsdDepthDialogPartDataCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogColorSource> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogColorSourceCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogInvert> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogInvertCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogBackDepth> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogBackDepthCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogFrontDepth> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogFrontDepthCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogDepthScale> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogDepthScaleCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogChannel> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogChannelCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogSampling> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogSamplingCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogCustomRadius> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogCustomRadiusCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogAlphaThreshold> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogAlphaThresholdCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogMissingPolicy> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogMissingPolicyCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogContourRepair> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogContourRepairCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogSurfaceSmoothing> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogSurfaceSmoothingCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::FillPsdDepthDialogAlphaDepthGaps> { using type = nijigenerate::commands::depth::psd_dialog::FillPsdDepthDialogAlphaDepthGapsCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogGpuComposition> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogGpuCompositionCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogDirectGridMatch> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogDirectGridMatchCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogProblemFilter> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogProblemFilterCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingAuto> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerMappingAutoCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingIgnored> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerMappingIgnoredCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingTarget> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerMappingTargetCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogTargetEnabled> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogTargetEnabledCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerEnabled> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerEnabledCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerVisible> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerVisibleCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthEnabled> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerDepthEnabledCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthInverted> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerDepthInvertedCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthScale> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerDepthScaleCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthOffset> { using type = nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerDepthOffsetCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::ResetPsdDepthDialogLayerDepthTransform> { using type = nijigenerate::commands::depth::psd_dialog::ResetPsdDepthDialogLayerDepthTransformCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::ApplyPsdDepthDialog> { using type = nijigenerate::commands::depth::psd_dialog::ApplyPsdDepthDialogCommand; };
template<> struct CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::CancelPsdDepthDialog> { using type = nijigenerate::commands::depth::psd_dialog::CancelPsdDepthDialogCommand; };

template<class K, class V> void ngVisitConcreteCommand(K key, std::shared_ptr<Command> value, V& visitor) {
    if constexpr (std::same_as<K, CommandScopeCommand>) {
        switch (key) {
        case CommandScopeCommand::GetCurrentCommandScope: visitor.template operator()<typename CommandClassFor<CommandScopeCommand::GetCurrentCommandScope>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, binding::BindingCommand>) {
        switch (key) {
        case binding::BindingCommand::UnsetKeyFrame: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::UnsetKeyFrame>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::SetKeyFrame: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::SetKeyFrame>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::ResetKeyFrame: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::ResetKeyFrame>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::InvertKeyFrame: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::InvertKeyFrame>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::MirrorKeyFrameHorizontally: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::MirrorKeyFrameHorizontally>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::MirrorKeyFrameVertically: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::MirrorKeyFrameVertically>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::FlipDeform: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::FlipDeform>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::SymmetrizeDeform: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::SymmetrizeDeform>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::SetFromHorizontalMirror: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::SetFromHorizontalMirror>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::SetFromVerticalMirror: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::SetFromVerticalMirror>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::SetFromDiagonalMirror: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::SetFromDiagonalMirror>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::SetFrom1DMirror: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::SetFrom1DMirror>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::CopyBinding: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::CopyBinding>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::PasteBinding: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::PasteBinding>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::RemoveBinding: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::RemoveBinding>::type>(key, value, ngCommandIdFromKey(key)); return;
        case binding::BindingCommand::SetInterpolation: visitor.template operator()<typename CommandClassFor<binding::BindingCommand::SetInterpolation>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, node::node::NodeCommand>) {
        switch (key) {
        case node::node::NodeCommand::AddNode: visitor.template operator()<typename CommandClassFor<node::node::NodeCommand::AddNode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::node::NodeCommand::InsertNode: visitor.template operator()<typename CommandClassFor<node::node::NodeCommand::InsertNode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::node::NodeCommand::SetNodeName: visitor.template operator()<typename CommandClassFor<node::node::NodeCommand::SetNodeName>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::node::NodeCommand::DeleteNode: visitor.template operator()<typename CommandClassFor<node::node::NodeCommand::DeleteNode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::node::NodeCommand::MoveNode: visitor.template operator()<typename CommandClassFor<node::node::NodeCommand::MoveNode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::node::NodeCommand::ConvertTo: visitor.template operator()<typename CommandClassFor<node::node::NodeCommand::ConvertTo>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::node::NodeCommand::CutNode: visitor.template operator()<typename CommandClassFor<node::node::NodeCommand::CutNode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::node::NodeCommand::CopyNode: visitor.template operator()<typename CommandClassFor<node::node::NodeCommand::CopyNode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::node::NodeCommand::PasteNode: visitor.template operator()<typename CommandClassFor<node::node::NodeCommand::PasteNode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::node::NodeCommand::ReloadNode: visitor.template operator()<typename CommandClassFor<node::node::NodeCommand::ReloadNode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::node::NodeCommand::VertexMode: visitor.template operator()<typename CommandClassFor<node::node::NodeCommand::VertexMode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::node::NodeCommand::ToggleVisibility: visitor.template operator()<typename CommandClassFor<node::node::NodeCommand::ToggleVisibility>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::node::NodeCommand::CentralizeNode: visitor.template operator()<typename CommandClassFor<node::node::NodeCommand::CentralizeNode>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, parameter::animedit::AnimeditCommand>) {
        switch (key) {
        case parameter::animedit::AnimeditCommand::AddAnimationKeyFrame: visitor.template operator()<typename CommandClassFor<parameter::animedit::AnimeditCommand::AddAnimationKeyFrame>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::animedit::AnimeditCommand::CopyAnimationKeyFrame: visitor.template operator()<typename CommandClassFor<parameter::animedit::AnimeditCommand::CopyAnimationKeyFrame>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::animedit::AnimeditCommand::PasteAnimationKeyFrame: visitor.template operator()<typename CommandClassFor<parameter::animedit::AnimeditCommand::PasteAnimationKeyFrame>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, parameter::group::GroupCommand>) {
        switch (key) {
        case parameter::group::GroupCommand::MoveParameter: visitor.template operator()<typename CommandClassFor<parameter::group::GroupCommand::MoveParameter>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::group::GroupCommand::CreateParamGroup: visitor.template operator()<typename CommandClassFor<parameter::group::GroupCommand::CreateParamGroup>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::group::GroupCommand::ChangeGroupColor: visitor.template operator()<typename CommandClassFor<parameter::group::GroupCommand::ChangeGroupColor>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::group::GroupCommand::DeleteParamGroup: visitor.template operator()<typename CommandClassFor<parameter::group::GroupCommand::DeleteParamGroup>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, parameter::param::ParamCommand>) {
        switch (key) {
        case parameter::param::ParamCommand::Add1DParameter: visitor.template operator()<typename CommandClassFor<parameter::param::ParamCommand::Add1DParameter>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::param::ParamCommand::Add2DParameter: visitor.template operator()<typename CommandClassFor<parameter::param::ParamCommand::Add2DParameter>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::param::ParamCommand::AddMouthParameter: visitor.template operator()<typename CommandClassFor<parameter::param::ParamCommand::AddMouthParameter>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::param::ParamCommand::RemoveParameter: visitor.template operator()<typename CommandClassFor<parameter::param::ParamCommand::RemoveParameter>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, parameter::paramedit::ParameditCommand>) {
        switch (key) {
        case parameter::paramedit::ParameditCommand::ConvertTo2DParam: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::ConvertTo2DParam>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::FlipX: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::FlipX>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::FlipY: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::FlipY>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::Flip1D: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::Flip1D>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::MirrorHorizontally: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::MirrorHorizontally>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::MirrorVertically: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::MirrorVertically>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::MirroredAutoFillDir1: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::MirroredAutoFillDir1>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::MirroredAutoFillDir2: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::MirroredAutoFillDir2>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::MirroredAutoFillDir3: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::MirroredAutoFillDir3>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::MirroredAutoFillDir4: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::MirroredAutoFillDir4>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::CopyParameter: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::CopyParameter>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::PasteParameter: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::PasteParameter>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::PasteParameterWithFlip: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::PasteParameterWithFlip>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::DuplicateParameter: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::DuplicateParameter>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::DuplicateParameterWithFlip: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::DuplicateParameterWithFlip>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::DeleteParameter: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::DeleteParameter>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::LinkTo: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::LinkTo>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::ToggleParameterArm: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::ToggleParameterArm>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::SetParameterKeypoint: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::SetParameterKeypoint>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::SetArmedParameterAndKeypoint: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::SetArmedParameterAndKeypoint>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::paramedit::ParameditCommand::SetStartingKeyFrame: visitor.template operator()<typename CommandClassFor<parameter::paramedit::ParameditCommand::SetStartingKeyFrame>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, parameter::prop::ParamPropCommand>) {
        switch (key) {
        case parameter::prop::ParamPropCommand::SetParameterName: visitor.template operator()<typename CommandClassFor<parameter::prop::ParamPropCommand::SetParameterName>::type>(key, value, ngCommandIdFromKey(key)); return;
        case parameter::prop::ParamPropCommand::ApplyParameterPropsAxes: visitor.template operator()<typename CommandClassFor<parameter::prop::ParamPropCommand::ApplyParameterPropsAxes>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, puppet::file::FileCommand>) {
        switch (key) {
        case puppet::file::FileCommand::NewFile: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::NewFile>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowOpenFileDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowOpenFileDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::OpenFile: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::OpenFile>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowSaveFileDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowSaveFileDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::SaveFile: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::SaveFile>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowSaveFileAsDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowSaveFileAsDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowImportPSDDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowImportPSDDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowImportPSDDepthMapDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowImportPSDDepthMapDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::OpenPSDDepthMapDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::OpenPSDDepthMapDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ImportPSD: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ImportPSD>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowImportKRADialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowImportKRADialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ImportKRA: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ImportKRA>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowImportINPDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowImportINPDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ImportINP: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ImportINP>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowImportImageFolderDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowImportImageFolderDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ImportImageFolder: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ImportImageFolder>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowMergePSDDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowMergePSDDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::MergePSD: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::MergePSD>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowMergeKRADialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowMergeKRADialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::MergeKRA: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::MergeKRA>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowMergeImageFileDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowMergeImageFileDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::MergeImageFiles: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::MergeImageFiles>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowMergeINPDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowMergeINPDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::MergeINP: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::MergeINP>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowExportToINPDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowExportToINPDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ExportINP: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ExportINP>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowExportToPNGDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowExportToPNGDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ExportPNG: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ExportPNG>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowExportToJpegDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowExportToJpegDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ExportJPEG: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ExportJPEG>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowExportToTGADialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowExportToTGADialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ExportTGA: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ExportTGA>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ShowExportToVideoDialog: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ShowExportToVideoDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::ExportVideo: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::ExportVideo>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::file::FileCommand::CloseProject: visitor.template operator()<typename CommandClassFor<puppet::file::FileCommand::CloseProject>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, puppet::edit::EditCommand>) {
        switch (key) {
        case puppet::edit::EditCommand::Undo: visitor.template operator()<typename CommandClassFor<puppet::edit::EditCommand::Undo>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::edit::EditCommand::Redo: visitor.template operator()<typename CommandClassFor<puppet::edit::EditCommand::Redo>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::edit::EditCommand::ShowSettingsWindow: visitor.template operator()<typename CommandClassFor<puppet::edit::EditCommand::ShowSettingsWindow>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::edit::EditCommand::ShowCommandBrowserWindow: visitor.template operator()<typename CommandClassFor<puppet::edit::EditCommand::ShowCommandBrowserWindow>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, puppet::view::ViewCommand>) {
        switch (key) {
        case puppet::view::ViewCommand::SetDefaultLayout: visitor.template operator()<typename CommandClassFor<puppet::view::ViewCommand::SetDefaultLayout>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::view::ViewCommand::ShowSaveScreenshotDialog: visitor.template operator()<typename CommandClassFor<puppet::view::ViewCommand::ShowSaveScreenshotDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::view::ViewCommand::SaveScreenshot: visitor.template operator()<typename CommandClassFor<puppet::view::ViewCommand::SaveScreenshot>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::view::ViewCommand::CaptureLiveScreenshot: visitor.template operator()<typename CommandClassFor<puppet::view::ViewCommand::CaptureLiveScreenshot>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::view::ViewCommand::ShowStatusForNerds: visitor.template operator()<typename CommandClassFor<puppet::view::ViewCommand::ShowStatusForNerds>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::view::ViewCommand::ToggleDifferenceAggregation: visitor.template operator()<typename CommandClassFor<puppet::view::ViewCommand::ToggleDifferenceAggregation>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, puppet::tool::ToolCommand>) {
        switch (key) {
        case puppet::tool::ToolCommand::ShowImportSessionDataDialog: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::ShowImportSessionDataDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::ImportSessionData: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::ImportSessionData>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::PremultTexture: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::PremultTexture>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::RebleedTexture: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::RebleedTexture>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::RegenerateMipmaps: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::RegenerateMipmaps>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::GenerateFakeLayerName: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::GenerateFakeLayerName>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::AttemptRepairPuppet: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::AttemptRepairPuppet>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::RegenerateNodeIDs: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::RegenerateNodeIDs>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::ModelEditMode: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::ModelEditMode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::AnimEditMode: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::AnimEditMode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::ExecuteAutoRig: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::ExecuteAutoRig>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::GetAutoRigStatus: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::GetAutoRigStatus>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::GetAutoRigMemoryStatus: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::GetAutoRigMemoryStatus>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::DeleteAutoRigSession: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::DeleteAutoRigSession>::type>(key, value, ngCommandIdFromKey(key)); return;
        case puppet::tool::ToolCommand::ResumeAutoRig: visitor.template operator()<typename CommandClassFor<puppet::tool::ToolCommand::ResumeAutoRig>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, viewport::control::ViewportCommand>) {
        switch (key) {
        case viewport::control::ViewportCommand::ToggleMirrorView: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::ToggleMirrorView>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::ToggleOnionSlice: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::ToggleOnionSlice>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::TogglePhysics: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::TogglePhysics>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::TogglePostProcess: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::TogglePostProcess>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::ResetPhysics: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::ResetPhysics>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::ResetParameters: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::ResetParameters>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::ListFlipPairs: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::ListFlipPairs>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::AddFlipPair: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::AddFlipPair>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::AutoAddFlipPairs: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::AutoAddFlipPairs>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::RemoveFlipPair: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::RemoveFlipPair>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::OpenFlipPairWindow: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::OpenFlipPairWindow>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::OpenAutomeshBatching: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::OpenAutomeshBatching>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::ResetViewportZoom: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::ResetViewportZoom>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::ResetViewportPosition: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::ResetViewportPosition>::type>(key, value, ngCommandIdFromKey(key)); return;
        case viewport::control::ViewportCommand::FitViewportToModel: visitor.template operator()<typename CommandClassFor<viewport::control::ViewportCommand::FitViewportToModel>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, viewport::palette::PaletteCommand>) {
        switch (key) {
        case viewport::palette::PaletteCommand::ListCommand: visitor.template operator()<typename CommandClassFor<viewport::palette::PaletteCommand::ListCommand>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, inspector::apply_node::InspectorNodeApplyCommand>) {
        switch (key) {
        case inspector::apply_node::InspectorNodeApplyCommand::TranslationX: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TranslationX>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::TranslationY: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TranslationY>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::TranslationZ: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TranslationZ>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::RotationX: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::RotationX>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::RotationY: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::RotationY>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::RotationZ: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::RotationZ>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::ScaleX: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ScaleX>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::ScaleY: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ScaleY>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PixelSnap: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PixelSnap>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::TogglePixelSnap: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TogglePixelSnap>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::ZSort: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ZSort>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::LockToRoot: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::LockToRoot>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::ToggleLockToRoot: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ToggleLockToRoot>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PinToMesh: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PinToMesh>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::TogglePinToMesh: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TogglePinToMesh>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::OffsetX: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::OffsetX>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::OffsetY: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::OffsetY>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::ViewportOrigin: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ViewportOrigin>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::CompositeTint: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::CompositeTint>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::CompositeScreenTint: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::CompositeScreenTint>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::CompositeBlendingMode: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::CompositeBlendingMode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::CompositeOpacity: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::CompositeOpacity>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::CompositeThreshold: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::CompositeThreshold>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PartTint: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartTint>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PartScreenTint: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartScreenTint>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PartEmissionStrength: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartEmissionStrength>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PartBlendingMode: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartBlendingMode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PartOpacity: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartOpacity>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PartMaskAlphaThreshold: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartMaskAlphaThreshold>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PartAutoResizedMesh: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PartAutoResizedMesh>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::TogglePartAutoResizedMesh: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TogglePartAutoResizedMesh>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::MeshGroupDynamic: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::MeshGroupDynamic>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::ToggleMeshGroupDynamic: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ToggleMeshGroupDynamic>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::MeshGroupTranslateChildren: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::MeshGroupTranslateChildren>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::ToggleMeshGroupTranslateChildren: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ToggleMeshGroupTranslateChildren>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::GridDeformDynamic: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::GridDeformDynamic>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::ToggleGridDeformDynamic: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ToggleGridDeformDynamic>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PathDeformDynamic: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformDynamic>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::TogglePathDeformDynamic: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TogglePathDeformDynamic>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PathDeformPhysicsEnabled: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformPhysicsEnabled>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::TogglePathDeformPhysicsEnabled: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::TogglePathDeformPhysicsEnabled>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PathDeformCurveType: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformCurveType>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PathDeformGravity: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformGravity>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PathDeformRestoreConstant: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformRestoreConstant>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PathDeformDamping: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformDamping>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PathDeformInputScale: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformInputScale>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::PathDeformPropagateScale: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::PathDeformPropagateScale>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsModelType: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsModelType>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsMapMode: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsMapMode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsLocalOnly: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsLocalOnly>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::ToggleSimplePhysicsLocalOnly: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::ToggleSimplePhysicsLocalOnly>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsGravity: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsGravity>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsLength: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsLength>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsFrequency: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsFrequency>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsAngleDamping: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsAngleDamping>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsLengthDamping: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsLengthDamping>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsOutputScaleX: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsOutputScaleX>::type>(key, value, ngCommandIdFromKey(key)); return;
        case inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsOutputScaleY: visitor.template operator()<typename CommandClassFor<inspector::apply_node::InspectorNodeApplyCommand::SimplePhysicsOutputScaleY>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, node::mask::NodeMaskCommand>) {
        switch (key) {
        case node::mask::NodeMaskCommand::AddMask: visitor.template operator()<typename CommandClassFor<node::mask::NodeMaskCommand::AddMask>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::mask::NodeMaskCommand::RemoveMask: visitor.template operator()<typename CommandClassFor<node::mask::NodeMaskCommand::RemoveMask>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::mask::NodeMaskCommand::ChangeMaskMode: visitor.template operator()<typename CommandClassFor<node::mask::NodeMaskCommand::ChangeMaskMode>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, node::welding::NodeWeldingCommand>) {
        switch (key) {
        case node::welding::NodeWeldingCommand::AddWelding: visitor.template operator()<typename CommandClassFor<node::welding::NodeWeldingCommand::AddWelding>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::welding::NodeWeldingCommand::RemoveWelding: visitor.template operator()<typename CommandClassFor<node::welding::NodeWeldingCommand::RemoveWelding>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::welding::NodeWeldingCommand::ChangeWeldingWeight: visitor.template operator()<typename CommandClassFor<node::welding::NodeWeldingCommand::ChangeWeldingWeight>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, node::simplephysics::NodeSimplePhysicsCommand>) {
        switch (key) {
        case node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsParameter: visitor.template operator()<typename CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsParameter>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::simplephysics::NodeSimplePhysicsCommand::ClearSimplePhysicsParameter: visitor.template operator()<typename CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::ClearSimplePhysicsParameter>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsModelType: visitor.template operator()<typename CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsModelType>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsMapMode: visitor.template operator()<typename CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsMapMode>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsLocalOnly: visitor.template operator()<typename CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsLocalOnly>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsGravity: visitor.template operator()<typename CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsGravity>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsLength: visitor.template operator()<typename CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsLength>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsFrequency: visitor.template operator()<typename CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsFrequency>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsAngleDamping: visitor.template operator()<typename CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsAngleDamping>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsLengthDamping: visitor.template operator()<typename CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsLengthDamping>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsOutputScaleX: visitor.template operator()<typename CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsOutputScaleX>::type>(key, value, ngCommandIdFromKey(key)); return;
        case node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsOutputScaleY: visitor.template operator()<typename CommandClassFor<node::simplephysics::NodeSimplePhysicsCommand::SetSimplePhysicsOutputScaleY>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode>) {
        switch (key) {
        case ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode::Points: visitor.template operator()<void>(key, value, ngCommandIdFromKey(key)); return;
        case ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode::Connect: visitor.template operator()<void>(key, value, ngCommandIdFromKey(key)); return;
        case ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode::PathDeform: visitor.template operator()<void>(key, value, ngCommandIdFromKey(key)); return;
        case ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode::Grid: visitor.template operator()<void>(key, value, ngCommandIdFromKey(key)); return;
        case ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode::Brush: visitor.template operator()<void>(key, value, ngCommandIdFromKey(key)); return;
        case ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode::BezierDeform: visitor.template operator()<void>(key, value, ngCommandIdFromKey(key)); return;
        case ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode::AltGridDeform: visitor.template operator()<void>(key, value, ngCommandIdFromKey(key)); return;
        case ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode::AltBezierDeform: visitor.template operator()<void>(key, value, ngCommandIdFromKey(key)); return;
        case ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode::LassoSelection: visitor.template operator()<void>(key, value, ngCommandIdFromKey(key)); return;
        case ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode::EdgeCutter: visitor.template operator()<void>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, automesh::config::AutoMeshConfigCommand>) {
        switch (key) {
        case automesh::config::AutoMeshConfigCommand::AutoMeshListProcessors: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshListProcessors>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshConfigCommand::AutoMeshGetActive: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshGetActive>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshConfigCommand::AutoMeshSetActive: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshSetActive>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshConfigCommand::AutoMeshApplyActive: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshApplyActive>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshConfigCommand::AutoMeshGetSchema: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshGetSchema>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshConfigCommand::AutoMeshGetValues: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshGetValues>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshConfigCommand::AutoMeshSetPreset: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshSetPreset>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshConfigCommand::AutoMeshSetValues: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshConfigCommand::AutoMeshSetValues>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, automesh::config::AutoMeshTypedCommand>) {
        switch (key) {
        case automesh::config::AutoMeshTypedCommand::AutoMeshSetSimple_OptimumAutoMeshProcessor: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetSimple_OptimumAutoMeshProcessor>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshTypedCommand::AutoMeshSetAdvanced_OptimumAutoMeshProcessor: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetAdvanced_OptimumAutoMeshProcessor>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshTypedCommand::AutoMeshSetPreset_OptimumAutoMeshProcessor: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetPreset_OptimumAutoMeshProcessor>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshTypedCommand::AutoMeshSetSimple_ContourAutoMeshProcessor: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetSimple_ContourAutoMeshProcessor>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshTypedCommand::AutoMeshSetAdvanced_ContourAutoMeshProcessor: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetAdvanced_ContourAutoMeshProcessor>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshTypedCommand::AutoMeshSetPreset_ContourAutoMeshProcessor: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetPreset_ContourAutoMeshProcessor>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshTypedCommand::AutoMeshSetSimple_GridAutoMeshProcessor: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetSimple_GridAutoMeshProcessor>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshTypedCommand::AutoMeshSetAdvanced_GridAutoMeshProcessor: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetAdvanced_GridAutoMeshProcessor>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshTypedCommand::AutoMeshSetPreset_GridAutoMeshProcessor: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetPreset_GridAutoMeshProcessor>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshTypedCommand::AutoMeshSetSimple_SkeletonExtractor: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetSimple_SkeletonExtractor>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshTypedCommand::AutoMeshSetAdvanced_SkeletonExtractor: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetAdvanced_SkeletonExtractor>::type>(key, value, ngCommandIdFromKey(key)); return;
        case automesh::config::AutoMeshTypedCommand::AutoMeshSetPreset_SkeletonExtractor: visitor.template operator()<typename CommandClassFor<automesh::config::AutoMeshTypedCommand::AutoMeshSetPreset_SkeletonExtractor>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, vertex::define_mesh::VertexCommand>) {
        switch (key) {
        case vertex::define_mesh::VertexCommand::DefineMesh: visitor.template operator()<typename CommandClassFor<vertex::define_mesh::VertexCommand::DefineMesh>::type>(key, value, ngCommandIdFromKey(key)); return;
        case vertex::define_mesh::VertexCommand::DefineVertices: visitor.template operator()<typename CommandClassFor<vertex::define_mesh::VertexCommand::DefineVertices>::type>(key, value, ngCommandIdFromKey(key)); return;
        case vertex::define_mesh::VertexCommand::DefineGrid: visitor.template operator()<typename CommandClassFor<vertex::define_mesh::VertexCommand::DefineGrid>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, model::set_deform_binding::ModelCommand>) {
        switch (key) {
        case model::set_deform_binding::ModelCommand::SetDeformBinding: visitor.template operator()<typename CommandClassFor<model::set_deform_binding::ModelCommand::SetDeformBinding>::type>(key, value, ngCommandIdFromKey(key)); return;
        case model::set_deform_binding::ModelCommand::SetTRSBinding: visitor.template operator()<typename CommandClassFor<model::set_deform_binding::ModelCommand::SetTRSBinding>::type>(key, value, ngCommandIdFromKey(key)); return;
        case model::set_deform_binding::ModelCommand::SetRotationYBinding: visitor.template operator()<typename CommandClassFor<model::set_deform_binding::ModelCommand::SetRotationYBinding>::type>(key, value, ngCommandIdFromKey(key)); return;
        case model::set_deform_binding::ModelCommand::SetRotationXBinding: visitor.template operator()<typename CommandClassFor<model::set_deform_binding::ModelCommand::SetRotationXBinding>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, depth::bone::DepthBoneCommand>) {
        switch (key) {
        case depth::bone::DepthBoneCommand::CreateDepthRigRoot: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::CreateDepthRigRoot>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::AddDepthBone: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::AddDepthBone>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::AddStandardDepthSkeleton: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::AddStandardDepthSkeleton>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::AddStandardDepthParameters: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::AddStandardDepthParameters>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::FitDepthRigRootZToDepth: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::FitDepthRigRootZToDepth>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::FitDepthBoneZToDepth: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::FitDepthBoneZToDepth>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::SetDepthBoneRest: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::SetDepthBoneRest>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::SetDepthBoneConstraint: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::SetDepthBoneConstraint>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::ListDepthBones: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::ListDepthBones>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::AddDepthBoneSource: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::AddDepthBoneSource>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::RemoveDepthBoneSource: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::RemoveDepthBoneSource>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::ListDepthBoneSources: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::ListDepthBoneSources>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::SetDepthBoneSourceSettings: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::SetDepthBoneSourceSettings>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::SetDepthBoneInfluenceRule: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::SetDepthBoneInfluenceRule>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::GetDepthBoneInfluenceRule: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::GetDepthBoneInfluenceRule>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::PreviewDepthBoneInfluence: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::PreviewDepthBoneInfluence>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::PreviewDepthBoneDeform: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::PreviewDepthBoneDeform>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::bone::DepthBoneCommand::ApplyDepthBoneDeform: visitor.template operator()<typename CommandClassFor<depth::bone::DepthBoneCommand::ApplyDepthBoneDeform>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, depth::editor::DepthEditorOperationCommand>) {
        switch (key) {
        case depth::editor::DepthEditorOperationCommand::AddEditorDepthOp: visitor.template operator()<typename CommandClassFor<depth::editor::DepthEditorOperationCommand::AddEditorDepthOp>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::editor::DepthEditorOperationCommand::UpdateEditorDepthOp: visitor.template operator()<typename CommandClassFor<depth::editor::DepthEditorOperationCommand::UpdateEditorDepthOp>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::editor::DepthEditorOperationCommand::RemoveEditorDepthOp: visitor.template operator()<typename CommandClassFor<depth::editor::DepthEditorOperationCommand::RemoveEditorDepthOp>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, depth::map::DepthMapCommand>) {
        switch (key) {
        case depth::map::DepthMapCommand::ListDepths: visitor.template operator()<typename CommandClassFor<depth::map::DepthMapCommand::ListDepths>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::map::DepthMapCommand::SetDepths: visitor.template operator()<typename CommandClassFor<depth::map::DepthMapCommand::SetDepths>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::map::DepthMapCommand::ClearDepths: visitor.template operator()<typename CommandClassFor<depth::map::DepthMapCommand::ClearDepths>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::map::DepthMapCommand::ListDepthOps: visitor.template operator()<typename CommandClassFor<depth::map::DepthMapCommand::ListDepthOps>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::map::DepthMapCommand::SetDepthOps: visitor.template operator()<typename CommandClassFor<depth::map::DepthMapCommand::SetDepthOps>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::map::DepthMapCommand::AddDepthOp: visitor.template operator()<typename CommandClassFor<depth::map::DepthMapCommand::AddDepthOp>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::map::DepthMapCommand::UpdateDepthOp: visitor.template operator()<typename CommandClassFor<depth::map::DepthMapCommand::UpdateDepthOp>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::map::DepthMapCommand::RemoveDepthOp: visitor.template operator()<typename CommandClassFor<depth::map::DepthMapCommand::RemoveDepthOp>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::map::DepthMapCommand::MoveDepthOp: visitor.template operator()<typename CommandClassFor<depth::map::DepthMapCommand::MoveDepthOp>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::map::DepthMapCommand::ClearDepthOps: visitor.template operator()<typename CommandClassFor<depth::map::DepthMapCommand::ClearDepthOps>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::map::DepthMapCommand::ApplyDepthOps: visitor.template operator()<typename CommandClassFor<depth::map::DepthMapCommand::ApplyDepthOps>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::map::DepthMapCommand::ImportPSDDepths: visitor.template operator()<typename CommandClassFor<depth::map::DepthMapCommand::ImportPSDDepths>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (std::same_as<K, depth::psd_dialog::PsdDepthDialogCommand>) {
        switch (key) {
        case depth::psd_dialog::PsdDepthDialogCommand::InspectPsdDepthDialog: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::InspectPsdDepthDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::GetPsdDepthDialogPartData: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::GetPsdDepthDialogPartData>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogColorSource: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogColorSource>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogInvert: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogInvert>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogBackDepth: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogBackDepth>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogFrontDepth: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogFrontDepth>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogDepthScale: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogDepthScale>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogChannel: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogChannel>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogSampling: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogSampling>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogCustomRadius: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogCustomRadius>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogAlphaThreshold: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogAlphaThreshold>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogMissingPolicy: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogMissingPolicy>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogContourRepair: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogContourRepair>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogSurfaceSmoothing: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogSurfaceSmoothing>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::FillPsdDepthDialogAlphaDepthGaps: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::FillPsdDepthDialogAlphaDepthGaps>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogGpuComposition: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogGpuComposition>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogDirectGridMatch: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogDirectGridMatch>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogProblemFilter: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogProblemFilter>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingAuto: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingAuto>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingIgnored: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingIgnored>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingTarget: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerMappingTarget>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogTargetEnabled: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogTargetEnabled>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerEnabled: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerEnabled>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerVisible: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerVisible>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthEnabled: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthEnabled>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthInverted: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthInverted>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthScale: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthScale>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthOffset: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::SetPsdDepthDialogLayerDepthOffset>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::ResetPsdDepthDialogLayerDepthTransform: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::ResetPsdDepthDialogLayerDepthTransform>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::ApplyPsdDepthDialog: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::ApplyPsdDepthDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        case depth::psd_dialog::PsdDepthDialogCommand::CancelPsdDepthDialog: visitor.template operator()<typename CommandClassFor<depth::psd_dialog::PsdDepthDialogCommand::CancelPsdDepthDialog>::type>(key, value, ngCommandIdFromKey(key)); return;
        }
        visitor.template operator()<void>(key, value, ngCommandIdFromKey(key));
        return;
    }
    if constexpr (!std::is_enum_v<K>) visitor.template operator()<Command>(key, value, ngCommandIdFromKey(key));
}
template<class V> void ngVisitConcreteCommands(V&& visitor) {
    ngForEachCommandMap([&](const auto& table) {
        for (const auto& [key, value] : table) ngVisitConcreteCommand(key, value, visitor);
    });
}
inline compat::RetainedString ngCommandSourceClassName(const std::shared_ptr<Command>& input) {
    // Source expression typeid loads the object's vtable before ClassInfo.
    const auto command = compat::ngRequireObject(input);
    // Exact eponymous template ClassInfo names follow pinned DMD/LDC pretty-printing.
    if (typeid(*command) == typeid(node::node::AddNodeCommandT<true>)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.AddNodeCommandT!true.AddNodeCommandT");
    if (typeid(*command) == typeid(node::node::InsertNodeCommandT<true>)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.InsertNodeCommandT!true.InsertNodeCommandT");
    if (typeid(*command) == typeid(node::node::ConvertToCommandT<true>)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.ConvertToCommandT!true.ConvertToCommandT");
    if (typeid(*command) == typeid(node::node::AddNodeCommandT<false>)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.AddNodeCommandT!false.AddNodeCommandT");
    if (typeid(*command) == typeid(node::node::InsertNodeCommandT<false>)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.InsertNodeCommandT!false.InsertNodeCommandT");
    if (typeid(*command) == typeid(node::dynamic::ConvertNodeToCommand<false>)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.dynamic.ConvertNodeToCommand!false.ConvertNodeToCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::GetCurrentCommandScopeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.command_scope.GetCurrentCommandScopeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshListProcessorsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.automesh.config.AutoMeshListProcessorsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshGetActiveCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.automesh.config.AutoMeshGetActiveCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetActiveCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.automesh.config.AutoMeshSetActiveCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshApplyActiveCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.automesh.config.AutoMeshApplyActiveCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshGetSchemaCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.automesh.config.AutoMeshGetSchemaCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshGetValuesCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.automesh.config.AutoMeshGetValuesCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetPresetCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.automesh.config.AutoMeshSetPresetCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetValuesCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.automesh.config.AutoMeshSetValuesCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetSimple_OptimumAutoMeshProcessorCommand)) {
        detail::prepareCommandResultClass<nijigenerate::commands::automesh::config::AutoMeshSetSimple_OptimumAutoMeshProcessorCommand>();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*command));
    }
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand)) {
        detail::prepareCommandResultClass<nijigenerate::commands::automesh::config::AutoMeshSetAdvanced_OptimumAutoMeshProcessorCommand>();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*command));
    }
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetPreset_OptimumAutoMeshProcessorCommand)) {
        detail::prepareCommandResultClass<nijigenerate::commands::automesh::config::AutoMeshSetPreset_OptimumAutoMeshProcessorCommand>();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*command));
    }
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetSimple_ContourAutoMeshProcessorCommand)) {
        detail::prepareCommandResultClass<nijigenerate::commands::automesh::config::AutoMeshSetSimple_ContourAutoMeshProcessorCommand>();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*command));
    }
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand)) {
        detail::prepareCommandResultClass<nijigenerate::commands::automesh::config::AutoMeshSetAdvanced_ContourAutoMeshProcessorCommand>();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*command));
    }
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetPreset_ContourAutoMeshProcessorCommand)) {
        detail::prepareCommandResultClass<nijigenerate::commands::automesh::config::AutoMeshSetPreset_ContourAutoMeshProcessorCommand>();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*command));
    }
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetSimple_GridAutoMeshProcessorCommand)) {
        detail::prepareCommandResultClass<nijigenerate::commands::automesh::config::AutoMeshSetSimple_GridAutoMeshProcessorCommand>();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*command));
    }
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetAdvanced_GridAutoMeshProcessorCommand)) {
        detail::prepareCommandResultClass<nijigenerate::commands::automesh::config::AutoMeshSetAdvanced_GridAutoMeshProcessorCommand>();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*command));
    }
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetPreset_GridAutoMeshProcessorCommand)) {
        detail::prepareCommandResultClass<nijigenerate::commands::automesh::config::AutoMeshSetPreset_GridAutoMeshProcessorCommand>();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*command));
    }
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetSimple_SkeletonExtractorCommand)) {
        detail::prepareCommandResultClass<nijigenerate::commands::automesh::config::AutoMeshSetSimple_SkeletonExtractorCommand>();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*command));
    }
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetAdvanced_SkeletonExtractorCommand)) {
        detail::prepareCommandResultClass<nijigenerate::commands::automesh::config::AutoMeshSetAdvanced_SkeletonExtractorCommand>();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*command));
    }
    if (typeid(*command) == typeid(nijigenerate::commands::automesh::config::AutoMeshSetPreset_SkeletonExtractorCommand)) {
        detail::prepareCommandResultClass<nijigenerate::commands::automesh::config::AutoMeshSetPreset_SkeletonExtractorCommand>();
        return nicxlive::core::d_runtime::ngObjectClassName(typeid(*command));
    }
    if (typeid(*command) == typeid(nijigenerate::commands::binding::UnsetKeyFrameCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.UnsetKeyFrameCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::SetKeyFrameCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.SetKeyFrameCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::ResetKeyFrameCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.ResetKeyFrameCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::InvertKeyFrameCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.InvertKeyFrameCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::MirrorKeyFrameHorizontallyCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.MirrorKeyFrameHorizontallyCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::MirrorKeyFrameVerticallyCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.MirrorKeyFrameVerticallyCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::FlipDeformCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.FlipDeformCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::SymmetrizeDeformCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.SymmetrizeDeformCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::SetFromHorizontalMirrorCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.SetFromHorizontalMirrorCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::SetFromVerticalMirrorCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.SetFromVerticalMirrorCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::SetFromDiagonalMirrorCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.SetFromDiagonalMirrorCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::SetFrom1DMirrorCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.SetFrom1DMirrorCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::CopyBindingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.CopyBindingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::PasteBindingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.PasteBindingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::RemoveBindingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.RemoveBindingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::binding::SetInterpolationCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.binding.binding.SetInterpolationCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::CreateDepthRigRootCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.CreateDepthRigRootCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::AddDepthBoneCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.AddDepthBoneCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::AddStandardDepthSkeletonCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.AddStandardDepthSkeletonCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::AddStandardDepthParametersCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.AddStandardDepthParametersCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::FitDepthRigRootZToDepthCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.FitDepthRigRootZToDepthCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::FitDepthBoneZToDepthCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.FitDepthBoneZToDepthCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::SetDepthBoneRestCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.SetDepthBoneRestCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::SetDepthBoneConstraintCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.SetDepthBoneConstraintCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::ListDepthBonesCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.ListDepthBonesCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::AddDepthBoneSourceCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.AddDepthBoneSourceCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::RemoveDepthBoneSourceCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.RemoveDepthBoneSourceCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::ListDepthBoneSourcesCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.ListDepthBoneSourcesCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::SetDepthBoneSourceSettingsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.SetDepthBoneSourceSettingsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::SetDepthBoneInfluenceRuleCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.SetDepthBoneInfluenceRuleCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::GetDepthBoneInfluenceRuleCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.GetDepthBoneInfluenceRuleCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::PreviewDepthBoneInfluenceCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.PreviewDepthBoneInfluenceCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::PreviewDepthBoneDeformCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.PreviewDepthBoneDeformCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::bone::ApplyDepthBoneDeformCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.bone.ApplyDepthBoneDeformCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::editor::AddEditorDepthOpCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.editor.AddEditorDepthOpCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::editor::UpdateEditorDepthOpCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.editor.UpdateEditorDepthOpCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::editor::RemoveEditorDepthOpCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.editor.RemoveEditorDepthOpCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::map::ListDepthsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map.ListDepthsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::map::SetDepthsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map.SetDepthsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::map::ClearDepthsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map.ClearDepthsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::map::ListDepthOpsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map.ListDepthOpsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::map::SetDepthOpsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map.SetDepthOpsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::map::AddDepthOpCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map.AddDepthOpCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::map::UpdateDepthOpCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map.UpdateDepthOpCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::map::RemoveDepthOpCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map.RemoveDepthOpCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::map::MoveDepthOpCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map.MoveDepthOpCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::map::ClearDepthOpsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map.ClearDepthOpsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::map::ApplyDepthOpsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map.ApplyDepthOpsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::map::ImportPSDDepthsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.map.ImportPSDDepthsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::InspectPsdDepthDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.InspectPsdDepthDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::GetPsdDepthDialogPartDataCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.GetPsdDepthDialogPartDataCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogColorSourceCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogColorSourceCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogInvertCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogInvertCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogBackDepthCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogBackDepthCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogFrontDepthCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogFrontDepthCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogDepthScaleCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogDepthScaleCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogChannelCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogChannelCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogSamplingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogSamplingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogCustomRadiusCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogCustomRadiusCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogAlphaThresholdCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogAlphaThresholdCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogMissingPolicyCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogMissingPolicyCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogContourRepairCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogContourRepairCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogSurfaceSmoothingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogSurfaceSmoothingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::FillPsdDepthDialogAlphaDepthGapsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.FillPsdDepthDialogAlphaDepthGapsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogGpuCompositionCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogGpuCompositionCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogDirectGridMatchCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogDirectGridMatchCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogProblemFilterCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogProblemFilterCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerMappingAutoCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogLayerMappingAutoCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerMappingIgnoredCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogLayerMappingIgnoredCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerMappingTargetCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogLayerMappingTargetCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogTargetEnabledCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogTargetEnabledCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerEnabledCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogLayerEnabledCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerVisibleCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogLayerVisibleCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerDepthEnabledCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogLayerDepthEnabledCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerDepthInvertedCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogLayerDepthInvertedCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerDepthScaleCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogLayerDepthScaleCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::SetPsdDepthDialogLayerDepthOffsetCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.SetPsdDepthDialogLayerDepthOffsetCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::ResetPsdDepthDialogLayerDepthTransformCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.ResetPsdDepthDialogLayerDepthTransformCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::ApplyPsdDepthDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.ApplyPsdDepthDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::depth::psd_dialog::CancelPsdDepthDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.depth.psd_dialog.CancelPsdDepthDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::ViewportOriginCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.ViewportOriginCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::CompositeTintCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.CompositeTintCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::CompositeScreenTintCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.CompositeScreenTintCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::CompositeBlendingModeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.CompositeBlendingModeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::CompositeOpacityCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.CompositeOpacityCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::CompositeThresholdCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.CompositeThresholdCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::OffsetXCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.OffsetXCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::OffsetYCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.OffsetYCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::GridDeformDynamicCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.GridDeformDynamicCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::ToggleGridDeformDynamicCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.ToggleGridDeformDynamicCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::MeshGroupDynamicCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.MeshGroupDynamicCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::MeshGroupTranslateChildrenCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.MeshGroupTranslateChildrenCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::ToggleMeshGroupDynamicCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.ToggleMeshGroupDynamicCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::ToggleMeshGroupTranslateChildrenCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.ToggleMeshGroupTranslateChildrenCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::TranslationXCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.TranslationXCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::TranslationYCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.TranslationYCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::TranslationZCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.TranslationZCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::RotationXCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.RotationXCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::RotationYCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.RotationYCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::RotationZCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.RotationZCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::ScaleXCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.ScaleXCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::ScaleYCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.ScaleYCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PixelSnapCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PixelSnapCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::TogglePixelSnapCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.TogglePixelSnapCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::ZSortCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.ZSortCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::LockToRootCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.LockToRootCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::ToggleLockToRootCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.ToggleLockToRootCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PinToMeshCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PinToMeshCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::TogglePinToMeshCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.TogglePinToMeshCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PartTintCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PartTintCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PartScreenTintCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PartScreenTintCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PartEmissionStrengthCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PartEmissionStrengthCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PartBlendingModeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PartBlendingModeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PartOpacityCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PartOpacityCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PartMaskAlphaThresholdCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PartMaskAlphaThresholdCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PartAutoResizedMeshCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PartAutoResizedMeshCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::TogglePartAutoResizedMeshCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.TogglePartAutoResizedMeshCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PathDeformDynamicCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PathDeformDynamicCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::TogglePathDeformDynamicCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.TogglePathDeformDynamicCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PathDeformPhysicsEnabledCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PathDeformPhysicsEnabledCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::TogglePathDeformPhysicsEnabledCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.TogglePathDeformPhysicsEnabledCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PathDeformCurveTypeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PathDeformCurveTypeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PathDeformGravityCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PathDeformGravityCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PathDeformRestoreConstantCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PathDeformRestoreConstantCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PathDeformDampingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PathDeformDampingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PathDeformInputScaleCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PathDeformInputScaleCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::PathDeformPropagateScaleCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.PathDeformPropagateScaleCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::SimplePhysicsModelTypeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.SimplePhysicsModelTypeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::SimplePhysicsMapModeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.SimplePhysicsMapModeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::SimplePhysicsLocalOnlyCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.SimplePhysicsLocalOnlyCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::ToggleSimplePhysicsLocalOnlyCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.ToggleSimplePhysicsLocalOnlyCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::SimplePhysicsGravityCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.SimplePhysicsGravityCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::SimplePhysicsLengthCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.SimplePhysicsLengthCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::SimplePhysicsFrequencyCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.SimplePhysicsFrequencyCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::SimplePhysicsAngleDampingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.SimplePhysicsAngleDampingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::SimplePhysicsLengthDampingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.SimplePhysicsLengthDampingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::SimplePhysicsOutputScaleXCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.SimplePhysicsOutputScaleXCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::inspector::apply_node::SimplePhysicsOutputScaleYCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.inspector.apply_node.SimplePhysicsOutputScaleYCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::model::set_deform_binding::SetDeformBindingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.model.set_deform_binding.SetDeformBindingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::model::set_deform_binding::SetTRSBindingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.model.set_deform_binding.SetTRSBindingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::model::set_deform_binding::SetRotationXBindingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.model.set_deform_binding.SetRotationXBindingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::model::set_deform_binding::SetRotationYBindingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.model.set_deform_binding.SetRotationYBindingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::mask::AddMaskCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.mask.AddMaskCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::mask::RemoveMaskCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.mask.RemoveMaskCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::mask::ChangeMaskModeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.mask.ChangeMaskModeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::node::MoveNodeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.MoveNodeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::node::SetNodeNameCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.SetNodeNameCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::node::DeleteNodeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.DeleteNodeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::node::CutNodeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.CutNodeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::node::CopyNodeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.CopyNodeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::node::PasteNodeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.PasteNodeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::node::ReloadNodeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.ReloadNodeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::node::VertexModeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.VertexModeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::node::ToggleVisibilityCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.ToggleVisibilityCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::node::CentralizeNodeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.node.CentralizeNodeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::simplephysics::SetSimplePhysicsModelTypeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.simplephysics.SetSimplePhysicsModelTypeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::simplephysics::SetSimplePhysicsMapModeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.simplephysics.SetSimplePhysicsMapModeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::simplephysics::SetSimplePhysicsLocalOnlyCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.simplephysics.SetSimplePhysicsLocalOnlyCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::simplephysics::SetSimplePhysicsParameterCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.simplephysics.SetSimplePhysicsParameterCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::simplephysics::ClearSimplePhysicsParameterCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.simplephysics.ClearSimplePhysicsParameterCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::welding::AddWeldingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.welding.AddWeldingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::welding::RemoveWeldingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.welding.RemoveWeldingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::node::welding::ChangeWeldingWeightCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.node.welding.ChangeWeldingWeightCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::animedit::AddAnimationKeyFrameCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.animedit.AddAnimationKeyFrameCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::animedit::CopyAnimationKeyFrameCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.animedit.CopyAnimationKeyFrameCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::animedit::PasteAnimationKeyFrameCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.animedit.PasteAnimationKeyFrameCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::group::MoveParameterCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.group.MoveParameterCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::group::CreateParamGroupCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.group.CreateParamGroupCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::group::ChangeGroupColorCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.group.ChangeGroupColorCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::group::DeleteParamGroupCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.group.DeleteParamGroupCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::ConvertTo2DParamCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.ConvertTo2DParamCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::FlipXCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.FlipXCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::FlipYCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.FlipYCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::Flip1DCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.Flip1DCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::MirrorHorizontallyCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.MirrorHorizontallyCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::MirrorVerticallyCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.MirrorVerticallyCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::MirroredAutoFillDir1Command)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.MirroredAutoFillDir1Command");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::MirroredAutoFillDir2Command)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.MirroredAutoFillDir2Command");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::MirroredAutoFillDir3Command)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.MirroredAutoFillDir3Command");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::MirroredAutoFillDir4Command)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.MirroredAutoFillDir4Command");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::CopyParameterCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.CopyParameterCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::PasteParameterCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.PasteParameterCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::PasteParameterWithFlipCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.PasteParameterWithFlipCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::DuplicateParameterCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.DuplicateParameterCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::DuplicateParameterWithFlipCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.DuplicateParameterWithFlipCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::DeleteParameterCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.DeleteParameterCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::LinkToCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.LinkToCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::ToggleParameterArmCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.ToggleParameterArmCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::SetParameterKeypointCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.SetParameterKeypointCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::SetArmedParameterAndKeypointCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.SetArmedParameterAndKeypointCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::paramedit::SetStartingKeyFrameCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.paramedit.SetStartingKeyFrameCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::param::Add1DParameterCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.param.Add1DParameterCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::param::Add2DParameterCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.param.Add2DParameterCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::param::AddMouthParameterCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.param.AddMouthParameterCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::param::RemoveParameterCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.param.RemoveParameterCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::prop::SetParameterNameCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.prop.SetParameterNameCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::parameter::prop::ApplyParameterPropsAxesCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.parameter.prop.ApplyParameterPropsAxesCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::edit::UndoCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.edit.UndoCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::edit::RedoCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.edit.RedoCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::edit::ShowSettingsWindowCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.edit.ShowSettingsWindowCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::edit::ShowCommandBrowserWindowCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.edit.ShowCommandBrowserWindowCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::NewFileCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.NewFileCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowOpenFileDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowOpenFileDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::OpenFileCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.OpenFileCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowSaveFileDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowSaveFileDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::SaveFileCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.SaveFileCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowSaveFileAsDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowSaveFileAsDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowImportPSDDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowImportPSDDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowImportPSDDepthMapDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowImportPSDDepthMapDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::OpenPSDDepthMapDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.OpenPSDDepthMapDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ImportPSDCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ImportPSDCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowImportKRADialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowImportKRADialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ImportKRACommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ImportKRACommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowImportINPDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowImportINPDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ImportINPCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ImportINPCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowImportImageFolderDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowImportImageFolderDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ImportImageFolderCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ImportImageFolderCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowMergePSDDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowMergePSDDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::MergePSDCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.MergePSDCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowMergeKRADialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowMergeKRADialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::MergeKRACommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.MergeKRACommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowMergeImageFileDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowMergeImageFileDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::MergeImageFilesCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.MergeImageFilesCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowMergeINPDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowMergeINPDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::MergeINPCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.MergeINPCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowExportToINPDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowExportToINPDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ExportINPCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ExportINPCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowExportToPNGDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowExportToPNGDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ExportPNGCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ExportPNGCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowExportToJpegDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowExportToJpegDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ExportJPEGCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ExportJPEGCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowExportToTGADialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowExportToTGADialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ExportTGACommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ExportTGACommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ShowExportToVideoDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowExportToVideoDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::ExportVideoCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ExportVideoCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::file::CloseProjectCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.CloseProjectCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::ShowImportSessionDataDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.ShowImportSessionDataDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::PremultTextureCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.PremultTextureCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::RebleedTextureCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.RebleedTextureCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::RegenerateMipmapsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.RegenerateMipmapsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::GenerateFakeLayerNameCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.GenerateFakeLayerNameCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::AttemptRepairPuppetCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.AttemptRepairPuppetCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::RegenerateNodeIDsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.RegenerateNodeIDsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::ModelEditModeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.ModelEditModeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::AnimEditModeCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.AnimEditModeCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::ImportSessionDataCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.ImportSessionDataCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::ExecuteAutoRigCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.ExecuteAutoRigCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::GetAutoRigStatusCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.GetAutoRigStatusCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::GetAutoRigMemoryStatusCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.GetAutoRigMemoryStatusCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::DeleteAutoRigSessionCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.DeleteAutoRigSessionCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::tool::ResumeAutoRigCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.tool.ResumeAutoRigCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::view::SetDefaultLayoutCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.view.SetDefaultLayoutCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::view::ShowSaveScreenshotDialogCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.view.ShowSaveScreenshotDialogCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::view::SaveScreenshotCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.view.SaveScreenshotCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::view::CaptureLiveScreenshotCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.view.CaptureLiveScreenshotCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::view::ShowStatusForNerdsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.view.ShowStatusForNerdsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::puppet::view::ToggleDifferenceAggregationCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.view.ToggleDifferenceAggregationCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::vertex::define_mesh::DefineMeshCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.vertex.define_mesh.DefineMeshCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::vertex::define_mesh::DefineVerticesCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.vertex.define_mesh.DefineVerticesCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::vertex::define_mesh::DefineGridCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.vertex.define_mesh.DefineGridCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::ToggleMirrorViewCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.ToggleMirrorViewCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::ToggleOnionSliceCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.ToggleOnionSliceCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::TogglePhysicsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.TogglePhysicsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::TogglePostProcessCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.TogglePostProcessCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::ResetPhysicsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.ResetPhysicsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::ResetParametersCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.ResetParametersCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::OpenFlipPairWindowCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.OpenFlipPairWindowCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::OpenAutomeshBatchingCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.OpenAutomeshBatchingCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::ResetViewportZoomCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.ResetViewportZoomCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::ResetViewportPositionCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.ResetViewportPositionCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::FitViewportToModelCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.FitViewportToModelCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::ListFlipPairsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.ListFlipPairsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::AddFlipPairCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.AddFlipPairCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::AutoAddFlipPairsCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.AutoAddFlipPairsCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::control::RemoveFlipPairCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.control.RemoveFlipPairCommand");
    if (typeid(*command) == typeid(nijigenerate::commands::viewport::palette::ListCommandCommand)) return compat::RetainedString::fromStaticLiteral("nijigenerate.commands.viewport.palette.ListCommandCommand");
    return nicxlive::core::d_runtime::ngObjectClassName(typeid(*command));
}
}
