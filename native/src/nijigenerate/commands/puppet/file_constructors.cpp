#include "file_commands.hpp"
#include <nijigenerate/core/translation.hpp>
#include <compat/object_metadata.hpp>
namespace nijigenerate::commands::puppet::file {
namespace {
template<std::size_t N> compat::RetainedString tr(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}
}
detail::FileCommandText NewFileCommand::sourceConstructorText() {
    return {tr("New"), tr("Create new project.")};
}
NewFileCommand::NewFileCommand()
    : NewFileCommand(sourceConstructorText()) {}
NewFileCommand::NewFileCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(NewFileCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.NewFileCommand")); }

detail::FileCommandText ShowOpenFileDialogCommand::sourceConstructorText() {
    return {tr("Open"), tr("Show \"Open\" dialog.")};
}
ShowOpenFileDialogCommand::ShowOpenFileDialogCommand()
    : ShowOpenFileDialogCommand(sourceConstructorText()) {}
ShowOpenFileDialogCommand::ShowOpenFileDialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowOpenFileDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowOpenFileDialogCommand")); }

detail::FileCommandText OpenFileCommand::sourceConstructorText() {
    return {tr("Open from file path"), tr("Open puppet from specified file.")};
}
OpenFileCommand::OpenFileCommand(compat::RetainedString file)
    : OpenFileCommand(sourceConstructorText(), file) {}
OpenFileCommand::OpenFileCommand(detail::FileCommandText text, compat::RetainedString file)
    : NativeBase{std::move(text.label), std::move(text.description), file} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(OpenFileCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.OpenFileCommand")); }

detail::FileCommandText ShowSaveFileDialogCommand::sourceConstructorText() {
    return {tr("Save"), tr("Show \"Save\" dialog.")};
}
ShowSaveFileDialogCommand::ShowSaveFileDialogCommand()
    : ShowSaveFileDialogCommand(sourceConstructorText()) {}
ShowSaveFileDialogCommand::ShowSaveFileDialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowSaveFileDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowSaveFileDialogCommand")); }

detail::FileCommandText SaveFileCommand::sourceConstructorText() {
    return {tr("Save to file path"), tr("Save puppet to specified file.")};
}
SaveFileCommand::SaveFileCommand(compat::RetainedString file)
    : SaveFileCommand(sourceConstructorText(), file) {}
SaveFileCommand::SaveFileCommand(detail::FileCommandText text, compat::RetainedString file)
    : ExCommand<SaveFileCommandArguments>{std::move(text.label), std::move(text.description), file} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(SaveFileCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.SaveFileCommand")); }

detail::FileCommandText ShowSaveFileAsDialogCommand::sourceConstructorText() {
    return {tr("Save As..."), tr("Show \"Save as\" dialog.")};
}
ShowSaveFileAsDialogCommand::ShowSaveFileAsDialogCommand()
    : ShowSaveFileAsDialogCommand(sourceConstructorText()) {}
ShowSaveFileAsDialogCommand::ShowSaveFileAsDialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowSaveFileAsDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowSaveFileAsDialogCommand")); }

detail::FileCommandText ShowImportPSDDialogCommand::sourceConstructorText() {
    return {tr("Import Photoshop Document"), tr("Show \"Import Photoshop Document\" dialog.")};
}
ShowImportPSDDialogCommand::ShowImportPSDDialogCommand()
    : ShowImportPSDDialogCommand(sourceConstructorText()) {}
ShowImportPSDDialogCommand::ShowImportPSDDialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowImportPSDDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowImportPSDDialogCommand")); }

detail::FileCommandText ShowImportPSDDepthMapDialogCommand::sourceConstructorText() {
    return {tr("Import PSD Depth Map"), tr("Show \"Import PSD Depth Map\" dialog.")};
}
ShowImportPSDDepthMapDialogCommand::ShowImportPSDDepthMapDialogCommand()
    : ShowImportPSDDepthMapDialogCommand(sourceConstructorText()) {}
ShowImportPSDDepthMapDialogCommand::ShowImportPSDDepthMapDialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowImportPSDDepthMapDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowImportPSDDepthMapDialogCommand")); }

detail::FileCommandText OpenPSDDepthMapDialogCommand::sourceConstructorText() {
    return {tr("Open PSD Depth Map Dialog"), tr("Open the PSD depth map import dialog for a specified source path.")};
}
OpenPSDDepthMapDialogCommand::OpenPSDDepthMapDialogCommand(compat::RetainedString path)
    : OpenPSDDepthMapDialogCommand(sourceConstructorText(), path) {}
OpenPSDDepthMapDialogCommand::OpenPSDDepthMapDialogCommand(detail::FileCommandText text, compat::RetainedString path)
    : ExCommand<OpenPSDDepthMapDialogCommandArguments>{std::move(text.label), std::move(text.description), path} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(OpenPSDDepthMapDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.OpenPSDDepthMapDialogCommand")); }

detail::FileCommandText ImportPSDCommand::sourceConstructorText() {
    return {tr("Import Photoshop Document from file path"), tr("Import a PSD file without showing a dialog.")};
}
ImportPSDCommand::ImportPSDCommand(compat::RetainedString path, bool keepStructure, compat::RetainedString layerGroupNodeType)
    : ImportPSDCommand(sourceConstructorText(), path, keepStructure, layerGroupNodeType) {}
ImportPSDCommand::ImportPSDCommand(detail::FileCommandText text, compat::RetainedString path, bool keepStructure, compat::RetainedString layerGroupNodeType)
    : NativeBase{std::move(text.label), std::move(text.description), path, keepStructure, layerGroupNodeType} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ImportPSDCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ImportPSDCommand")); }

detail::FileCommandText ShowImportKRADialogCommand::sourceConstructorText() {
    return {tr("Import Krita Document"), tr("Show \"Import Krita Document\" dialog.")};
}
ShowImportKRADialogCommand::ShowImportKRADialogCommand()
    : ShowImportKRADialogCommand(sourceConstructorText()) {}
ShowImportKRADialogCommand::ShowImportKRADialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowImportKRADialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowImportKRADialogCommand")); }

detail::FileCommandText ImportKRACommand::sourceConstructorText() {
    return {tr("Import Krita Document from file path"), tr("Import a KRA file without showing a dialog.")};
}
ImportKRACommand::ImportKRACommand(compat::RetainedString path, bool keepStructure, compat::RetainedString layerGroupNodeType)
    : ImportKRACommand(sourceConstructorText(), path, keepStructure, layerGroupNodeType) {}
ImportKRACommand::ImportKRACommand(detail::FileCommandText text, compat::RetainedString path, bool keepStructure, compat::RetainedString layerGroupNodeType)
    : NativeBase{std::move(text.label), std::move(text.description), path, keepStructure, layerGroupNodeType} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ImportKRACommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ImportKRACommand")); }

detail::FileCommandText ShowImportINPDialogCommand::sourceConstructorText() {
    return {tr("Import nijilive puppet"), tr("Show \"Import nijilive puppet file\" dialog.")};
}
ShowImportINPDialogCommand::ShowImportINPDialogCommand()
    : ShowImportINPDialogCommand(sourceConstructorText()) {}
ShowImportINPDialogCommand::ShowImportINPDialogCommand(detail::FileCommandText text)
    : NativeBase{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowImportINPDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowImportINPDialogCommand")); }

detail::FileCommandText ImportINPCommand::sourceConstructorText() {
    return {tr("Import nijilive puppet from file path"), tr("Import an INP file without showing a dialog.")};
}
ImportINPCommand::ImportINPCommand(compat::RetainedString path)
    : ImportINPCommand(sourceConstructorText(), path) {}
ImportINPCommand::ImportINPCommand(detail::FileCommandText text, compat::RetainedString path)
    : NativeBase{std::move(text.label), std::move(text.description), path} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ImportINPCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ImportINPCommand")); }

detail::FileCommandText ShowImportImageFolderDialogCommand::sourceConstructorText() {
    return {tr("Import Image Folder"), tr("Show \"Import Image Folder\" dialog.")};
}
ShowImportImageFolderDialogCommand::ShowImportImageFolderDialogCommand()
    : ShowImportImageFolderDialogCommand(sourceConstructorText()) {}
ShowImportImageFolderDialogCommand::ShowImportImageFolderDialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowImportImageFolderDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowImportImageFolderDialogCommand")); }

detail::FileCommandText ImportImageFolderCommand::sourceConstructorText() {
    return {tr("Import Image Folder from file path"), tr("Import a folder of image files without showing a dialog.")};
}
ImportImageFolderCommand::ImportImageFolderCommand(compat::RetainedString path)
    : ImportImageFolderCommand(sourceConstructorText(), path) {}
ImportImageFolderCommand::ImportImageFolderCommand(detail::FileCommandText text, compat::RetainedString path)
    : ExCommand<ImportImageFolderCommandArguments>{std::move(text.label), std::move(text.description), path} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ImportImageFolderCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ImportImageFolderCommand")); }

detail::FileCommandText ShowMergePSDDialogCommand::sourceConstructorText() {
    return {tr("Merge Photoshop Document"), tr("Show \"Merge Photoshop Document\" dialog.")};
}
ShowMergePSDDialogCommand::ShowMergePSDDialogCommand()
    : ShowMergePSDDialogCommand(sourceConstructorText()) {}
ShowMergePSDDialogCommand::ShowMergePSDDialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowMergePSDDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowMergePSDDialogCommand")); }

detail::FileCommandText MergePSDCommand::sourceConstructorText() {
    return {tr("Merge Photoshop Document from file path"), tr("Merge a PSD file into the current puppet using default mapping.")};
}
MergePSDCommand::MergePSDCommand(compat::RetainedString path, bool renameMapped, bool retranslateMapped)
    : MergePSDCommand(sourceConstructorText(), path, renameMapped, retranslateMapped) {}
MergePSDCommand::MergePSDCommand(detail::FileCommandText text, compat::RetainedString path, bool renameMapped, bool retranslateMapped)
    : ExCommand<MergePSDCommandArguments>{std::move(text.label), std::move(text.description), path, renameMapped, retranslateMapped} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(MergePSDCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.MergePSDCommand")); }

detail::FileCommandText ShowMergeKRADialogCommand::sourceConstructorText() {
    return {tr("Merge Krita Document"), tr("Show \"Merge Krita Document\" dialog.")};
}
ShowMergeKRADialogCommand::ShowMergeKRADialogCommand()
    : ShowMergeKRADialogCommand(sourceConstructorText()) {}
ShowMergeKRADialogCommand::ShowMergeKRADialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowMergeKRADialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowMergeKRADialogCommand")); }

detail::FileCommandText MergeKRACommand::sourceConstructorText() {
    return {tr("Merge Krita Document from file path"), tr("Merge a KRA file into the current puppet using default mapping.")};
}
MergeKRACommand::MergeKRACommand(compat::RetainedString path, bool renameMapped, bool retranslateMapped)
    : MergeKRACommand(sourceConstructorText(), path, renameMapped, retranslateMapped) {}
MergeKRACommand::MergeKRACommand(detail::FileCommandText text, compat::RetainedString path, bool renameMapped, bool retranslateMapped)
    : ExCommand<MergeKRACommandArguments>{std::move(text.label), std::move(text.description), path, renameMapped, retranslateMapped} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(MergeKRACommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.MergeKRACommand")); }

detail::FileCommandText ShowMergeImageFileDialogCommand::sourceConstructorText() {
    return {tr("Merge Image Files"), tr("Show \"Merge image files\" dialog.")};
}
ShowMergeImageFileDialogCommand::ShowMergeImageFileDialogCommand()
    : ShowMergeImageFileDialogCommand(sourceConstructorText()) {}
ShowMergeImageFileDialogCommand::ShowMergeImageFileDialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowMergeImageFileDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowMergeImageFileDialogCommand")); }

detail::FileCommandText MergeImageFilesCommand::sourceConstructorText() {
    return {tr("Merge Image Files from file paths"), tr("Merge image files into the current puppet without showing a dialog.")};
}
MergeImageFilesCommand::MergeImageFilesCommand(compat::RetainedString paths)
    : MergeImageFilesCommand(sourceConstructorText(), paths) {}
MergeImageFilesCommand::MergeImageFilesCommand(detail::FileCommandText text, compat::RetainedString paths)
    : ExCommand<MergeImageFilesCommandArguments>{std::move(text.label), std::move(text.description), paths} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(MergeImageFilesCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.MergeImageFilesCommand")); }

detail::FileCommandText ShowMergeINPDialogCommand::sourceConstructorText() {
    return {tr("Merge nijigenerate project"), tr("Show \"Merge nijilive puppet file\" dialog.")};
}
ShowMergeINPDialogCommand::ShowMergeINPDialogCommand()
    : ShowMergeINPDialogCommand(sourceConstructorText()) {}
ShowMergeINPDialogCommand::ShowMergeINPDialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowMergeINPDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowMergeINPDialogCommand")); }

detail::FileCommandText MergeINPCommand::sourceConstructorText() {
    return {tr("Merge nijigenerate project from file path"), tr("Merge an INP project into the current puppet without showing a dialog.")};
}
MergeINPCommand::MergeINPCommand(compat::RetainedString path, bool mergeParameters, bool mergeAnimations)
    : MergeINPCommand(sourceConstructorText(), path, mergeParameters, mergeAnimations) {}
MergeINPCommand::MergeINPCommand(detail::FileCommandText text, compat::RetainedString path, bool mergeParameters, bool mergeAnimations)
    : ExCommand<MergeINPCommandArguments>{std::move(text.label), std::move(text.description), path, mergeParameters, mergeAnimations} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(MergeINPCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.MergeINPCommand")); }

detail::FileCommandText ShowExportToINPDialogCommand::sourceConstructorText() {
    return {tr("Export nijilive puppet"), tr("Show \"Export to nijilive puppet\" dialog.")};
}
ShowExportToINPDialogCommand::ShowExportToINPDialogCommand()
    : ShowExportToINPDialogCommand(sourceConstructorText()) {}
ShowExportToINPDialogCommand::ShowExportToINPDialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowExportToINPDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowExportToINPDialogCommand")); }

detail::FileCommandText ExportINPCommand::sourceConstructorText() {
    return {tr("Export nijilive puppet to file path"), tr("Export an INP file without showing a dialog.")};
}
ExportINPCommand::ExportINPCommand(compat::RetainedString file, std::size_t atlasResolution, bool nonLinearScaling, float scale, std::int32_t padding, bool optimizePruneUnused)
    : ExportINPCommand(sourceConstructorText(), file, atlasResolution, nonLinearScaling, scale, padding, optimizePruneUnused) {}
ExportINPCommand::ExportINPCommand(detail::FileCommandText text, compat::RetainedString file, std::size_t atlasResolution, bool nonLinearScaling, float scale, std::int32_t padding, bool optimizePruneUnused)
    : ExCommand<ExportINPCommandArguments>{std::move(text.label), std::move(text.description), file, atlasResolution, nonLinearScaling, scale, padding, optimizePruneUnused} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExportINPCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ExportINPCommand")); }

detail::FileCommandText ShowExportToPNGDialogCommand::sourceConstructorText() {
    return {tr("Export PNG (*.png)"), tr("Show \"Export to png image\" dialog.")};
}
ShowExportToPNGDialogCommand::ShowExportToPNGDialogCommand()
    : ShowExportToPNGDialogCommand(sourceConstructorText()) {}
ShowExportToPNGDialogCommand::ShowExportToPNGDialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowExportToPNGDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowExportToPNGDialogCommand")); }

detail::FileCommandText ExportPNGCommand::sourceConstructorText() {
    return {tr("Export PNG to file path"), tr("Export a PNG image without showing a dialog.")};
}
ExportPNGCommand::ExportPNGCommand(compat::RetainedString file, compat::RetainedString cameraName, bool transparency, bool postprocessing)
    : ExportPNGCommand(sourceConstructorText(), file, cameraName, transparency, postprocessing) {}
ExportPNGCommand::ExportPNGCommand(detail::FileCommandText text, compat::RetainedString file, compat::RetainedString cameraName, bool transparency, bool postprocessing)
    : ExCommand<ExportPNGCommandArguments>{std::move(text.label), std::move(text.description), file, cameraName, transparency, postprocessing} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExportPNGCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ExportPNGCommand")); }

detail::FileCommandText ShowExportToJpegDialogCommand::sourceConstructorText() {
    return {tr("Export JPEG (*.jpeg)"), tr("Show \"Export to jpeg image\" dialog.")};
}
ShowExportToJpegDialogCommand::ShowExportToJpegDialogCommand()
    : ShowExportToJpegDialogCommand(sourceConstructorText()) {}
ShowExportToJpegDialogCommand::ShowExportToJpegDialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowExportToJpegDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowExportToJpegDialogCommand")); }

detail::FileCommandText ExportJPEGCommand::sourceConstructorText() {
    return {tr("Export JPEG to file path"), tr("Export a JPEG image without showing a dialog.")};
}
ExportJPEGCommand::ExportJPEGCommand(compat::RetainedString file, compat::RetainedString cameraName, bool transparency, bool postprocessing)
    : ExportJPEGCommand(sourceConstructorText(), file, cameraName, transparency, postprocessing) {}
ExportJPEGCommand::ExportJPEGCommand(detail::FileCommandText text, compat::RetainedString file, compat::RetainedString cameraName, bool transparency, bool postprocessing)
    : ExCommand<ExportJPEGCommandArguments>{std::move(text.label), std::move(text.description), file, cameraName, transparency, postprocessing} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExportJPEGCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ExportJPEGCommand")); }

detail::FileCommandText ShowExportToTGADialogCommand::sourceConstructorText() {
    return {tr("Export TARGA (*.tga)"), tr("Show \"Export to TGA image\" dialog.")};
}
ShowExportToTGADialogCommand::ShowExportToTGADialogCommand()
    : ShowExportToTGADialogCommand(sourceConstructorText()) {}
ShowExportToTGADialogCommand::ShowExportToTGADialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowExportToTGADialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowExportToTGADialogCommand")); }

detail::FileCommandText ExportTGACommand::sourceConstructorText() {
    return {tr("Export TGA to file path"), tr("Export a TGA image without showing a dialog.")};
}
ExportTGACommand::ExportTGACommand(compat::RetainedString file, compat::RetainedString cameraName, bool transparency, bool postprocessing)
    : ExportTGACommand(sourceConstructorText(), file, cameraName, transparency, postprocessing) {}
ExportTGACommand::ExportTGACommand(detail::FileCommandText text, compat::RetainedString file, compat::RetainedString cameraName, bool transparency, bool postprocessing)
    : ExCommand<ExportTGACommandArguments>{std::move(text.label), std::move(text.description), file, cameraName, transparency, postprocessing} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExportTGACommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ExportTGACommand")); }

detail::FileCommandText ShowExportToVideoDialogCommand::sourceConstructorText() {
    return {tr("Export Video"), tr("Show \"Export to video\" dialog.")};
}
ShowExportToVideoDialogCommand::ShowExportToVideoDialogCommand()
    : ShowExportToVideoDialogCommand(sourceConstructorText()) {}
ShowExportToVideoDialogCommand::ShowExportToVideoDialogCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ShowExportToVideoDialogCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ShowExportToVideoDialogCommand")); }

detail::FileCommandText ExportVideoCommand::sourceConstructorText() {
    return {tr("Export Video to file path"), tr("Export a video without showing a dialog.")};
}
ExportVideoCommand::ExportVideoCommand(compat::RetainedString file, compat::RetainedString animationName, compat::RetainedString cameraName, compat::RetainedString codec, std::int32_t loops, std::int32_t framerate, bool transparency, bool postprocessing)
    : ExportVideoCommand(sourceConstructorText(), file, animationName, cameraName, codec, loops, framerate, transparency, postprocessing) {}
ExportVideoCommand::ExportVideoCommand(detail::FileCommandText text, compat::RetainedString file, compat::RetainedString animationName, compat::RetainedString cameraName, compat::RetainedString codec, std::int32_t loops, std::int32_t framerate, bool transparency, bool postprocessing)
    : ExCommand<ExportVideoCommandArguments>{std::move(text.label), std::move(text.description), file, animationName, cameraName, codec, loops, framerate, transparency, postprocessing} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ExportVideoCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.ExportVideoCommand")); }

detail::FileCommandText CloseProjectCommand::sourceConstructorText() {
    return {tr("Close Project"), tr("Close active puppet project.")};
}
CloseProjectCommand::CloseProjectCommand()
    : CloseProjectCommand(sourceConstructorText()) {}
CloseProjectCommand::CloseProjectCommand(detail::FileCommandText text)
    : ExCommand<CommandNoArguments>{std::move(text.label), std::move(text.description)} { nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(CloseProjectCommand),
    compat::RetainedString::fromStaticLiteral("nijigenerate.commands.puppet.file.CloseProjectCommand")); }

} // namespace nijigenerate::commands::puppet::file
