#include "package.hpp"
#include "imageexport_case.hpp"
#include <nijigenerate/actions/node_hierarchy_actions.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/exception_message.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/project.hpp>
#include <nodes/part.hpp>
#include <texture.hpp>
#include <compat/source_allocator.hpp>
#include <array>
#include <cstring>
#include <new>
#include <stdexcept>
#include <source_location>
#include <string>
#include <utility>

namespace nijigenerate::io {
namespace {
namespace tfd = platform::tinyfiledialogs;
namespace dialog = widgets::dialog;
template<class Operation> decltype(auto) allocateBytes(Operation&& operation) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}

IoString duplicateCString(const char* value) {
    const auto view = incToDString(value);
    return view.empty() ? IoString{} : IoString(view);
}
bool separator(char value) { return value == '/' || value == '\\'; }
std::string_view baseName(std::string_view path) {
    const bool unc = path.size() >= 3 && separator(path[0]) && separator(path[1]) && !separator(path[2]);
    auto remainder = path;
    if (path.size() >= 2 && path[1] == ':') remainder.remove_prefix(2);
    else if (unc) {
        std::size_t root = 3;
        while (root < path.size() && !separator(path[root])) ++root;
        if (root < path.size()) {
            auto next = root;
            do { ++next; } while (next < path.size() && separator(path[next]));
            if (next < path.size()) {
                do { ++next; } while (next < path.size() && !separator(path[next]));
                root = next;
            }
        }
        remainder.remove_prefix(root);
    }
    if (remainder.empty()) return unc ? path.substr(0, 1) : std::string_view{};
    auto trimmed = remainder;
    while (!trimmed.empty() && separator(trimmed.back())) trimmed.remove_suffix(1);
    if (trimmed.empty()) return remainder.substr(0, 1);
    const auto last = trimmed.find_last_of("/\\");
    return last == std::string_view::npos ? trimmed : trimmed.substr(last + 1);
}
compat::RetainedString lowerExtension(IoString filename) {
    const auto bytes = filename.view();
    for (auto index = bytes.size(); index > 0;) {
        --index;
        if (separator(bytes[index]) || bytes[index] == ':') break;
        if (bytes[index] == '.' && index > 0 &&
            !separator(bytes[index - 1]) && bytes[index - 1] != ':')
            return imageexport::detail::lowerExtension(filename.slice(index, bytes.size()));
    }
    return {};
}
std::string formatOne(compat::RetainedString pattern, IoString value) {
    const std::array arguments{core::translation::ngFormatArgument(value.retainedString())};
    return core::translation::ngFormatTyped(pattern.view(), arguments);
}
template<class MakeMessage> void errorDialog(MakeMessage makeMessage) {
    const auto title = core::translation::ngTranslateCString("Error");
    const auto message = makeMessage();
    const auto owner = message.retentionOwner();
    const auto body = owner.use_count() ? dialog::DialogText::retained(message.view(), owner)
        : dialog::DialogText::borrowed(message.view());
    dialog::ngDialog(dialog::DialogTag::retained(title.data(), title.owner), body);
}
} // namespace

std::string_view incToDString(const char* text) {
    return text ? std::string_view(text, std::strlen(text)) : std::string_view{};
}
IoString incShowImportDialog(std::span<const TFD_Filter> filters, IoString title, bool multiple) {
    const compat::SourceCString ctitle{compat::RetainedStringSlice(title)};
    return duplicateCString(tfd::tinyfd_openFileDialog(ctitle.data(), "", filters, multiple));
}
IoString incShowOpenFolderDialog(IoString title) {
    const compat::SourceCString ctitle{compat::RetainedStringSlice(title)};
    return duplicateCString(tfd::tinyfd_selectFolderDialog(ctitle.data(), nullptr));
}
IoString incShowOpenDialog(std::span<const TFD_Filter> filters, IoString title) {
    const compat::SourceCString ctitle{compat::RetainedStringSlice(title)};
    return duplicateCString(tfd::tinyfd_openFileDialog(ctitle.data(), "", filters, false));
}
IoString incShowSaveDialog(std::span<const TFD_Filter> filters, IoString filename, IoString title) {
    const compat::SourceCString ctitle{compat::RetainedStringSlice(title)};
    const compat::SourceCString cfilename{compat::RetainedStringSlice(filename)};
    return duplicateCString(tfd::tinyfd_saveFileDialog(ctitle.data(), cfilename.data(), filters));
}
void incCreatePartsFromFiles(compat::Slice<IoString> files) {
    const auto count = files.size();
    for (std::size_t index = 0; index < count; ++index) {
        const auto file = files[index];
        const auto nameBytes = baseName(file.view());
        const auto nameOffset = nameBytes.data() ? static_cast<std::size_t>(nameBytes.data() - file.data()) : 0;
        const auto name = file.slice(nameOffset, nameOffset + nameBytes.size());
        const auto loweredExtension = lowerExtension(name);
        const auto extension = loweredExtension.view();
        if (extension != ".png" && extension != ".tga" && extension != ".jpeg" && extension != ".jpg") {
            const auto invalidExtension = lowerExtension(name);
            const auto message = allocateBytes([&] {
                std::string bytes{"Invalid file type "};
                bytes.append(invalidExtension.view());
                return compat::RetainedString(std::move(bytes));
            });
            throw compat::ThrowableException(message,
                compat::RetainedString::fromStaticLiteral("nijigenerate/io/package.d"), 208);
        }
        try {
            auto texture = std::allocate_shared<nicxlive::core::ShallowTexture>(
                nicxlive::core::d_runtime::SourceAllocator<nicxlive::core::ShallowTexture>{}, file.retainedString());
            nicxlive::core::inTexPremultiply(texture->data, texture->channels);
            auto part = nicxlive::core::nodes::Part::createSimpleFromShallowRetained(
                *texture, name.retainedString().toNullableString());
            const auto parent = project::incSelectedNode();
            actions::node::incAddChildWithHistory(part, parent, name);
        } catch (const compat::ThrowableException& exception) {
            const auto message = exception.msg.view();
            if (message.size() < 11) throw compat::ngStaticError<compat::ArraySliceError>(0, 11, message.size(),
                compat::RetainedString::fromStaticLiteral("nijigenerate/io/package.d"), 200);
            if (message.substr(0, 11) == "unsupported") errorDialog([name] {
                return compat::RetainedString(formatOne(core::translation::ngTranslateRetained(
                    compat::RetainedString::fromStaticLiteral("%s is not supported")), name));
            });
            else errorDialog([&exception] {
                return exception.msg;
            });
        }
        compat::ngRequireObject(project::incActivePuppet())->rescanNodes();
    }
}
IoString incGetKeepLayerFolder() {
    if (core::settings::incSettingsCanGet("KeepLayerFolder"))
        return core::settings::incSettingsGet<IoString>("KeepLayerFolder");
    return IoString(compat::RetainedString::fromStaticLiteral("NotPreserve"));
}
bool incSetKeepLayerFolder(IoString selection) {
    core::settings::incSettingsSet("KeepLayerFolder", selection);
    return true;
}
ImportKeepHandler::ImportKeepHandler()
    : DialogHandler(dialog::DialogTag::borrowed(DefaultDialogTag)) { prepareSourceClassInfo(); }
bool ImportKeepHandler::onClickCancel() {
    const auto receiver = shared_from_this();
    return load(AskKeepLayerFolder::Cancel);
}
bool ImportKeepHandler::onClickYes() {
    const auto receiver = shared_from_this();
    return load(AskKeepLayerFolder::Preserve);
}
bool ImportKeepHandler::onClickNo() {
    const auto receiver = shared_from_this();
    return load(AskKeepLayerFolder::NotPreserve);
}
// The original overridable base method returns false; concrete loaders override it.
bool ImportKeepHandler::load(AskKeepLayerFolder) { return false; }
void ImportKeepHandler::show() {
    const auto receiver = shared_from_this();
    const auto tag = dialog::DialogTag::borrowed(INC_KEEP_STRUCT_DIALOG_NAME);
    const auto title = core::translation::ngTranslateCString("File import");
    const auto message = core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(
        "Would you like to keep the folder structure of the imported file?\n\nYou can change the default behaviour in the settings."));
    const auto messageOwner = message.retentionOwner();
    const auto body = messageOwner.use_count() ? dialog::DialogText::retained(message.view(), messageOwner)
        : dialog::DialogText::borrowed(message.view());
    dialog::ngDialog(tag,
        dialog::DialogTag::retained(title.data(), title.owner), body,
        dialog::DialogLevel::Warning, DialogButtons::Yes | DialogButtons::No | DialogButtons::Cancel);
}
bool incKeepStructDialog(std::shared_ptr<ImportKeepHandler> handler) {
    if (incGetKeepLayerFolder().view() == "Preserve") compat::ngRequireObject(handler)->load(AskKeepLayerFolder::Preserve);
    else if (incGetKeepLayerFolder().view() == "NotPreserve") compat::ngRequireObject(handler)->load(AskKeepLayerFolder::NotPreserve);
    else { compat::ngRequireObject(handler)->registerHandler(); handler->show(); }
    return true;
}
} // namespace nijigenerate::io
