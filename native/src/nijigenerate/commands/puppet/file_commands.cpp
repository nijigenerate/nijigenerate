#include "file_commands.hpp"
#include "file_runtime.hpp"
#include <nijigenerate/io/save.hpp>
#include <nijigenerate/io/psd.hpp>
#include <nijigenerate/io/kra.hpp>
#include <nijigenerate/io/inimport.hpp>
#include <nijigenerate/io/project_reader.hpp>
#include <nijigenerate/io/inpexport.hpp>
#include <nijigenerate/io/imageexport.hpp>
#include <nijigenerate/project.hpp>
#include <nijigenerate/windows/psddepthmap.hpp>
#include <nijigenerate/windows/psdmerge.hpp>
#include <nijigenerate/windows/kramerge.hpp>
#include <nijigenerate/windows/imgexport.hpp>
#include <nijigenerate/windows/videoexport.hpp>
#include <nijigenerate/compat/exception_message.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <new>
namespace nijigenerate::commands::puppet::file {
using namespace detail;
using namespace project;
using namespace io;
using namespace io::save;
using namespace io::inimport;
using namespace io::inpexport;
using namespace io::imageexport;
using namespace windows::base;
using io::psd::incImportShowPSDDialog;
using io::kra::incImportShowKRADialog;
using windows::psddepthmap::PSDDepthMapWindow;
using windows::psdmerge::PSDMergeWindow;
using windows::kramerge::KRAMergeWindow;
using windows::imgexport::ImageExportWindow;
using windows::videoexport::VideoExportWindow;
using Load=LoadResult<std::shared_ptr<nicxlive::core::Puppet>>;
namespace {
struct LoadedPuppetSliceArgument {
    const std::shared_ptr<nicxlive::core::Puppet>& puppet;
    operator compat::Slice<std::shared_ptr<nicxlive::core::Puppet>>() const {
        return puppet ? compat::Slice<std::shared_ptr<nicxlive::core::Puppet>>{puppet}
            : compat::Slice<std::shared_ptr<nicxlive::core::Puppet>>{};
    }
};
}

std::shared_ptr<CommandResult> NewFileCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        incNewProjectAsk();
        return CommandResult::opCall(true);
    }

std::shared_ptr<CommandResult> ShowOpenFileDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        incFileOpen();
        return CommandResult::opCall(true);
    }

std::shared_ptr<OpenFileCommand::Result> OpenFileCommand::run(const std::shared_ptr<Context>& ctx) {
    return runTyped(ctx);
}
std::shared_ptr<OpenFileCommand::Result> OpenFileCommand::runTyped(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (!file.isNull()) {
            if (!incOpenProject(file)) {
                return Load::opCall(false, {}, "Puppet could not be loaded");
            }
            auto puppet = incActivePuppet();
            return std::allocate_shared<Load>(nicxlive::core::d_runtime::SourceAllocator<Load>{},
                true, LoadedPuppetSliceArgument{puppet}, compat::RetainedString::fromStaticLiteral("Puppet loaded"));
        }
        return Load::opCall(false, {}, "File path not provided");
    }

std::shared_ptr<CommandResult> ShowSaveFileDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        incFileSave();
        return CommandResult::opCall(true);
    }

std::shared_ptr<CommandResult> SaveFileCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (!file.isNull()) { return CommandResult::opCall(incSaveProject(file)); }
        return CommandResult::opCall(false, "File path not provided");
    }

std::shared_ptr<CommandResult> ShowSaveFileAsDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        incFileSaveAs();
        return CommandResult::opCall(true);
    }

std::shared_ptr<CommandResult> ShowImportPSDDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        incPopWelcomeWindow();
        incImportShowPSDDialog();
        return CommandResult::opCall(true);
    }

std::shared_ptr<CommandResult> ShowImportPSDDepthMapDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        compat::Slice<TFD_Filter> filters;
        filters.resize(1);
        const compat::Slice<const char*> patterns0{"*.psd", "*.png"};
        filters[0] = {{patterns0.data(), patterns0.size()}, "Depth Source (*.psd, *.png)"};

        compat::RetainedString path = incShowImportDialog(
            {filters.data(), filters.size()}, tr("Import PSD Depth Map...")).retainedString();
        if (!path.empty()) {
            ngPushWindow(ngCreateWindow<PSDDepthMapWindow>(path));
            return CommandResult::opCall(true);
        }
        return CommandResult::opCall(false, "Import canceled");
    }

std::shared_ptr<CommandResult> OpenPSDDepthMapDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (path.empty()) return CommandResult::opCall(false, "Path not provided");
        ngPushWindow(ngCreateWindow<PSDDepthMapWindow>(path));
        return CommandResult::opCall(true);
    }

std::shared_ptr<ImportPSDCommand::Result> ImportPSDCommand::run(const std::shared_ptr<Context>& ctx) {
    return runTyped(ctx);
}
std::shared_ptr<ImportPSDCommand::Result> ImportPSDCommand::runTyped(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (path.empty()) {
            return Load::opCall(false, {}, "Path not provided");
        }
        IncImportSettings settings;
        settings.keepStructure = keepStructure;
        settings.layerGroupNodeType = !layerGroupNodeType.empty() ? layerGroupNodeType : compat::RetainedString::fromStaticLiteral("DynamicComposite");
        incImport<io::psd::PSD>(path, settings);
        return ngLoadedActivePuppet("PSD imported");
    }

std::shared_ptr<CommandResult> ShowImportKRADialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        incPopWelcomeWindow();
        incImportShowKRADialog();
        return CommandResult::opCall(true);
    }

std::shared_ptr<ImportKRACommand::Result> ImportKRACommand::run(const std::shared_ptr<Context>& ctx) {
    return runTyped(ctx);
}
std::shared_ptr<ImportKRACommand::Result> ImportKRACommand::runTyped(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (path.empty()) {
            return Load::opCall(false, {}, "Path not provided");
        }
        IncImportSettings settings;
        settings.keepStructure = keepStructure;
        settings.layerGroupNodeType = !layerGroupNodeType.empty() ? layerGroupNodeType : compat::RetainedString::fromStaticLiteral("DynamicComposite");
        incImport<io::kra::KRA>(path, settings);
        return ngLoadedActivePuppet("KRA imported");
    }

std::shared_ptr<ShowImportINPDialogCommand::Result> ShowImportINPDialogCommand::run(const std::shared_ptr<Context>& ctx) {
    return runTyped(ctx);
}
std::shared_ptr<ShowImportINPDialogCommand::Result> ShowImportINPDialogCommand::runTyped(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        compat::Slice<TFD_Filter> filters;
        filters.resize(1);
        const compat::Slice<const char*> patterns0{"*.inp"};
        filters[0] = {{patterns0.data(), patterns0.size()}, "nijilive Puppet (*.inp)"};

        compat::RetainedString file = incShowOpenDialog(
            {filters.data(), filters.size()}, tr("Import...")).retainedString();
        if (!file.isNull()) {
            incImportINP(file);
            auto puppet = incActivePuppet();
            return std::allocate_shared<Load>(nicxlive::core::d_runtime::SourceAllocator<Load>{},
                true, LoadedPuppetSliceArgument{puppet}, compat::RetainedString::fromStaticLiteral("Puppet imported"));
        }
        return Load::opCall(false, {}, "Import canceled");
    }

std::shared_ptr<ImportINPCommand::Result> ImportINPCommand::run(const std::shared_ptr<Context>& ctx) {
    return runTyped(ctx);
}
std::shared_ptr<ImportINPCommand::Result> ImportINPCommand::runTyped(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (path.empty()) {
            return Load::opCall(false, {}, "Path not provided");
        }
        incImportINP(path);
        return ngLoadedActivePuppet("INP imported");
    }

std::shared_ptr<CommandResult> ShowImportImageFolderDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        compat::RetainedString folder = incShowOpenFolderDialog(tr("Select a Folder...")).retainedString();
        if (!folder.isNull()) {
            incImportFolder(folder);
            return CommandResult::opCall(true);
        }
        return CommandResult::opCall(false, "Import canceled");
    }

std::shared_ptr<CommandResult> ImportImageFolderCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (path.empty()) return ngMissingPathResult();
        incImportFolder(path);
        return CommandResult::opCall(true);
    }

std::shared_ptr<CommandResult> ShowMergePSDDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        compat::Slice<TFD_Filter> filters;
        filters.resize(1);
        const compat::Slice<const char*> patterns0{"*.psd"};
        filters[0] = {{patterns0.data(), patterns0.size()}, "Photoshop Document (*.psd)"};

        compat::RetainedString file = incShowOpenDialog(
            {filters.data(), filters.size()}, tr("Import...")).retainedString();
        if (!file.isNull()) {
            incPopWelcomeWindow();
            ngPushWindow(ngCreateWindow<PSDMergeWindow>(file));
            return CommandResult::opCall(true);
        }
        return CommandResult::opCall(false, "Merge canceled");
    }

std::shared_ptr<CommandResult> MergePSDCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (path.empty()) return ngMissingPathResult();
        if (!incActivePuppet()) return CommandResult::opCall(false, "No active puppet");
        ngMergePSDDefault(path, renameMapped, retranslateMapped);
        return CommandResult::opCall(true);
    }

std::shared_ptr<CommandResult> ShowMergeKRADialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        compat::Slice<TFD_Filter> filters;
        filters.resize(1);
        const compat::Slice<const char*> patterns0{"*.kra"};
        filters[0] = {{patterns0.data(), patterns0.size()}, "Krita Document (*.kra)"};

        compat::RetainedString file = incShowOpenDialog(
            {filters.data(), filters.size()}, tr("Import...")).retainedString();
        if (!file.isNull()) {
            incPopWelcomeWindow();
            ngPushWindow(ngCreateWindow<KRAMergeWindow>(file));
            return CommandResult::opCall(true);
        }
        return CommandResult::opCall(false, "Merge canceled");
    }

std::shared_ptr<CommandResult> MergeKRACommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (path.empty()) return ngMissingPathResult();
        if (!incActivePuppet()) return CommandResult::opCall(false, "No active puppet");
        ngMergeKRADefault(path, renameMapped, retranslateMapped);
        return CommandResult::opCall(true);
    }

std::shared_ptr<CommandResult> ShowMergeImageFileDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        compat::Slice<TFD_Filter> filters;
        filters.resize(3);
        const compat::Slice<const char*> patterns0{"*.png"};
        filters[0] = {{patterns0.data(), patterns0.size()}, "Portable Network Graphics (*.png)"};
        const compat::Slice<const char*> patterns1{"*.jpeg", "*.jpg"};
        filters[1] = {{patterns1.data(), patterns1.size()}, "JPEG Image (*.jpeg)"};
        const compat::Slice<const char*> patterns2{"*.tga"};
        filters[2] = {{patterns2.data(), patterns2.size()}, "TARGA Graphics (*.tga)"};

        compat::RetainedString path = incShowImportDialog(
            {filters.data(), filters.size()}, tr("Import..."), true).retainedString();
        if (!path.isNull()) {
            try {
                incCreatePartsFromFiles(splitPaths(path));
                return CommandResult::opCall(true);
            } catch (const compat::ThrowableException& ex) {
                showError(ex.msg);
                return CommandResult::opCall(false, ex.msg);
            }
        }
        return CommandResult::opCall(false, "Merge canceled");
    }

std::shared_ptr<CommandResult> MergeImageFilesCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (paths.empty()) return ngMissingPathResult(compat::RetainedString::fromStaticLiteral("Paths"));
        try {
            incCreatePartsFromFiles(splitPaths(paths));
            return CommandResult::opCall(true);
        } catch (const compat::ThrowableException& ex) {
            return CommandResult::opCall(false, ex.msg);
        }
    }

std::shared_ptr<CommandResult> ShowMergeINPDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        incPopWelcomeWindow();
        
        
        

        
        
        
        
        return CommandResult::opCall(true);
    }

std::shared_ptr<CommandResult> MergeINPCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (path.empty()) return ngMissingPathResult();
        auto source = io::project_reader::ngLoadProjectPuppet<ext::ExPuppet>(path);
        if (!source) return CommandResult::opCall(false, "Failed to load INP");
        ngMergeImportedPuppet(source, mergeParameters, mergeAnimations);
        return CommandResult::opCall(true);
    }

std::shared_ptr<CommandResult> ShowExportToINPDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        compat::Slice<TFD_Filter> filters;
        filters.resize(1);
        const compat::Slice<const char*> patterns0{"*.inp"};
        filters[0] = {{patterns0.data(), patterns0.size()}, "nijilive Puppet (*.inp)"};

        compat::RetainedString file = incShowSaveDialog(
            {filters.data(), filters.size()}, "", tr("Export...")).retainedString();
        if (!file.isNull()) { incExportINP(file); return CommandResult::opCall(true); }
        return CommandResult::opCall(false, "Export canceled");
    }

std::shared_ptr<CommandResult> ExportINPCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (file.empty()) return ngMissingPathResult(compat::RetainedString::fromStaticLiteral("File"));
        if (!incActivePuppet()) return CommandResult::opCall(false, "No active puppet");

        IncINPExportSettings settings;
        settings.atlasResolution = atlasResolution;
        settings.nonLinearScaling = nonLinearScaling;
        settings.scale = scale;
        settings.padding = padding;
        settings.optimizePruneUnused = optimizePruneUnused;

        incINPExport(incActivePuppet(), settings, withExtension(file,"inp"));
        return CommandResult::opCall(true);
    }

std::shared_ptr<CommandResult> ShowExportToPNGDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        compat::Slice<TFD_Filter> filters;
        filters.resize(1);
        const compat::Slice<const char*> patterns0{"*.png"};
        filters[0] = {{patterns0.data(), patterns0.size()}, "Portable Network Graphics (*.png)"};

        compat::RetainedString file = incShowSaveDialog(
            {filters.data(), filters.size()}, "", tr("Export...")).retainedString();
        if (!file.isNull()) { ngPushWindow(ngCreateWindow<ImageExportWindow>(withExtension(file,"png"))); return CommandResult::opCall(true); }
        return CommandResult::opCall(false, "Export canceled");
    }

std::shared_ptr<CommandResult> ExportPNGCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (file.empty()) return ngMissingPathResult(compat::RetainedString::fromStaticLiteral("File"));
        compat::RetainedString error;
        auto camera = ngResolveExportCamera(cameraName, error);
        if (!camera) return CommandResult::opCall(false, error);

        nicxlive::core::common::SharedArray<std::uint8_t> data;
        int width, height;
        ngRenderCamera(camera, transparency, postprocessing, data, width, height);
        incExportImage(withExtension(file,"png"), compat::Slice<std::uint8_t>::retainedView(
            data.retentionOwner(), data.data(), data.size()), width, height);
        return CommandResult::opCall(true);
    }

std::shared_ptr<CommandResult> ShowExportToJpegDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        compat::Slice<TFD_Filter> filters;
        filters.resize(1);
        const compat::Slice<const char*> patterns0{"*.jpeg", "*.jpg"};
        filters[0] = {{patterns0.data(), patterns0.size()}, "JPEG Image (*.jpeg)"};

        compat::RetainedString file = incShowSaveDialog(
            {filters.data(), filters.size()}, "", tr("Export...")).retainedString();
        if (!file.isNull()) { ngPushWindow(ngCreateWindow<ImageExportWindow>(withExtension(file,"jpeg"))); return CommandResult::opCall(true); }
        return CommandResult::opCall(false, "Export canceled");
    }

std::shared_ptr<CommandResult> ExportJPEGCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (file.empty()) return ngMissingPathResult(compat::RetainedString::fromStaticLiteral("File"));
        compat::RetainedString error;
        auto camera = ngResolveExportCamera(cameraName, error);
        if (!camera) return CommandResult::opCall(false, error);

        nicxlive::core::common::SharedArray<std::uint8_t> data;
        int width, height;
        ngRenderCamera(camera, transparency, postprocessing, data, width, height);
        incExportImage(withExtension(file,"jpeg"), compat::Slice<std::uint8_t>::retainedView(
            data.retentionOwner(), data.data(), data.size()), width, height);
        return CommandResult::opCall(true);
    }

std::shared_ptr<CommandResult> ShowExportToTGADialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        compat::Slice<TFD_Filter> filters;
        filters.resize(1);
        const compat::Slice<const char*> patterns0{"*.tga"};
        filters[0] = {{patterns0.data(), patterns0.size()}, "TARGA Graphics (*.tga)"};

        compat::RetainedString file = incShowSaveDialog(
            {filters.data(), filters.size()}, "", tr("Export...")).retainedString();
        if (!file.isNull()) { ngPushWindow(ngCreateWindow<ImageExportWindow>(withExtension(file,"tga"))); return CommandResult::opCall(true); }
        return CommandResult::opCall(false, "Export canceled");
    }

std::shared_ptr<CommandResult> ExportTGACommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        if (file.empty()) return ngMissingPathResult(compat::RetainedString::fromStaticLiteral("File"));
        compat::RetainedString error;
        auto camera = ngResolveExportCamera(cameraName, error);
        if (!camera) return CommandResult::opCall(false, error);

        nicxlive::core::common::SharedArray<std::uint8_t> data;
        int width, height;
        ngRenderCamera(camera, transparency, postprocessing, data, width, height);
        incExportImage(withExtension(file,"tga"), compat::Slice<std::uint8_t>::retainedView(
            data.retentionOwner(), data.data(), data.size()), width, height);
        return CommandResult::opCall(true);
    }

std::shared_ptr<CommandResult> ShowExportToVideoDialogCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        compat::Slice<TFD_Filter> filters;
        filters.resize(4);
        const compat::Slice<const char*> patterns0{"*.mp4"};
        filters[0] = {{patterns0.data(), patterns0.size()}, "H.264 Video (*.mp4)"};
        const compat::Slice<const char*> patterns1{"*.avi"};
        filters[1] = {{patterns1.data(), patterns1.size()}, "AVI Video (*.avi)"};
        const compat::Slice<const char*> patterns2{"*.webm"};
        filters[2] = {{patterns2.data(), patterns2.size()}, "WebM Video (*.webm)"};
        const compat::Slice<const char*> patterns3{"*.png"};
        filters[3] = {{patterns3.data(), patterns3.size()}, "PNG Sequence (*.png)"};

        compat::RetainedString file = incShowSaveDialog(
            {filters.data(), filters.size()}, "", tr("Export...")).retainedString();
        if (!file.isNull()) {

            
            if (extension(file).empty()) file = withExtension(file,"mp4");
            ngPushWindow(ngCreateWindow<VideoExportWindow>(file));
            return CommandResult::opCall(true);
        }
        return CommandResult::opCall(false, "Export canceled");
    }

std::shared_ptr<CommandResult> CloseProjectCommand::run(std::shared_ptr<Context> ctx) {
    [[maybe_unused]] const auto receiverLifetime = weak_from_this().lock();
        incCloseProjectAsk();
        return CommandResult::opCall(true);
    }

}
