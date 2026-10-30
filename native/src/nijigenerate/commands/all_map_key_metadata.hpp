#pragma once
#include <nijigenerate/commands/detail/map_validation.hpp>
#include <nijigenerate/commands/puppet/tool_map_metadata.hpp>
#include <array>
#include <nijigenerate/commands/command_scope.hpp>
#include <nijigenerate/commands/binding/binding_types.hpp>
#include <nijigenerate/commands/node/node_types.hpp>
#include <nijigenerate/commands/parameter/animedit.hpp>
#include <nijigenerate/commands/parameter/group_types.hpp>
#include <nijigenerate/commands/parameter/param_types.hpp>
#include <nijigenerate/commands/parameter/paramedit_types.hpp>
#include <nijigenerate/commands/parameter/prop_types.hpp>
#include <nijigenerate/commands/puppet/file.hpp>
#include <nijigenerate/commands/puppet/edit_types.hpp>
#include <nijigenerate/commands/puppet/view.hpp>
#include <nijigenerate/commands/viewport/control_types.hpp>
#include <nijigenerate/commands/viewport/palette_types.hpp>
#include <nijigenerate/commands/inspector/apply_node.hpp>
#include <nijigenerate/commands/node/mask.hpp>
#include <nijigenerate/commands/node/welding.hpp>
#include <nijigenerate/commands/node/simplephysics.hpp>
#include <nijigenerate/commands/../viewport/common/mesheditor/tools/enums.hpp>
#include <nijigenerate/commands/automesh_config_command.hpp>
#include <nijigenerate/commands/automesh/config_tables.hpp>
#include <nijigenerate/commands/vertex/define_mesh.hpp>
#include <nijigenerate/commands/model/set_deform_binding.hpp>
#include <nijigenerate/commands/depth/bone.hpp>
#include <nijigenerate/commands/depth/editor_types.hpp>
#include <nijigenerate/commands/depth/map.hpp>
#include <nijigenerate/commands/depth/psd_dialog.hpp>
namespace nijigenerate::commands::detail {
template<> struct CommandMapKeyMetadata<CommandScopeCommand> {
    using Key = CommandScopeCommand;
    inline static constexpr std::string_view typeName = "CommandScopeCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::GetCurrentCommandScope, "GetCurrentCommandScope"}
    };
};
template<> struct CommandMapKeyMetadata<binding::BindingCommand> {
    using Key = binding::BindingCommand;
    inline static constexpr std::string_view typeName = "BindingCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::UnsetKeyFrame, "UnsetKeyFrame"},
        CommandEnumMember<Key>{Key::SetKeyFrame, "SetKeyFrame"},
        CommandEnumMember<Key>{Key::ResetKeyFrame, "ResetKeyFrame"},
        CommandEnumMember<Key>{Key::InvertKeyFrame, "InvertKeyFrame"},
        CommandEnumMember<Key>{Key::MirrorKeyFrameHorizontally, "MirrorKeyFrameHorizontally"},
        CommandEnumMember<Key>{Key::MirrorKeyFrameVertically, "MirrorKeyFrameVertically"},
        CommandEnumMember<Key>{Key::FlipDeform, "FlipDeform"},
        CommandEnumMember<Key>{Key::SymmetrizeDeform, "SymmetrizeDeform"},
        CommandEnumMember<Key>{Key::SetFromHorizontalMirror, "SetFromHorizontalMirror"},
        CommandEnumMember<Key>{Key::SetFromVerticalMirror, "SetFromVerticalMirror"},
        CommandEnumMember<Key>{Key::SetFromDiagonalMirror, "SetFromDiagonalMirror"},
        CommandEnumMember<Key>{Key::SetFrom1DMirror, "SetFrom1DMirror"},
        CommandEnumMember<Key>{Key::CopyBinding, "CopyBinding"},
        CommandEnumMember<Key>{Key::PasteBinding, "PasteBinding"},
        CommandEnumMember<Key>{Key::RemoveBinding, "RemoveBinding"},
        CommandEnumMember<Key>{Key::SetInterpolation, "SetInterpolation"}
    };
};
template<> struct CommandMapKeyMetadata<node::node::NodeCommand> {
    using Key = node::node::NodeCommand;
    inline static constexpr std::string_view typeName = "NodeCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::AddNode, "AddNode"},
        CommandEnumMember<Key>{Key::InsertNode, "InsertNode"},
        CommandEnumMember<Key>{Key::SetNodeName, "SetNodeName"},
        CommandEnumMember<Key>{Key::DeleteNode, "DeleteNode"},
        CommandEnumMember<Key>{Key::MoveNode, "MoveNode"},
        CommandEnumMember<Key>{Key::ConvertTo, "ConvertTo"},
        CommandEnumMember<Key>{Key::CutNode, "CutNode"},
        CommandEnumMember<Key>{Key::CopyNode, "CopyNode"},
        CommandEnumMember<Key>{Key::PasteNode, "PasteNode"},
        CommandEnumMember<Key>{Key::ReloadNode, "ReloadNode"},
        CommandEnumMember<Key>{Key::VertexMode, "VertexMode"},
        CommandEnumMember<Key>{Key::ToggleVisibility, "ToggleVisibility"},
        CommandEnumMember<Key>{Key::CentralizeNode, "CentralizeNode"}
    };
};
template<> struct CommandMapKeyMetadata<parameter::animedit::AnimeditCommand> {
    using Key = parameter::animedit::AnimeditCommand;
    inline static constexpr std::string_view typeName = "AnimeditCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::AddAnimationKeyFrame, "AddAnimationKeyFrame"},
        CommandEnumMember<Key>{Key::CopyAnimationKeyFrame, "CopyAnimationKeyFrame"},
        CommandEnumMember<Key>{Key::PasteAnimationKeyFrame, "PasteAnimationKeyFrame"}
    };
};
template<> struct CommandMapKeyMetadata<parameter::group::GroupCommand> {
    using Key = parameter::group::GroupCommand;
    inline static constexpr std::string_view typeName = "GroupCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::MoveParameter, "MoveParameter"},
        CommandEnumMember<Key>{Key::CreateParamGroup, "CreateParamGroup"},
        CommandEnumMember<Key>{Key::ChangeGroupColor, "ChangeGroupColor"},
        CommandEnumMember<Key>{Key::DeleteParamGroup, "DeleteParamGroup"}
    };
};
template<> struct CommandMapKeyMetadata<parameter::param::ParamCommand> {
    using Key = parameter::param::ParamCommand;
    inline static constexpr std::string_view typeName = "ParamCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::Add1DParameter, "Add1DParameter"},
        CommandEnumMember<Key>{Key::Add2DParameter, "Add2DParameter"},
        CommandEnumMember<Key>{Key::AddMouthParameter, "AddMouthParameter"},
        CommandEnumMember<Key>{Key::RemoveParameter, "RemoveParameter"}
    };
};
template<> struct CommandMapKeyMetadata<parameter::paramedit::ParameditCommand> {
    using Key = parameter::paramedit::ParameditCommand;
    inline static constexpr std::string_view typeName = "ParameditCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::ConvertTo2DParam, "ConvertTo2DParam"},
        CommandEnumMember<Key>{Key::FlipX, "FlipX"},
        CommandEnumMember<Key>{Key::FlipY, "FlipY"},
        CommandEnumMember<Key>{Key::Flip1D, "Flip1D"},
        CommandEnumMember<Key>{Key::MirrorHorizontally, "MirrorHorizontally"},
        CommandEnumMember<Key>{Key::MirrorVertically, "MirrorVertically"},
        CommandEnumMember<Key>{Key::MirroredAutoFillDir1, "MirroredAutoFillDir1"},
        CommandEnumMember<Key>{Key::MirroredAutoFillDir2, "MirroredAutoFillDir2"},
        CommandEnumMember<Key>{Key::MirroredAutoFillDir3, "MirroredAutoFillDir3"},
        CommandEnumMember<Key>{Key::MirroredAutoFillDir4, "MirroredAutoFillDir4"},
        CommandEnumMember<Key>{Key::CopyParameter, "CopyParameter"},
        CommandEnumMember<Key>{Key::PasteParameter, "PasteParameter"},
        CommandEnumMember<Key>{Key::PasteParameterWithFlip, "PasteParameterWithFlip"},
        CommandEnumMember<Key>{Key::DuplicateParameter, "DuplicateParameter"},
        CommandEnumMember<Key>{Key::DuplicateParameterWithFlip, "DuplicateParameterWithFlip"},
        CommandEnumMember<Key>{Key::DeleteParameter, "DeleteParameter"},
        CommandEnumMember<Key>{Key::LinkTo, "LinkTo"},
        CommandEnumMember<Key>{Key::ToggleParameterArm, "ToggleParameterArm"},
        CommandEnumMember<Key>{Key::SetParameterKeypoint, "SetParameterKeypoint"},
        CommandEnumMember<Key>{Key::SetArmedParameterAndKeypoint, "SetArmedParameterAndKeypoint"},
        CommandEnumMember<Key>{Key::SetStartingKeyFrame, "SetStartingKeyFrame"}
    };
};
template<> struct CommandMapKeyMetadata<parameter::prop::ParamPropCommand> {
    using Key = parameter::prop::ParamPropCommand;
    inline static constexpr std::string_view typeName = "ParamPropCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::SetParameterName, "SetParameterName"},
        CommandEnumMember<Key>{Key::ApplyParameterPropsAxes, "ApplyParameterPropsAxes"}
    };
};
template<> struct CommandMapKeyMetadata<puppet::file::FileCommand> {
    using Key = puppet::file::FileCommand;
    inline static constexpr std::string_view typeName = "FileCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::NewFile, "NewFile"},
        CommandEnumMember<Key>{Key::ShowOpenFileDialog, "ShowOpenFileDialog"},
        CommandEnumMember<Key>{Key::OpenFile, "OpenFile"},
        CommandEnumMember<Key>{Key::ShowSaveFileDialog, "ShowSaveFileDialog"},
        CommandEnumMember<Key>{Key::SaveFile, "SaveFile"},
        CommandEnumMember<Key>{Key::ShowSaveFileAsDialog, "ShowSaveFileAsDialog"},
        CommandEnumMember<Key>{Key::ShowImportPSDDialog, "ShowImportPSDDialog"},
        CommandEnumMember<Key>{Key::ShowImportPSDDepthMapDialog, "ShowImportPSDDepthMapDialog"},
        CommandEnumMember<Key>{Key::OpenPSDDepthMapDialog, "OpenPSDDepthMapDialog"},
        CommandEnumMember<Key>{Key::ImportPSD, "ImportPSD"},
        CommandEnumMember<Key>{Key::ShowImportKRADialog, "ShowImportKRADialog"},
        CommandEnumMember<Key>{Key::ImportKRA, "ImportKRA"},
        CommandEnumMember<Key>{Key::ShowImportINPDialog, "ShowImportINPDialog"},
        CommandEnumMember<Key>{Key::ImportINP, "ImportINP"},
        CommandEnumMember<Key>{Key::ShowImportImageFolderDialog, "ShowImportImageFolderDialog"},
        CommandEnumMember<Key>{Key::ImportImageFolder, "ImportImageFolder"},
        CommandEnumMember<Key>{Key::ShowMergePSDDialog, "ShowMergePSDDialog"},
        CommandEnumMember<Key>{Key::MergePSD, "MergePSD"},
        CommandEnumMember<Key>{Key::ShowMergeKRADialog, "ShowMergeKRADialog"},
        CommandEnumMember<Key>{Key::MergeKRA, "MergeKRA"},
        CommandEnumMember<Key>{Key::ShowMergeImageFileDialog, "ShowMergeImageFileDialog"},
        CommandEnumMember<Key>{Key::MergeImageFiles, "MergeImageFiles"},
        CommandEnumMember<Key>{Key::ShowMergeINPDialog, "ShowMergeINPDialog"},
        CommandEnumMember<Key>{Key::MergeINP, "MergeINP"},
        CommandEnumMember<Key>{Key::ShowExportToINPDialog, "ShowExportToINPDialog"},
        CommandEnumMember<Key>{Key::ExportINP, "ExportINP"},
        CommandEnumMember<Key>{Key::ShowExportToPNGDialog, "ShowExportToPNGDialog"},
        CommandEnumMember<Key>{Key::ExportPNG, "ExportPNG"},
        CommandEnumMember<Key>{Key::ShowExportToJpegDialog, "ShowExportToJpegDialog"},
        CommandEnumMember<Key>{Key::ExportJPEG, "ExportJPEG"},
        CommandEnumMember<Key>{Key::ShowExportToTGADialog, "ShowExportToTGADialog"},
        CommandEnumMember<Key>{Key::ExportTGA, "ExportTGA"},
        CommandEnumMember<Key>{Key::ShowExportToVideoDialog, "ShowExportToVideoDialog"},
        CommandEnumMember<Key>{Key::ExportVideo, "ExportVideo"},
        CommandEnumMember<Key>{Key::CloseProject, "CloseProject"}
    };
};
template<> struct CommandMapKeyMetadata<puppet::edit::EditCommand> {
    using Key = puppet::edit::EditCommand;
    inline static constexpr std::string_view typeName = "EditCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::Undo, "Undo"},
        CommandEnumMember<Key>{Key::Redo, "Redo"},
        CommandEnumMember<Key>{Key::ShowSettingsWindow, "ShowSettingsWindow"},
        CommandEnumMember<Key>{Key::ShowCommandBrowserWindow, "ShowCommandBrowserWindow"}
    };
};
template<> struct CommandMapKeyMetadata<puppet::view::ViewCommand> {
    using Key = puppet::view::ViewCommand;
    inline static constexpr std::string_view typeName = "ViewCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::SetDefaultLayout, "SetDefaultLayout"},
        CommandEnumMember<Key>{Key::ShowSaveScreenshotDialog, "ShowSaveScreenshotDialog"},
        CommandEnumMember<Key>{Key::SaveScreenshot, "SaveScreenshot"},
        CommandEnumMember<Key>{Key::CaptureLiveScreenshot, "CaptureLiveScreenshot"},
        CommandEnumMember<Key>{Key::ShowStatusForNerds, "ShowStatusForNerds"},
        CommandEnumMember<Key>{Key::ToggleDifferenceAggregation, "ToggleDifferenceAggregation"}
    };
};
template<> struct CommandMapKeyMetadata<viewport::control::ViewportCommand> {
    using Key = viewport::control::ViewportCommand;
    inline static constexpr std::string_view typeName = "ViewportCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::ToggleMirrorView, "ToggleMirrorView"},
        CommandEnumMember<Key>{Key::ToggleOnionSlice, "ToggleOnionSlice"},
        CommandEnumMember<Key>{Key::TogglePhysics, "TogglePhysics"},
        CommandEnumMember<Key>{Key::TogglePostProcess, "TogglePostProcess"},
        CommandEnumMember<Key>{Key::ResetPhysics, "ResetPhysics"},
        CommandEnumMember<Key>{Key::ResetParameters, "ResetParameters"},
        CommandEnumMember<Key>{Key::ListFlipPairs, "ListFlipPairs"},
        CommandEnumMember<Key>{Key::AddFlipPair, "AddFlipPair"},
        CommandEnumMember<Key>{Key::AutoAddFlipPairs, "AutoAddFlipPairs"},
        CommandEnumMember<Key>{Key::RemoveFlipPair, "RemoveFlipPair"},
        CommandEnumMember<Key>{Key::OpenFlipPairWindow, "OpenFlipPairWindow"},
        CommandEnumMember<Key>{Key::OpenAutomeshBatching, "OpenAutomeshBatching"},
        CommandEnumMember<Key>{Key::ResetViewportZoom, "ResetViewportZoom"},
        CommandEnumMember<Key>{Key::ResetViewportPosition, "ResetViewportPosition"},
        CommandEnumMember<Key>{Key::FitViewportToModel, "FitViewportToModel"}
    };
};
template<> struct CommandMapKeyMetadata<viewport::palette::PaletteCommand> {
    using Key = viewport::palette::PaletteCommand;
    inline static constexpr std::string_view typeName = "PaletteCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::ListCommand, "ListCommand"}
    };
};
template<> struct CommandMapKeyMetadata<inspector::apply_node::InspectorNodeApplyCommand> {
    using Key = inspector::apply_node::InspectorNodeApplyCommand;
    inline static constexpr std::string_view typeName = "InspectorNodeApplyCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::TranslationX, "TranslationX"},
        CommandEnumMember<Key>{Key::TranslationY, "TranslationY"},
        CommandEnumMember<Key>{Key::TranslationZ, "TranslationZ"},
        CommandEnumMember<Key>{Key::RotationX, "RotationX"},
        CommandEnumMember<Key>{Key::RotationY, "RotationY"},
        CommandEnumMember<Key>{Key::RotationZ, "RotationZ"},
        CommandEnumMember<Key>{Key::ScaleX, "ScaleX"},
        CommandEnumMember<Key>{Key::ScaleY, "ScaleY"},
        CommandEnumMember<Key>{Key::PixelSnap, "PixelSnap"},
        CommandEnumMember<Key>{Key::TogglePixelSnap, "TogglePixelSnap"},
        CommandEnumMember<Key>{Key::ZSort, "ZSort"},
        CommandEnumMember<Key>{Key::LockToRoot, "LockToRoot"},
        CommandEnumMember<Key>{Key::ToggleLockToRoot, "ToggleLockToRoot"},
        CommandEnumMember<Key>{Key::PinToMesh, "PinToMesh"},
        CommandEnumMember<Key>{Key::TogglePinToMesh, "TogglePinToMesh"},
        CommandEnumMember<Key>{Key::OffsetX, "OffsetX"},
        CommandEnumMember<Key>{Key::OffsetY, "OffsetY"},
        CommandEnumMember<Key>{Key::ViewportOrigin, "ViewportOrigin"},
        CommandEnumMember<Key>{Key::CompositeTint, "CompositeTint"},
        CommandEnumMember<Key>{Key::CompositeScreenTint, "CompositeScreenTint"},
        CommandEnumMember<Key>{Key::CompositeBlendingMode, "CompositeBlendingMode"},
        CommandEnumMember<Key>{Key::CompositeOpacity, "CompositeOpacity"},
        CommandEnumMember<Key>{Key::CompositeThreshold, "CompositeThreshold"},
        CommandEnumMember<Key>{Key::PartTint, "PartTint"},
        CommandEnumMember<Key>{Key::PartScreenTint, "PartScreenTint"},
        CommandEnumMember<Key>{Key::PartEmissionStrength, "PartEmissionStrength"},
        CommandEnumMember<Key>{Key::PartBlendingMode, "PartBlendingMode"},
        CommandEnumMember<Key>{Key::PartOpacity, "PartOpacity"},
        CommandEnumMember<Key>{Key::PartMaskAlphaThreshold, "PartMaskAlphaThreshold"},
        CommandEnumMember<Key>{Key::PartAutoResizedMesh, "PartAutoResizedMesh"},
        CommandEnumMember<Key>{Key::TogglePartAutoResizedMesh, "TogglePartAutoResizedMesh"},
        CommandEnumMember<Key>{Key::MeshGroupDynamic, "MeshGroupDynamic"},
        CommandEnumMember<Key>{Key::ToggleMeshGroupDynamic, "ToggleMeshGroupDynamic"},
        CommandEnumMember<Key>{Key::MeshGroupTranslateChildren, "MeshGroupTranslateChildren"},
        CommandEnumMember<Key>{Key::ToggleMeshGroupTranslateChildren, "ToggleMeshGroupTranslateChildren"},
        CommandEnumMember<Key>{Key::GridDeformDynamic, "GridDeformDynamic"},
        CommandEnumMember<Key>{Key::ToggleGridDeformDynamic, "ToggleGridDeformDynamic"},
        CommandEnumMember<Key>{Key::PathDeformDynamic, "PathDeformDynamic"},
        CommandEnumMember<Key>{Key::TogglePathDeformDynamic, "TogglePathDeformDynamic"},
        CommandEnumMember<Key>{Key::PathDeformPhysicsEnabled, "PathDeformPhysicsEnabled"},
        CommandEnumMember<Key>{Key::TogglePathDeformPhysicsEnabled, "TogglePathDeformPhysicsEnabled"},
        CommandEnumMember<Key>{Key::PathDeformCurveType, "PathDeformCurveType"},
        CommandEnumMember<Key>{Key::PathDeformGravity, "PathDeformGravity"},
        CommandEnumMember<Key>{Key::PathDeformRestoreConstant, "PathDeformRestoreConstant"},
        CommandEnumMember<Key>{Key::PathDeformDamping, "PathDeformDamping"},
        CommandEnumMember<Key>{Key::PathDeformInputScale, "PathDeformInputScale"},
        CommandEnumMember<Key>{Key::PathDeformPropagateScale, "PathDeformPropagateScale"},
        CommandEnumMember<Key>{Key::SimplePhysicsModelType, "SimplePhysicsModelType"},
        CommandEnumMember<Key>{Key::SimplePhysicsMapMode, "SimplePhysicsMapMode"},
        CommandEnumMember<Key>{Key::SimplePhysicsLocalOnly, "SimplePhysicsLocalOnly"},
        CommandEnumMember<Key>{Key::ToggleSimplePhysicsLocalOnly, "ToggleSimplePhysicsLocalOnly"},
        CommandEnumMember<Key>{Key::SimplePhysicsGravity, "SimplePhysicsGravity"},
        CommandEnumMember<Key>{Key::SimplePhysicsLength, "SimplePhysicsLength"},
        CommandEnumMember<Key>{Key::SimplePhysicsFrequency, "SimplePhysicsFrequency"},
        CommandEnumMember<Key>{Key::SimplePhysicsAngleDamping, "SimplePhysicsAngleDamping"},
        CommandEnumMember<Key>{Key::SimplePhysicsLengthDamping, "SimplePhysicsLengthDamping"},
        CommandEnumMember<Key>{Key::SimplePhysicsOutputScaleX, "SimplePhysicsOutputScaleX"},
        CommandEnumMember<Key>{Key::SimplePhysicsOutputScaleY, "SimplePhysicsOutputScaleY"}
    };
};
template<> struct CommandMapKeyMetadata<node::mask::NodeMaskCommand> {
    using Key = node::mask::NodeMaskCommand;
    inline static constexpr std::string_view typeName = "NodeMaskCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::AddMask, "AddMask"},
        CommandEnumMember<Key>{Key::RemoveMask, "RemoveMask"},
        CommandEnumMember<Key>{Key::ChangeMaskMode, "ChangeMaskMode"}
    };
};
template<> struct CommandMapKeyMetadata<node::welding::NodeWeldingCommand> {
    using Key = node::welding::NodeWeldingCommand;
    inline static constexpr std::string_view typeName = "NodeWeldingCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::AddWelding, "AddWelding"},
        CommandEnumMember<Key>{Key::RemoveWelding, "RemoveWelding"},
        CommandEnumMember<Key>{Key::ChangeWeldingWeight, "ChangeWeldingWeight"}
    };
};
template<> struct CommandMapKeyMetadata<node::simplephysics::NodeSimplePhysicsCommand> {
    using Key = node::simplephysics::NodeSimplePhysicsCommand;
    inline static constexpr std::string_view typeName = "NodeSimplePhysicsCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::SetSimplePhysicsParameter, "SetSimplePhysicsParameter"},
        CommandEnumMember<Key>{Key::ClearSimplePhysicsParameter, "ClearSimplePhysicsParameter"},
        CommandEnumMember<Key>{Key::SetSimplePhysicsModelType, "SetSimplePhysicsModelType"},
        CommandEnumMember<Key>{Key::SetSimplePhysicsMapMode, "SetSimplePhysicsMapMode"},
        CommandEnumMember<Key>{Key::SetSimplePhysicsLocalOnly, "SetSimplePhysicsLocalOnly"},
        CommandEnumMember<Key>{Key::SetSimplePhysicsGravity, "SetSimplePhysicsGravity"},
        CommandEnumMember<Key>{Key::SetSimplePhysicsLength, "SetSimplePhysicsLength"},
        CommandEnumMember<Key>{Key::SetSimplePhysicsFrequency, "SetSimplePhysicsFrequency"},
        CommandEnumMember<Key>{Key::SetSimplePhysicsAngleDamping, "SetSimplePhysicsAngleDamping"},
        CommandEnumMember<Key>{Key::SetSimplePhysicsLengthDamping, "SetSimplePhysicsLengthDamping"},
        CommandEnumMember<Key>{Key::SetSimplePhysicsOutputScaleX, "SetSimplePhysicsOutputScaleX"},
        CommandEnumMember<Key>{Key::SetSimplePhysicsOutputScaleY, "SetSimplePhysicsOutputScaleY"}
    };
};
template<> struct CommandMapKeyMetadata<::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode> {
    using Key = ::nijigenerate::viewport::common::mesheditor::tools::enums::VertexToolMode;
    inline static constexpr std::string_view typeName = "VertexToolMode";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::Points, "Points"},
        CommandEnumMember<Key>{Key::Connect, "Connect"},
        CommandEnumMember<Key>{Key::PathDeform, "PathDeform"},
        CommandEnumMember<Key>{Key::Grid, "Grid"},
        CommandEnumMember<Key>{Key::Brush, "Brush"},
        CommandEnumMember<Key>{Key::BezierDeform, "BezierDeform"},
        CommandEnumMember<Key>{Key::AltGridDeform, "AltGridDeform"},
        CommandEnumMember<Key>{Key::AltBezierDeform, "AltBezierDeform"},
        CommandEnumMember<Key>{Key::LassoSelection, "LassoSelection"},
        CommandEnumMember<Key>{Key::EdgeCutter, "EdgeCutter"}
    };
};
template<> struct CommandMapKeyMetadata<automesh::config::AutoMeshConfigCommand> {
    using Key = automesh::config::AutoMeshConfigCommand;
    inline static constexpr std::string_view typeName = "AutoMeshConfigCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::AutoMeshListProcessors, "AutoMeshListProcessors"},
        CommandEnumMember<Key>{Key::AutoMeshGetActive, "AutoMeshGetActive"},
        CommandEnumMember<Key>{Key::AutoMeshSetActive, "AutoMeshSetActive"},
        CommandEnumMember<Key>{Key::AutoMeshApplyActive, "AutoMeshApplyActive"},
        CommandEnumMember<Key>{Key::AutoMeshGetSchema, "AutoMeshGetSchema"},
        CommandEnumMember<Key>{Key::AutoMeshGetValues, "AutoMeshGetValues"},
        CommandEnumMember<Key>{Key::AutoMeshSetPreset, "AutoMeshSetPreset"},
        CommandEnumMember<Key>{Key::AutoMeshSetValues, "AutoMeshSetValues"}
    };
};
template<> struct CommandMapKeyMetadata<automesh::config::AutoMeshTypedCommand> {
    using Key = automesh::config::AutoMeshTypedCommand;
    inline static constexpr std::string_view typeName = "AutoMeshTypedCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::AutoMeshSetSimple_OptimumAutoMeshProcessor, "AutoMeshSetSimple_OptimumAutoMeshProcessor"},
        CommandEnumMember<Key>{Key::AutoMeshSetAdvanced_OptimumAutoMeshProcessor, "AutoMeshSetAdvanced_OptimumAutoMeshProcessor"},
        CommandEnumMember<Key>{Key::AutoMeshSetPreset_OptimumAutoMeshProcessor, "AutoMeshSetPreset_OptimumAutoMeshProcessor"},
        CommandEnumMember<Key>{Key::AutoMeshSetSimple_ContourAutoMeshProcessor, "AutoMeshSetSimple_ContourAutoMeshProcessor"},
        CommandEnumMember<Key>{Key::AutoMeshSetAdvanced_ContourAutoMeshProcessor, "AutoMeshSetAdvanced_ContourAutoMeshProcessor"},
        CommandEnumMember<Key>{Key::AutoMeshSetPreset_ContourAutoMeshProcessor, "AutoMeshSetPreset_ContourAutoMeshProcessor"},
        CommandEnumMember<Key>{Key::AutoMeshSetSimple_GridAutoMeshProcessor, "AutoMeshSetSimple_GridAutoMeshProcessor"},
        CommandEnumMember<Key>{Key::AutoMeshSetAdvanced_GridAutoMeshProcessor, "AutoMeshSetAdvanced_GridAutoMeshProcessor"},
        CommandEnumMember<Key>{Key::AutoMeshSetPreset_GridAutoMeshProcessor, "AutoMeshSetPreset_GridAutoMeshProcessor"},
        CommandEnumMember<Key>{Key::AutoMeshSetSimple_SkeletonExtractor, "AutoMeshSetSimple_SkeletonExtractor"},
        CommandEnumMember<Key>{Key::AutoMeshSetAdvanced_SkeletonExtractor, "AutoMeshSetAdvanced_SkeletonExtractor"},
        CommandEnumMember<Key>{Key::AutoMeshSetPreset_SkeletonExtractor, "AutoMeshSetPreset_SkeletonExtractor"}
    };
};
template<> struct CommandMapKeyMetadata<vertex::define_mesh::VertexCommand> {
    using Key = vertex::define_mesh::VertexCommand;
    inline static constexpr std::string_view typeName = "VertexCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::DefineMesh, "DefineMesh"},
        CommandEnumMember<Key>{Key::DefineVertices, "DefineVertices"},
        CommandEnumMember<Key>{Key::DefineGrid, "DefineGrid"}
    };
};
template<> struct CommandMapKeyMetadata<model::set_deform_binding::ModelCommand> {
    using Key = model::set_deform_binding::ModelCommand;
    inline static constexpr std::string_view typeName = "ModelCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::SetDeformBinding, "SetDeformBinding"},
        CommandEnumMember<Key>{Key::SetTRSBinding, "SetTRSBinding"},
        CommandEnumMember<Key>{Key::SetRotationYBinding, "SetRotationYBinding"},
        CommandEnumMember<Key>{Key::SetRotationXBinding, "SetRotationXBinding"}
    };
};
template<> struct CommandMapKeyMetadata<depth::bone::DepthBoneCommand> {
    using Key = depth::bone::DepthBoneCommand;
    inline static constexpr std::string_view typeName = "DepthBoneCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::CreateDepthRigRoot, "CreateDepthRigRoot"},
        CommandEnumMember<Key>{Key::AddDepthBone, "AddDepthBone"},
        CommandEnumMember<Key>{Key::AddStandardDepthSkeleton, "AddStandardDepthSkeleton"},
        CommandEnumMember<Key>{Key::AddStandardDepthParameters, "AddStandardDepthParameters"},
        CommandEnumMember<Key>{Key::FitDepthRigRootZToDepth, "FitDepthRigRootZToDepth"},
        CommandEnumMember<Key>{Key::FitDepthBoneZToDepth, "FitDepthBoneZToDepth"},
        CommandEnumMember<Key>{Key::SetDepthBoneRest, "SetDepthBoneRest"},
        CommandEnumMember<Key>{Key::SetDepthBoneConstraint, "SetDepthBoneConstraint"},
        CommandEnumMember<Key>{Key::ListDepthBones, "ListDepthBones"},
        CommandEnumMember<Key>{Key::AddDepthBoneSource, "AddDepthBoneSource"},
        CommandEnumMember<Key>{Key::RemoveDepthBoneSource, "RemoveDepthBoneSource"},
        CommandEnumMember<Key>{Key::ListDepthBoneSources, "ListDepthBoneSources"},
        CommandEnumMember<Key>{Key::SetDepthBoneSourceSettings, "SetDepthBoneSourceSettings"},
        CommandEnumMember<Key>{Key::SetDepthBoneInfluenceRule, "SetDepthBoneInfluenceRule"},
        CommandEnumMember<Key>{Key::GetDepthBoneInfluenceRule, "GetDepthBoneInfluenceRule"},
        CommandEnumMember<Key>{Key::PreviewDepthBoneInfluence, "PreviewDepthBoneInfluence"},
        CommandEnumMember<Key>{Key::PreviewDepthBoneDeform, "PreviewDepthBoneDeform"},
        CommandEnumMember<Key>{Key::ApplyDepthBoneDeform, "ApplyDepthBoneDeform"}
    };
};
template<> struct CommandMapKeyMetadata<depth::editor::DepthEditorOperationCommand> {
    using Key = depth::editor::DepthEditorOperationCommand;
    inline static constexpr std::string_view typeName = "DepthEditorOperationCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::AddEditorDepthOp, "AddEditorDepthOp"},
        CommandEnumMember<Key>{Key::UpdateEditorDepthOp, "UpdateEditorDepthOp"},
        CommandEnumMember<Key>{Key::RemoveEditorDepthOp, "RemoveEditorDepthOp"}
    };
};
template<> struct CommandMapKeyMetadata<depth::map::DepthMapCommand> {
    using Key = depth::map::DepthMapCommand;
    inline static constexpr std::string_view typeName = "DepthMapCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::ListDepths, "ListDepths"},
        CommandEnumMember<Key>{Key::SetDepths, "SetDepths"},
        CommandEnumMember<Key>{Key::ClearDepths, "ClearDepths"},
        CommandEnumMember<Key>{Key::ListDepthOps, "ListDepthOps"},
        CommandEnumMember<Key>{Key::SetDepthOps, "SetDepthOps"},
        CommandEnumMember<Key>{Key::AddDepthOp, "AddDepthOp"},
        CommandEnumMember<Key>{Key::UpdateDepthOp, "UpdateDepthOp"},
        CommandEnumMember<Key>{Key::RemoveDepthOp, "RemoveDepthOp"},
        CommandEnumMember<Key>{Key::MoveDepthOp, "MoveDepthOp"},
        CommandEnumMember<Key>{Key::ClearDepthOps, "ClearDepthOps"},
        CommandEnumMember<Key>{Key::ApplyDepthOps, "ApplyDepthOps"},
        CommandEnumMember<Key>{Key::ImportPSDDepths, "ImportPSDDepths"}
    };
};
template<> struct CommandMapKeyMetadata<depth::psd_dialog::PsdDepthDialogCommand> {
    using Key = depth::psd_dialog::PsdDepthDialogCommand;
    inline static constexpr std::string_view typeName = "PsdDepthDialogCommand";
    inline static constexpr std::array members{
        CommandEnumMember<Key>{Key::InspectPsdDepthDialog, "InspectPsdDepthDialog"},
        CommandEnumMember<Key>{Key::GetPsdDepthDialogPartData, "GetPsdDepthDialogPartData"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogColorSource, "SetPsdDepthDialogColorSource"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogInvert, "SetPsdDepthDialogInvert"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogBackDepth, "SetPsdDepthDialogBackDepth"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogFrontDepth, "SetPsdDepthDialogFrontDepth"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogDepthScale, "SetPsdDepthDialogDepthScale"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogChannel, "SetPsdDepthDialogChannel"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogSampling, "SetPsdDepthDialogSampling"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogCustomRadius, "SetPsdDepthDialogCustomRadius"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogAlphaThreshold, "SetPsdDepthDialogAlphaThreshold"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogMissingPolicy, "SetPsdDepthDialogMissingPolicy"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogContourRepair, "SetPsdDepthDialogContourRepair"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogSurfaceSmoothing, "SetPsdDepthDialogSurfaceSmoothing"},
        CommandEnumMember<Key>{Key::FillPsdDepthDialogAlphaDepthGaps, "FillPsdDepthDialogAlphaDepthGaps"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogGpuComposition, "SetPsdDepthDialogGpuComposition"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogDirectGridMatch, "SetPsdDepthDialogDirectGridMatch"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogProblemFilter, "SetPsdDepthDialogProblemFilter"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogLayerMappingAuto, "SetPsdDepthDialogLayerMappingAuto"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogLayerMappingIgnored, "SetPsdDepthDialogLayerMappingIgnored"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogLayerMappingTarget, "SetPsdDepthDialogLayerMappingTarget"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogTargetEnabled, "SetPsdDepthDialogTargetEnabled"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogLayerEnabled, "SetPsdDepthDialogLayerEnabled"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogLayerVisible, "SetPsdDepthDialogLayerVisible"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogLayerDepthEnabled, "SetPsdDepthDialogLayerDepthEnabled"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogLayerDepthInverted, "SetPsdDepthDialogLayerDepthInverted"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogLayerDepthScale, "SetPsdDepthDialogLayerDepthScale"},
        CommandEnumMember<Key>{Key::SetPsdDepthDialogLayerDepthOffset, "SetPsdDepthDialogLayerDepthOffset"},
        CommandEnumMember<Key>{Key::ResetPsdDepthDialogLayerDepthTransform, "ResetPsdDepthDialogLayerDepthTransform"},
        CommandEnumMember<Key>{Key::ApplyPsdDepthDialog, "ApplyPsdDepthDialog"},
        CommandEnumMember<Key>{Key::CancelPsdDepthDialog, "CancelPsdDepthDialog"}
    };
};
}
