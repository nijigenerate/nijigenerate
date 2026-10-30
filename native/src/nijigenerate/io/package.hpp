#pragma once

#include "psd.hpp"
#include "depthmap_psd.hpp"
#include "kra.hpp"
#include "inpexport.hpp"
#include "videoexport.hpp"
#include "imageexport.hpp"
#include <nijigenerate/platform/tinyfiledialogs.hpp>
#include <nijigenerate/core/settings_string.hpp>
#include <nijigenerate/core/typed_format_enum.hpp>
#include <nijigenerate/compat/retained_string.hpp>
#include <nijigenerate/compat/slice.hpp>
#include <nijigenerate/widgets/dialog.hpp>
#include <common/object_key_policy.hpp>
#include <compat/object_metadata.hpp>
#include <array>
#include <memory>
#include <span>
#include <string_view>

namespace nijigenerate::io {
using IoString = core::settings::SettingsString;
using TFD_Filter = platform::tinyfiledialogs::TFD_Filter;
using DialogButtons = widgets::dialog::DialogButtons;

// Like fromStringz, this is a borrowed view of the caller's C buffer.
std::string_view incToDString(const char* text);
IoString incShowImportDialog(std::span<const TFD_Filter> filters, IoString title, bool multiple = false);
IoString incShowOpenFolderDialog(IoString title = compat::RetainedString::fromStaticLiteral("Open..."));
IoString incShowOpenDialog(std::span<const TFD_Filter> filters,
    IoString title = compat::RetainedString::fromStaticLiteral("Open..."));
IoString incShowSaveDialog(std::span<const TFD_Filter> filters, IoString filename,
    IoString title = compat::RetainedString::fromStaticLiteral("Save..."));
void incCreatePartsFromFiles(compat::Slice<IoString> files);
IoString incGetKeepLayerFolder();
bool incSetKeepLayerFolder(IoString selection);

enum class AskKeepLayerFolder { Preserve, NotPreserve, Cancel };
class ImportKeepHandler : public widgets::dialog::DialogHandler {
public:
    inline static constexpr char DefaultDialogTag[] = "ImportKeepFolderStructPopup";
    ImportKeepHandler();
    bool onClickCancel() override;
    bool onClickYes() override;
    bool onClickNo() override;
    virtual bool load(AskKeepLayerFolder selection);
    virtual void show();
    const char* INC_KEEP_STRUCT_DIALOG_NAME = DefaultDialogTag;
protected:
    void prepareSourceClassInfo() override {
        widgets::dialog::DialogHandler::prepareSourceClassInfo();
        static const bool registered = [] {
            nicxlive::core::d_runtime::ngRegisterObjectTypeNames<ImportKeepHandler>(
                compat::RetainedString::fromStaticLiteral("nijigenerate.io.ImportKeepHandler"),
                compat::RetainedString::fromStaticLiteral("(nijigenerate.io.ImportKeepHandler)"));
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(ImportKeepHandler),
                compat::RetainedString::fromStaticLiteral("nijigenerate.io.ImportKeepHandler"));
            return true;
        }();
        (void)registered;
    }
};
bool incKeepStructDialog(std::shared_ptr<ImportKeepHandler> handler);
} // namespace nijigenerate::io

namespace nijigenerate::core::translation {
template<> struct FormatEnumTraits<::nijigenerate::io::AskKeepLayerFolder> {
    using Value = ::nijigenerate::io::AskKeepLayerFolder;
    inline static constexpr std::string_view name = "AskKeepLayerFolder";
    inline static constexpr std::array members{
        FormatEnumMember<Value>{Value::Preserve, "Preserve"},
        FormatEnumMember<Value>{Value::NotPreserve, "NotPreserve"},
        FormatEnumMember<Value>{Value::Cancel, "Cancel"}};
};
}

namespace nicxlive::core::d_runtime {
template<> struct SourceTypeSpellings<::nijigenerate::io::ImportKeepHandler> {
    inline static constexpr bool isNativeObject = true;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.io.ImportKeepHandler)"),
            RetainedString::fromStaticLiteral("(ImportKeepHandler)"),
            RetainedString::fromStaticLiteral("nijigenerate.io.ImportKeepHandler"),
            RetainedString::fromStaticLiteral("ImportKeepHandler")};
    }
};
template<> struct SourceTypeSpellings<::nijigenerate::io::AskKeepLayerFolder> {
    inline static constexpr bool isNativeObject = false;
    static SourceTypeSpellingNames get() {
        return {RetainedString::fromStaticLiteral("(nijigenerate.io.AskKeepLayerFolder)"),
            RetainedString::fromStaticLiteral("(AskKeepLayerFolder)"),
            RetainedString::fromStaticLiteral("nijigenerate.io.AskKeepLayerFolder"),
            RetainedString::fromStaticLiteral("AskKeepLayerFolder")};
    }
};
template<> struct SourceObjectTypeNames<::nijigenerate::io::ImportKeepHandler> {
    inline static constexpr char className[] = "nijigenerate.io.ImportKeepHandler";
    inline static constexpr char templateArgumentName[] = "(nijigenerate.io.ImportKeepHandler)";
};
} // namespace nicxlive::core::d_runtime
