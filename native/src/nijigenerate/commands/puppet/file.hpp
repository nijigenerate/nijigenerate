#pragma once
#include <cstdint>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <memory>
#include <nijigenerate/commands/command_table.hpp>
#include <nijigenerate/core/selector/tokenizer.hpp>

namespace nijigenerate::commands { class Command; }

namespace nijigenerate::commands::puppet::file {
enum class FileCommand : std::int32_t {
    NewFile = 0,
    ShowOpenFileDialog,
    OpenFile,
    ShowSaveFileDialog,
    SaveFile,
    ShowSaveFileAsDialog,
    ShowImportPSDDialog,
    ShowImportPSDDepthMapDialog,
    OpenPSDDepthMapDialog,
    ImportPSD,
    ShowImportKRADialog,
    ImportKRA,
    ShowImportINPDialog,
    ImportINP,
    ShowImportImageFolderDialog,
    ImportImageFolder,
    ShowMergePSDDialog,
    MergePSD,
    ShowMergeKRADialog,
    MergeKRA,
    ShowMergeImageFileDialog,
    MergeImageFiles,
    ShowMergeINPDialog,
    MergeINP,
    ShowExportToINPDialog,
    ExportINP,
    ShowExportToPNGDialog,
    ExportPNG,
    ShowExportToJpegDialog,
    ExportJPEG,
    ShowExportToTGADialog,
    ExportTGA,
    ShowExportToVideoDialog,
    ExportVideo,
    CloseProject
};

} // namespace nijigenerate::commands::puppet::file

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<commands::puppet::file::FileCommand> {
    using Enum = commands::puppet::file::FileCommand;
    inline static constexpr std::string_view name = "FileCommand";
    inline static constexpr std::array members{
        FormatEnumMember<Enum>{Enum::NewFile, "NewFile"},
        FormatEnumMember<Enum>{Enum::ShowOpenFileDialog, "ShowOpenFileDialog"},
        FormatEnumMember<Enum>{Enum::OpenFile, "OpenFile"},
        FormatEnumMember<Enum>{Enum::ShowSaveFileDialog, "ShowSaveFileDialog"},
        FormatEnumMember<Enum>{Enum::SaveFile, "SaveFile"},
        FormatEnumMember<Enum>{Enum::ShowSaveFileAsDialog, "ShowSaveFileAsDialog"},
        FormatEnumMember<Enum>{Enum::ShowImportPSDDialog, "ShowImportPSDDialog"},
        FormatEnumMember<Enum>{Enum::ShowImportPSDDepthMapDialog, "ShowImportPSDDepthMapDialog"},
        FormatEnumMember<Enum>{Enum::OpenPSDDepthMapDialog, "OpenPSDDepthMapDialog"},
        FormatEnumMember<Enum>{Enum::ImportPSD, "ImportPSD"},
        FormatEnumMember<Enum>{Enum::ShowImportKRADialog, "ShowImportKRADialog"},
        FormatEnumMember<Enum>{Enum::ImportKRA, "ImportKRA"},
        FormatEnumMember<Enum>{Enum::ShowImportINPDialog, "ShowImportINPDialog"},
        FormatEnumMember<Enum>{Enum::ImportINP, "ImportINP"},
        FormatEnumMember<Enum>{Enum::ShowImportImageFolderDialog, "ShowImportImageFolderDialog"},
        FormatEnumMember<Enum>{Enum::ImportImageFolder, "ImportImageFolder"},
        FormatEnumMember<Enum>{Enum::ShowMergePSDDialog, "ShowMergePSDDialog"},
        FormatEnumMember<Enum>{Enum::MergePSD, "MergePSD"},
        FormatEnumMember<Enum>{Enum::ShowMergeKRADialog, "ShowMergeKRADialog"},
        FormatEnumMember<Enum>{Enum::MergeKRA, "MergeKRA"},
        FormatEnumMember<Enum>{Enum::ShowMergeImageFileDialog, "ShowMergeImageFileDialog"},
        FormatEnumMember<Enum>{Enum::MergeImageFiles, "MergeImageFiles"},
        FormatEnumMember<Enum>{Enum::ShowMergeINPDialog, "ShowMergeINPDialog"},
        FormatEnumMember<Enum>{Enum::MergeINP, "MergeINP"},
        FormatEnumMember<Enum>{Enum::ShowExportToINPDialog, "ShowExportToINPDialog"},
        FormatEnumMember<Enum>{Enum::ExportINP, "ExportINP"},
        FormatEnumMember<Enum>{Enum::ShowExportToPNGDialog, "ShowExportToPNGDialog"},
        FormatEnumMember<Enum>{Enum::ExportPNG, "ExportPNG"},
        FormatEnumMember<Enum>{Enum::ShowExportToJpegDialog, "ShowExportToJpegDialog"},
        FormatEnumMember<Enum>{Enum::ExportJPEG, "ExportJPEG"},
        FormatEnumMember<Enum>{Enum::ShowExportToTGADialog, "ShowExportToTGADialog"},
        FormatEnumMember<Enum>{Enum::ExportTGA, "ExportTGA"},
        FormatEnumMember<Enum>{Enum::ShowExportToVideoDialog, "ShowExportToVideoDialog"},
        FormatEnumMember<Enum>{Enum::ExportVideo, "ExportVideo"},
        FormatEnumMember<Enum>{Enum::CloseProject, "CloseProject"}};
};
} // namespace nijigenerate::core::translation

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::commands::puppet::file::FileCommand> {
    inline static constexpr bool isNativeObject = false;
    inline static constexpr char qualifiedArgument[] = "(nijigenerate.commands.puppet.file.FileCommand)";
    inline static constexpr char unqualifiedArgument[] = "(FileCommand)";
    inline static constexpr char qualifiedElement[] = "nijigenerate.commands.puppet.file.FileCommand";
    inline static constexpr char unqualifiedElement[] = "FileCommand";
};
} // namespace nicxlive::core::d_runtime

namespace nijigenerate::commands::puppet::file {

using CommandTable = ::nijigenerate::commands::CommandTableStorage<FileCommand>;
extern thread_local CommandTable commands;
}

// Complete original module surface, including the registered concrete commands.
#include "file_commands.hpp"
