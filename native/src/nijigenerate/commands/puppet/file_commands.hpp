#pragma once
#include <nijigenerate/commands/register_command.hpp>
#include <nijigenerate/commands/detail/initializer_body.hpp>
#include "file.hpp"
#include <nijigenerate/commands/command.hpp>
#include <nijigenerate/commands/context.hpp>
#include <nijigenerate/commands/covariant_command.hpp>
#include <nijigenerate/commands/detail/declared_constructor_arguments.hpp>
#include <compat/source_allocator.hpp>
#include <concepts>
#include <cstddef>
#include <memory>
#include <new>
#include <tuple>
#include <type_traits>
#include <utility>
namespace nijigenerate::commands::puppet::file {
namespace detail {
struct FileCommandText {
    compat::RetainedString label;
    compat::RetainedString description;
};
}
struct OpenFileCommandArguments {
    compat::RetainedString file{};
};
struct SaveFileCommandArguments {
    compat::RetainedString file{};
};
struct OpenPSDDepthMapDialogCommandArguments {
    compat::RetainedString path{};
};
struct ImportPSDCommandArguments {
    compat::RetainedString path{};
    bool keepStructure{};
    compat::RetainedString layerGroupNodeType{};
};
struct ImportKRACommandArguments {
    compat::RetainedString path{};
    bool keepStructure{};
    compat::RetainedString layerGroupNodeType{};
};
struct ImportINPCommandArguments {
    compat::RetainedString path{};
};
struct ImportImageFolderCommandArguments {
    compat::RetainedString path{};
};
struct MergePSDCommandArguments {
    compat::RetainedString path{};
    bool renameMapped{};
    bool retranslateMapped{};
};
struct MergeKRACommandArguments {
    compat::RetainedString path{};
    bool renameMapped{};
    bool retranslateMapped{};
};
struct MergeImageFilesCommandArguments {
    compat::RetainedString paths{};
};
struct MergeINPCommandArguments {
    compat::RetainedString path{};
    bool mergeParameters{};
    bool mergeAnimations{};
};
struct ExportINPCommandArguments {
    compat::RetainedString file{};
    std::size_t atlasResolution{};
    bool nonLinearScaling{};
    float scale = std::numeric_limits<float>::quiet_NaN();
    std::int32_t padding{};
    bool optimizePruneUnused{};
};
struct ExportPNGCommandArguments {
    compat::RetainedString file{};
    compat::RetainedString cameraName{};
    bool transparency{};
    bool postprocessing{};
};
struct ExportJPEGCommandArguments {
    compat::RetainedString file{};
    compat::RetainedString cameraName{};
    bool transparency{};
    bool postprocessing{};
};
struct ExportTGACommandArguments {
    compat::RetainedString file{};
    compat::RetainedString cameraName{};
    bool transparency{};
    bool postprocessing{};
};
struct ExportVideoCommandArguments {
    compat::RetainedString file{};
    compat::RetainedString animationName{};
    compat::RetainedString cameraName{};
    compat::RetainedString codec{};
    std::int32_t loops{};
    std::int32_t framerate{};
    bool transparency{};
    bool postprocessing{};
};
}
namespace nijigenerate::commands {
template<> struct CommandFields<puppet::file::OpenFileCommandArguments> {
    using A=puppet::file::OpenFileCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::file,"file","specifies file path.","string">{}
    };
};
template<> struct CommandFields<puppet::file::SaveFileCommandArguments> {
    using A=puppet::file::SaveFileCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::file,"file","specifies file path.","string">{}
    };
};
template<> struct CommandFields<puppet::file::OpenPSDDepthMapDialogCommandArguments> {
    using A=puppet::file::OpenPSDDepthMapDialogCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::path,"path","Path to the PSD or PNG depth source to open in the dialog","string">{}
    };
};
template<> struct CommandFields<puppet::file::ImportPSDCommandArguments> {
    using A=puppet::file::ImportPSDCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::path,"path","path to PSD file.","string">{},
        FieldDescriptor<&A::keepStructure,"keepStructure","preserve layer groups as nodes.","bool">{},
        FieldDescriptor<&A::layerGroupNodeType,"layerGroupNodeType","replacement node type when preserving group structure.","string">{}
    };
};
template<> struct CommandFields<puppet::file::ImportKRACommandArguments> {
    using A=puppet::file::ImportKRACommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::path,"path","path to KRA file.","string">{},
        FieldDescriptor<&A::keepStructure,"keepStructure","preserve layer groups as nodes.","bool">{},
        FieldDescriptor<&A::layerGroupNodeType,"layerGroupNodeType","replacement node type when preserving group structure.","string">{}
    };
};
template<> struct CommandFields<puppet::file::ImportINPCommandArguments> {
    using A=puppet::file::ImportINPCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::path,"path","path to INP file.","string">{}
    };
};
template<> struct CommandFields<puppet::file::ImportImageFolderCommandArguments> {
    using A=puppet::file::ImportImageFolderCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::path,"path","path to folder containing image files.","string">{}
    };
};
template<> struct CommandFields<puppet::file::MergePSDCommandArguments> {
    using A=puppet::file::MergePSDCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::path,"path","path to PSD file.","string">{},
        FieldDescriptor<&A::renameMapped,"renameMapped","rename mapped parts to imported layer names.","bool">{},
        FieldDescriptor<&A::retranslateMapped,"retranslateMapped","move mapped parts to imported layer positions.","bool">{}
    };
};
template<> struct CommandFields<puppet::file::MergeKRACommandArguments> {
    using A=puppet::file::MergeKRACommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::path,"path","path to KRA file.","string">{},
        FieldDescriptor<&A::renameMapped,"renameMapped","rename mapped parts to imported layer names.","bool">{},
        FieldDescriptor<&A::retranslateMapped,"retranslateMapped","move mapped parts to imported layer positions.","bool">{}
    };
};
template<> struct CommandFields<puppet::file::MergeImageFilesCommandArguments> {
    using A=puppet::file::MergeImageFilesCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::paths,"paths","paths to image files, separated by |.","string">{}
    };
};
template<> struct CommandFields<puppet::file::MergeINPCommandArguments> {
    using A=puppet::file::MergeINPCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::path,"path","path to INP file.","string">{},
        FieldDescriptor<&A::mergeParameters,"mergeParameters","merge imported parameters into the active puppet.","bool">{},
        FieldDescriptor<&A::mergeAnimations,"mergeAnimations","merge imported animations into the active puppet.","bool">{}
    };
};
template<> struct CommandFields<puppet::file::ExportINPCommandArguments> {
    using A=puppet::file::ExportINPCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::file,"file","output path for INP file.","string">{},
        FieldDescriptor<&A::atlasResolution,"atlasResolution","atlas resolution.","ulong">{},
        FieldDescriptor<&A::nonLinearScaling,"nonLinearScaling","whether oversized parts may scale individually.","bool">{},
        FieldDescriptor<&A::scale,"scale","output texture scale.","float">{},
        FieldDescriptor<&A::padding,"padding","atlas padding.","int">{},
        FieldDescriptor<&A::optimizePruneUnused,"optimizePruneUnused","whether disabled branches are pruned.","bool">{}
    };
};
template<> struct CommandFields<puppet::file::ExportPNGCommandArguments> {
    using A=puppet::file::ExportPNGCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::file,"file","output path for PNG file.","string">{},
        FieldDescriptor<&A::cameraName,"cameraName","camera name. Empty selects the first camera.","string">{},
        FieldDescriptor<&A::transparency,"transparency","whether to render with transparent background.","bool">{},
        FieldDescriptor<&A::postprocessing,"postprocessing","whether to apply post-processing.","bool">{}
    };
};
template<> struct CommandFields<puppet::file::ExportJPEGCommandArguments> {
    using A=puppet::file::ExportJPEGCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::file,"file","output path for JPEG file.","string">{},
        FieldDescriptor<&A::cameraName,"cameraName","camera name. Empty selects the first camera.","string">{},
        FieldDescriptor<&A::transparency,"transparency","whether to render with transparent background.","bool">{},
        FieldDescriptor<&A::postprocessing,"postprocessing","whether to apply post-processing.","bool">{}
    };
};
template<> struct CommandFields<puppet::file::ExportTGACommandArguments> {
    using A=puppet::file::ExportTGACommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::file,"file","output path for TGA file.","string">{},
        FieldDescriptor<&A::cameraName,"cameraName","camera name. Empty selects the first camera.","string">{},
        FieldDescriptor<&A::transparency,"transparency","whether to render with transparent background.","bool">{},
        FieldDescriptor<&A::postprocessing,"postprocessing","whether to apply post-processing.","bool">{}
    };
};
template<> struct CommandFields<puppet::file::ExportVideoCommandArguments> {
    using A=puppet::file::ExportVideoCommandArguments;
    static constexpr auto descriptors=std::tuple{
        FieldDescriptor<&A::file,"file","output path for the video or image sequence.","string">{},
        FieldDescriptor<&A::animationName,"animationName","animation name. Empty selects the first animation.","string">{},
        FieldDescriptor<&A::cameraName,"cameraName","camera name. Empty selects the first camera.","string">{},
        FieldDescriptor<&A::codec,"codec","ffmpeg codec tag. Empty selects auto.","string">{},
        FieldDescriptor<&A::loops,"loops","number of animation loops to export.","int">{},
        FieldDescriptor<&A::framerate,"framerate","output framerate. Negative selects animation framerate.","int">{},
        FieldDescriptor<&A::transparency,"transparency","whether to render with transparent background.","bool">{},
        FieldDescriptor<&A::postprocessing,"postprocessing","whether to apply post-processing.","bool">{}
    };
};
}
namespace nijigenerate::commands::puppet::file {
class NewFileCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<NewFileCommand> {
    static detail::FileCommandText sourceConstructorText();
    NewFileCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<NewFileCommand>;
public:
    NewFileCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ShowOpenFileDialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowOpenFileDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowOpenFileDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowOpenFileDialogCommand>;
public:
    ShowOpenFileDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class OpenFileCommand : public ::nijigenerate::commands::detail::CovariantCommand<OpenFileCommand, OpenFileCommandArguments> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<OpenFileCommand, OpenFileCommandArguments>;
    static detail::FileCommandText sourceConstructorText();
    OpenFileCommand(detail::FileCommandText text, compat::RetainedString file);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<OpenFileCommand>;
public:
    using Result = LoadResult<std::shared_ptr<nicxlive::core::Puppet>>;
    OpenFileCommand(compat::RetainedString file);
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& ctx);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> ctx);
};
class ShowSaveFileDialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowSaveFileDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowSaveFileDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowSaveFileDialogCommand>;
public:
    ShowSaveFileDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class SaveFileCommand : public ExCommand<SaveFileCommandArguments>, public std::enable_shared_from_this<SaveFileCommand> {
    static detail::FileCommandText sourceConstructorText();
    SaveFileCommand(detail::FileCommandText text, compat::RetainedString file);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<SaveFileCommand>;
public:
    SaveFileCommand(compat::RetainedString file);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ShowSaveFileAsDialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowSaveFileAsDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowSaveFileAsDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowSaveFileAsDialogCommand>;
public:
    ShowSaveFileAsDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ShowImportPSDDialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowImportPSDDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowImportPSDDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowImportPSDDialogCommand>;
public:
    ShowImportPSDDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ShowImportPSDDepthMapDialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowImportPSDDepthMapDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowImportPSDDepthMapDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowImportPSDDepthMapDialogCommand>;
public:
    ShowImportPSDDepthMapDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class OpenPSDDepthMapDialogCommand : public ExCommand<OpenPSDDepthMapDialogCommandArguments>, public std::enable_shared_from_this<OpenPSDDepthMapDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    OpenPSDDepthMapDialogCommand(detail::FileCommandText text, compat::RetainedString path);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<OpenPSDDepthMapDialogCommand>;
public:
    OpenPSDDepthMapDialogCommand(compat::RetainedString path);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ImportPSDCommand : public ::nijigenerate::commands::detail::CovariantCommand<ImportPSDCommand, ImportPSDCommandArguments> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<ImportPSDCommand, ImportPSDCommandArguments>;
    static detail::FileCommandText sourceConstructorText();
    ImportPSDCommand(detail::FileCommandText text, compat::RetainedString path, bool keepStructure, compat::RetainedString layerGroupNodeType);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ImportPSDCommand>;
public:
    using Result = LoadResult<std::shared_ptr<nicxlive::core::Puppet>>;
    ImportPSDCommand(compat::RetainedString path, bool keepStructure = true, compat::RetainedString layerGroupNodeType = compat::RetainedString::fromStaticLiteral("DynamicComposite"));
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& ctx);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> ctx);
};
class ShowImportKRADialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowImportKRADialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowImportKRADialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowImportKRADialogCommand>;
public:
    ShowImportKRADialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ImportKRACommand : public ::nijigenerate::commands::detail::CovariantCommand<ImportKRACommand, ImportKRACommandArguments> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<ImportKRACommand, ImportKRACommandArguments>;
    static detail::FileCommandText sourceConstructorText();
    ImportKRACommand(detail::FileCommandText text, compat::RetainedString path, bool keepStructure, compat::RetainedString layerGroupNodeType);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ImportKRACommand>;
public:
    using Result = LoadResult<std::shared_ptr<nicxlive::core::Puppet>>;
    ImportKRACommand(compat::RetainedString path, bool keepStructure = true, compat::RetainedString layerGroupNodeType = compat::RetainedString::fromStaticLiteral("DynamicComposite"));
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& ctx);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> ctx);
};
class ShowImportINPDialogCommand : public ::nijigenerate::commands::detail::CovariantCommand<ShowImportINPDialogCommand> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<ShowImportINPDialogCommand>;
    static detail::FileCommandText sourceConstructorText();
    ShowImportINPDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowImportINPDialogCommand>;
public:
    using Result = LoadResult<std::shared_ptr<nicxlive::core::Puppet>>;
    ShowImportINPDialogCommand();
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& ctx);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> ctx);
};
class ImportINPCommand : public ::nijigenerate::commands::detail::CovariantCommand<ImportINPCommand, ImportINPCommandArguments> {
    using NativeBase = ::nijigenerate::commands::detail::CovariantCommand<ImportINPCommand, ImportINPCommandArguments>;
    static detail::FileCommandText sourceConstructorText();
    ImportINPCommand(detail::FileCommandText text, compat::RetainedString path);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ImportINPCommand>;
public:
    using Result = LoadResult<std::shared_ptr<nicxlive::core::Puppet>>;
    ImportINPCommand(compat::RetainedString path);
    virtual std::shared_ptr<Result> run(const std::shared_ptr<Context>& ctx);
    virtual std::shared_ptr<Result> runTyped(std::shared_ptr<Context> ctx);
};
class ShowImportImageFolderDialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowImportImageFolderDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowImportImageFolderDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowImportImageFolderDialogCommand>;
public:
    ShowImportImageFolderDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ImportImageFolderCommand : public ExCommand<ImportImageFolderCommandArguments>, public std::enable_shared_from_this<ImportImageFolderCommand> {
    static detail::FileCommandText sourceConstructorText();
    ImportImageFolderCommand(detail::FileCommandText text, compat::RetainedString path);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ImportImageFolderCommand>;
public:
    ImportImageFolderCommand(compat::RetainedString path);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ShowMergePSDDialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowMergePSDDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowMergePSDDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowMergePSDDialogCommand>;
public:
    ShowMergePSDDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class MergePSDCommand : public ExCommand<MergePSDCommandArguments>, public std::enable_shared_from_this<MergePSDCommand> {
    static detail::FileCommandText sourceConstructorText();
    MergePSDCommand(detail::FileCommandText text, compat::RetainedString path, bool renameMapped, bool retranslateMapped);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<MergePSDCommand>;
public:
    MergePSDCommand(compat::RetainedString path, bool renameMapped = false, bool retranslateMapped = false);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ShowMergeKRADialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowMergeKRADialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowMergeKRADialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowMergeKRADialogCommand>;
public:
    ShowMergeKRADialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class MergeKRACommand : public ExCommand<MergeKRACommandArguments>, public std::enable_shared_from_this<MergeKRACommand> {
    static detail::FileCommandText sourceConstructorText();
    MergeKRACommand(detail::FileCommandText text, compat::RetainedString path, bool renameMapped, bool retranslateMapped);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<MergeKRACommand>;
public:
    MergeKRACommand(compat::RetainedString path, bool renameMapped = false, bool retranslateMapped = false);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ShowMergeImageFileDialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowMergeImageFileDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowMergeImageFileDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowMergeImageFileDialogCommand>;
public:
    ShowMergeImageFileDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class MergeImageFilesCommand : public ExCommand<MergeImageFilesCommandArguments>, public std::enable_shared_from_this<MergeImageFilesCommand> {
    static detail::FileCommandText sourceConstructorText();
    MergeImageFilesCommand(detail::FileCommandText text, compat::RetainedString paths);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<MergeImageFilesCommand>;
public:
    MergeImageFilesCommand(compat::RetainedString paths);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ShowMergeINPDialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowMergeINPDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowMergeINPDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowMergeINPDialogCommand>;
public:
    ShowMergeINPDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class MergeINPCommand : public ExCommand<MergeINPCommandArguments>, public std::enable_shared_from_this<MergeINPCommand> {
    static detail::FileCommandText sourceConstructorText();
    MergeINPCommand(detail::FileCommandText text, compat::RetainedString path, bool mergeParameters, bool mergeAnimations);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<MergeINPCommand>;
public:
    MergeINPCommand(compat::RetainedString path, bool mergeParameters = true, bool mergeAnimations = true);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ShowExportToINPDialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowExportToINPDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowExportToINPDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowExportToINPDialogCommand>;
public:
    ShowExportToINPDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ExportINPCommand : public ExCommand<ExportINPCommandArguments>, public std::enable_shared_from_this<ExportINPCommand> {
    static detail::FileCommandText sourceConstructorText();
    ExportINPCommand(detail::FileCommandText text, compat::RetainedString file, std::size_t atlasResolution, bool nonLinearScaling, float scale, std::int32_t padding, bool optimizePruneUnused);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ExportINPCommand>;
public:
    ExportINPCommand(compat::RetainedString file, std::size_t atlasResolution = 2048, bool nonLinearScaling = false, float scale = 1, std::int32_t padding = 16, bool optimizePruneUnused = true);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ShowExportToPNGDialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowExportToPNGDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowExportToPNGDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowExportToPNGDialogCommand>;
public:
    ShowExportToPNGDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ExportPNGCommand : public ExCommand<ExportPNGCommandArguments>, public std::enable_shared_from_this<ExportPNGCommand> {
    static detail::FileCommandText sourceConstructorText();
    ExportPNGCommand(detail::FileCommandText text, compat::RetainedString file, compat::RetainedString cameraName, bool transparency, bool postprocessing);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ExportPNGCommand>;
public:
    ExportPNGCommand(compat::RetainedString file, compat::RetainedString cameraName = compat::RetainedString::emptyLiteral(), bool transparency = false, bool postprocessing = false);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ShowExportToJpegDialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowExportToJpegDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowExportToJpegDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowExportToJpegDialogCommand>;
public:
    ShowExportToJpegDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ExportJPEGCommand : public ExCommand<ExportJPEGCommandArguments>, public std::enable_shared_from_this<ExportJPEGCommand> {
    static detail::FileCommandText sourceConstructorText();
    ExportJPEGCommand(detail::FileCommandText text, compat::RetainedString file, compat::RetainedString cameraName, bool transparency, bool postprocessing);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ExportJPEGCommand>;
public:
    ExportJPEGCommand(compat::RetainedString file, compat::RetainedString cameraName = compat::RetainedString::emptyLiteral(), bool transparency = false, bool postprocessing = false);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ShowExportToTGADialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowExportToTGADialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowExportToTGADialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowExportToTGADialogCommand>;
public:
    ShowExportToTGADialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ExportTGACommand : public ExCommand<ExportTGACommandArguments>, public std::enable_shared_from_this<ExportTGACommand> {
    static detail::FileCommandText sourceConstructorText();
    ExportTGACommand(detail::FileCommandText text, compat::RetainedString file, compat::RetainedString cameraName, bool transparency, bool postprocessing);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ExportTGACommand>;
public:
    ExportTGACommand(compat::RetainedString file, compat::RetainedString cameraName = compat::RetainedString::emptyLiteral(), bool transparency = false, bool postprocessing = false);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ShowExportToVideoDialogCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<ShowExportToVideoDialogCommand> {
    static detail::FileCommandText sourceConstructorText();
    ShowExportToVideoDialogCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ShowExportToVideoDialogCommand>;
public:
    ShowExportToVideoDialogCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class ExportVideoCommand : public ExCommand<ExportVideoCommandArguments>, public std::enable_shared_from_this<ExportVideoCommand> {
    static detail::FileCommandText sourceConstructorText();
    ExportVideoCommand(detail::FileCommandText text, compat::RetainedString file, compat::RetainedString animationName, compat::RetainedString cameraName, compat::RetainedString codec, std::int32_t loops, std::int32_t framerate, bool transparency, bool postprocessing);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<ExportVideoCommand>;
public:
    ExportVideoCommand(compat::RetainedString file, compat::RetainedString animationName = compat::RetainedString::emptyLiteral(), compat::RetainedString cameraName = compat::RetainedString::emptyLiteral(), compat::RetainedString codec = compat::RetainedString::fromStaticLiteral("auto"), std::int32_t loops = 1, std::int32_t framerate = -1, bool transparency = false, bool postprocessing = false);
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
class CloseProjectCommand : public ExCommand<CommandNoArguments>, public std::enable_shared_from_this<CloseProjectCommand> {
    static detail::FileCommandText sourceConstructorText();
    CloseProjectCommand(detail::FileCommandText text);
    friend struct ::nijigenerate::commands::SourceCommandConstructor<CloseProjectCommand>;
public:
    CloseProjectCommand();
    std::shared_ptr<CommandResult> run(std::shared_ptr<Context> ctx) override;
};
void ngInitFileCommands();
}

namespace nicxlive::core::d_runtime {
#define NG_FILE_COMMAND_SOURCE_NAMES(Type) \
    template<> struct SourceObjectTypeNames<::nijigenerate::commands::puppet::file::Type> { \
        inline static constexpr char className[] = "nijigenerate.commands.puppet.file." #Type; \
        inline static constexpr char templateArgumentName[] = "(nijigenerate.commands.puppet.file." #Type ")"; \
    }; \
    template<> struct SourceTypeSpellings<::nijigenerate::commands::puppet::file::Type> { \
        inline static constexpr bool isNativeObject = true; \
        inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.puppet.file." #Type ")"; \
        inline static constexpr char unqualifiedArgument[] = "(" #Type ")"; \
        inline static constexpr char qualifiedElement[] = "nijigenerate.commands.puppet.file." #Type; \
        inline static constexpr char unqualifiedElement[] = #Type; \
    };
NG_FILE_COMMAND_SOURCE_NAMES(NewFileCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowOpenFileDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(OpenFileCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowSaveFileDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(SaveFileCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowSaveFileAsDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowImportPSDDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowImportPSDDepthMapDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(OpenPSDDepthMapDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ImportPSDCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowImportKRADialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ImportKRACommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowImportINPDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ImportINPCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowImportImageFolderDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ImportImageFolderCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowMergePSDDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(MergePSDCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowMergeKRADialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(MergeKRACommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowMergeImageFileDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(MergeImageFilesCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowMergeINPDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(MergeINPCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowExportToINPDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ExportINPCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowExportToPNGDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ExportPNGCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowExportToJpegDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ExportJPEGCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowExportToTGADialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ExportTGACommand)
NG_FILE_COMMAND_SOURCE_NAMES(ShowExportToVideoDialogCommand)
NG_FILE_COMMAND_SOURCE_NAMES(ExportVideoCommand)
NG_FILE_COMMAND_SOURCE_NAMES(CloseProjectCommand)
#undef NG_FILE_COMMAND_SOURCE_NAMES
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands {
template<> struct CommandTraits<puppet::file::NewFileCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiConfirm,EffectProjectReset};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowOpenFileDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialog};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::OpenFileCommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectImport};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::OpenFileCommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowSaveFileDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialog};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::SaveFileCommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectFileWrite};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::SaveFileCommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowSaveFileAsDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialog};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowImportPSDDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialog};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowImportPSDDepthMapDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialog};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::OpenPSDDepthMapDialogCommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,GuiDialog};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::OpenPSDDepthMapDialogCommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ImportPSDCommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectImport};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::ImportPSDCommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowImportKRADialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialog};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ImportKRACommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectImport};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::ImportKRACommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowImportINPDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialog,EffectImport};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ImportINPCommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectImport};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::ImportINPCommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowImportImageFolderDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialog,EffectImport};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ImportImageFolderCommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectImport};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::ImportImageFolderCommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowMergePSDDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogWindow};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::MergePSDCommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectStructuralEdit};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::MergePSDCommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowMergeKRADialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogWindow};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::MergeKRACommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectStructuralEdit};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::MergeKRACommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowMergeImageFileDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialog};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::MergeImageFilesCommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectStructuralEdit};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::MergeImageFilesCommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowMergeINPDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::MergeINPCommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectStructuralEdit};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::MergeINPCommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowExportToINPDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialog};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ExportINPCommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectFileWrite};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::ExportINPCommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowExportToPNGDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogWindow};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ExportPNGCommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectFileWrite};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::ExportPNGCommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowExportToJpegDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogWindow};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ExportJPEGCommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectFileWrite};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::ExportJPEGCommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowExportToTGADialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogWindow};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ExportTGACommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectFileWrite};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::ExportTGACommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ShowExportToVideoDialogCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiDialogWindow};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::ExportVideoCommand> {
    static constexpr auto attributes=std::tuple{ShortcutHidden,EffectFileWrite};
    using BaseClasses=CommandTypeList<ExCommand<puppet::file::ExportVideoCommandArguments>,Command>;
};
template<> struct CommandTraits<puppet::file::CloseProjectCommand> {
    static constexpr auto attributes=std::tuple{McpHidden,GuiConfirm,EffectProjectReset};
    using BaseClasses=CommandTypeList<ExCommand<CommandNoArguments>,Command>;
};
}

namespace nijigenerate::commands::puppet::file {
namespace constructor_detail {

template<class Storage, std::size_t Minimum> struct Operations {
    using Descriptors = std::remove_cv_t<decltype(CommandFields<Storage>::descriptors)>;
    template<std::size_t I> using Parameter = typename std::tuple_element_t<I, Descriptors>::Type;
    static constexpr std::size_t count = std::tuple_size_v<Descriptors>;

    template<std::size_t... I>
    static auto parameterTypes(std::index_sequence<I...>) -> std::tuple<Parameter<I>...>;
    template<class... Args> using Targets = decltype(parameterTypes(std::index_sequence_for<Args...>{}));

    template<class... Args> static consteval bool supported() {
        if constexpr (sizeof...(Args) < Minimum || sizeof...(Args) > count) return false;
        else {
            constexpr bool parameters = commands::detail::declaredSourceParametersSupported<Targets<Args...>,
                Args...>();
            if constexpr (!parameters) return false;
            else if constexpr (count == 0)
                return SourceExCommandConstructorProgram<Storage>::pairSupported();
            else return []<std::size_t... I>(std::index_sequence<I...>) {
                return SourceExCommandConstructorProgram<Storage>::template preferredSupported<Parameter<I>&...>();
            }(std::make_index_sequence<count>{});
        }
    }

    template<class C, class... Args>
    static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, Args&&... args) {
        return commands::detail::constructDeclaredSourceCommand<C, Targets<Args...>>(
            allocator, std::forward<Args>(args)...);
    }
};
} // namespace constructor_detail
} // namespace nijigenerate::commands::puppet::file

namespace nijigenerate::commands {

// Defining records for the actual classes, not a fallback for arbitrary C.
#define NG_FILE_CONSTRUCTOR_PROGRAM(Type, Storage, Minimum) \
    template<> struct SourceCommandConstructor<puppet::file::Type> { \
        template<class... Args> struct Program { \
            using C = puppet::file::Type; \
            using Operations = puppet::file::constructor_detail::Operations<Storage, Minimum>; \
            static consteval bool supported() { \
                return Operations::template supported<Args...>() && requires { \
                    { C::sourceConstructorText() } -> std::same_as<puppet::file::detail::FileCommandText>; \
                }; \
            } \
            static std::shared_ptr<C> construct(nicxlive::core::d_runtime::SourceAllocator<C> allocator, \
                Args&&... args) requires(supported()) { \
                return Operations::template construct<C>(allocator, std::forward<Args>(args)...); \
            } \
        }; \
    };

NG_FILE_CONSTRUCTOR_PROGRAM(NewFileCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowOpenFileDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(OpenFileCommand, puppet::file::OpenFileCommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowSaveFileDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(SaveFileCommand, puppet::file::SaveFileCommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowSaveFileAsDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowImportPSDDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowImportPSDDepthMapDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(OpenPSDDepthMapDialogCommand, puppet::file::OpenPSDDepthMapDialogCommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ImportPSDCommand, puppet::file::ImportPSDCommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowImportKRADialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(ImportKRACommand, puppet::file::ImportKRACommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowImportINPDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(ImportINPCommand, puppet::file::ImportINPCommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowImportImageFolderDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(ImportImageFolderCommand, puppet::file::ImportImageFolderCommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowMergePSDDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(MergePSDCommand, puppet::file::MergePSDCommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowMergeKRADialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(MergeKRACommand, puppet::file::MergeKRACommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowMergeImageFileDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(MergeImageFilesCommand, puppet::file::MergeImageFilesCommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowMergeINPDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(MergeINPCommand, puppet::file::MergeINPCommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowExportToINPDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(ExportINPCommand, puppet::file::ExportINPCommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowExportToPNGDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(ExportPNGCommand, puppet::file::ExportPNGCommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowExportToJpegDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(ExportJPEGCommand, puppet::file::ExportJPEGCommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowExportToTGADialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(ExportTGACommand, puppet::file::ExportTGACommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(ShowExportToVideoDialogCommand, CommandNoArguments, 0)
NG_FILE_CONSTRUCTOR_PROGRAM(ExportVideoCommand, puppet::file::ExportVideoCommandArguments, 1)
NG_FILE_CONSTRUCTOR_PROGRAM(CloseProjectCommand, CommandNoArguments, 0)

#undef NG_FILE_CONSTRUCTOR_PROGRAM
} // namespace nijigenerate::commands

namespace nijigenerate::commands::puppet::file {
namespace initialization {
struct Table {
    using map_type = decltype(commands);
    static map_type& get() { return commands; }
};
struct EmptyFile { static auto get() { return std::tuple{compat::RetainedString::emptyLiteral()}; } };
struct ImportArguments {
    static auto get() {
        return std::tuple{compat::RetainedString::emptyLiteral(), true,
            compat::RetainedString::fromStaticLiteral("DynamicComposite")};
    }
};
struct MergeArguments {
    static auto get() { return std::tuple{compat::RetainedString::emptyLiteral(), false, false}; }
};
struct MergeInpArguments {
    static auto get() { return std::tuple{compat::RetainedString::emptyLiteral(), true, true}; }
};
struct ExportInpArguments {
    static auto get() {
        return std::tuple{compat::RetainedString::emptyLiteral(), std::size_t{2048}, false, 1.0f, 16, true};
    }
};
struct ImageArguments {
    static auto get() {
        return std::tuple{compat::RetainedString::emptyLiteral(), compat::RetainedString::emptyLiteral(), false, false};
    }
};
struct VideoArguments {
    static auto get() {
        return std::tuple{compat::RetainedString::emptyLiteral(), compat::RetainedString::emptyLiteral(),
            compat::RetainedString::emptyLiteral(), compat::RetainedString::fromStaticLiteral("auto"), 1, -1, false, false};
    }
};
template<auto Id, class C>
using Optional = ::nijigenerate::commands::detail::OptionalCommandRegistration<Id, C>;
template<auto Id, class C, class Args>
using Required = ::nijigenerate::commands::detail::ConstructedCommandRegistration<Id, C, Args>;
using Program = ::nijigenerate::commands::detail::CommandRegistrationSequence<
    Optional<FileCommand::NewFile, NewFileCommand>,
    Optional<FileCommand::ShowOpenFileDialog, ShowOpenFileDialogCommand>,
    Optional<FileCommand::OpenFile, OpenFileCommand>,
    Optional<FileCommand::ShowSaveFileDialog, ShowSaveFileDialogCommand>,
    Optional<FileCommand::SaveFile, SaveFileCommand>,
    Optional<FileCommand::ShowSaveFileAsDialog, ShowSaveFileAsDialogCommand>,
    Optional<FileCommand::ShowImportPSDDialog, ShowImportPSDDialogCommand>,
    Optional<FileCommand::ShowImportPSDDepthMapDialog, ShowImportPSDDepthMapDialogCommand>,
    Optional<FileCommand::OpenPSDDepthMapDialog, OpenPSDDepthMapDialogCommand>,
    Optional<FileCommand::ImportPSD, ImportPSDCommand>,
    Optional<FileCommand::ShowImportKRADialog, ShowImportKRADialogCommand>,
    Optional<FileCommand::ImportKRA, ImportKRACommand>,
    Optional<FileCommand::ShowImportINPDialog, ShowImportINPDialogCommand>,
    Optional<FileCommand::ImportINP, ImportINPCommand>,
    Optional<FileCommand::ShowImportImageFolderDialog, ShowImportImageFolderDialogCommand>,
    Optional<FileCommand::ImportImageFolder, ImportImageFolderCommand>,
    Optional<FileCommand::ShowMergePSDDialog, ShowMergePSDDialogCommand>,
    Optional<FileCommand::MergePSD, MergePSDCommand>,
    Optional<FileCommand::ShowMergeKRADialog, ShowMergeKRADialogCommand>,
    Optional<FileCommand::MergeKRA, MergeKRACommand>,
    Optional<FileCommand::ShowMergeImageFileDialog, ShowMergeImageFileDialogCommand>,
    Optional<FileCommand::MergeImageFiles, MergeImageFilesCommand>,
    Optional<FileCommand::ShowMergeINPDialog, ShowMergeINPDialogCommand>,
    Optional<FileCommand::MergeINP, MergeINPCommand>,
    Optional<FileCommand::ShowExportToINPDialog, ShowExportToINPDialogCommand>,
    Optional<FileCommand::ExportINP, ExportINPCommand>,
    Optional<FileCommand::ShowExportToPNGDialog, ShowExportToPNGDialogCommand>,
    Optional<FileCommand::ExportPNG, ExportPNGCommand>,
    Optional<FileCommand::ShowExportToJpegDialog, ShowExportToJpegDialogCommand>,
    Optional<FileCommand::ExportJPEG, ExportJPEGCommand>,
    Optional<FileCommand::ShowExportToTGADialog, ShowExportToTGADialogCommand>,
    Optional<FileCommand::ExportTGA, ExportTGACommand>,
    Optional<FileCommand::ShowExportToVideoDialog, ShowExportToVideoDialogCommand>,
    Optional<FileCommand::ExportVideo, ExportVideoCommand>,
    Optional<FileCommand::CloseProject, CloseProjectCommand>,
    Required<FileCommand::OpenFile, OpenFileCommand, EmptyFile>,
    Required<FileCommand::SaveFile, SaveFileCommand, EmptyFile>,
    Required<FileCommand::OpenPSDDepthMapDialog, OpenPSDDepthMapDialogCommand, EmptyFile>,
    Required<FileCommand::ImportPSD, ImportPSDCommand, ImportArguments>,
    Required<FileCommand::ImportKRA, ImportKRACommand, ImportArguments>,
    Required<FileCommand::ImportINP, ImportINPCommand, EmptyFile>,
    Required<FileCommand::ImportImageFolder, ImportImageFolderCommand, EmptyFile>,
    Required<FileCommand::MergePSD, MergePSDCommand, MergeArguments>,
    Required<FileCommand::MergeKRA, MergeKRACommand, MergeArguments>,
    Required<FileCommand::MergeImageFiles, MergeImageFilesCommand, EmptyFile>,
    Required<FileCommand::MergeINP, MergeINPCommand, MergeInpArguments>,
    Required<FileCommand::ExportINP, ExportINPCommand, ExportInpArguments>,
    Required<FileCommand::ExportPNG, ExportPNGCommand, ImageArguments>,
    Required<FileCommand::ExportJPEG, ExportJPEGCommand, ImageArguments>,
    Required<FileCommand::ExportTGA, ExportTGACommand, ImageArguments>,
    Required<FileCommand::ExportVideo, ExportVideoCommand, VideoArguments>>;
}
template<class T> requires (std::same_as<T, FileCommand> &&
    initialization::Program::supported<initialization::Table>())
void ngInitCommands() { ngInitFileCommands(); }
}

namespace nijigenerate::commands::detail {
template<> struct InitializerBody<puppet::file::FileCommand> {
    static consteval bool supported() {
        return puppet::file::initialization::Program::supported<puppet::file::initialization::Table>();
    }
    template<class ActualKey = puppet::file::FileCommand>
        requires (std::same_as<ActualKey, puppet::file::FileCommand> && supported())
    static void invoke() { puppet::file::ngInitCommands<ActualKey>(); }
};
}
