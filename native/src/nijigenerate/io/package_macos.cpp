/*
    Copyright (c) 2020-2023, Inochi2D Project
    Copyright (c) 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    macOS translation of the independently authored io/package.d.
*/
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
#include <cstdint>
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
std::string_view baseName(std::string_view path) {
    if (path.empty()) return {};
    while (path.size() > 1 && path.back() == '/') path.remove_suffix(1);
    if (path == "/") return path;
    const auto last = path.find_last_of('/');
    return last == std::string_view::npos ? path : path.substr(last + 1);
}
compat::RetainedString lowerExtension(IoString filename) {
    const auto bytes = filename.view();
    const auto dot = bytes.find_last_of('.');
    if (dot == 0 || dot == std::string_view::npos) return {};
    return imageexport::detail::lowerExtension(filename.slice(dot, bytes.size()));
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
}
std::string_view incToDString(const char* value) {
    return value ? std::string_view(value, std::strlen(value)) : std::string_view{};
}
IoString incShowImportDialog(std::span<const TFD_Filter> filters, IoString title, bool multiple) {
    const compat::SourceCString caption{compat::RetainedStringSlice(title)};
    return duplicateCString(tfd::tinyfd_openFileDialog(caption.data(), "", filters, multiple));
}
IoString incShowOpenFolderDialog(IoString title) {
    const compat::SourceCString caption{compat::RetainedStringSlice(title)};
    return duplicateCString(tfd::tinyfd_selectFolderDialog(caption.data(), nullptr));
}
IoString incShowOpenDialog(std::span<const TFD_Filter> filters, IoString title) {
    const compat::SourceCString caption{compat::RetainedStringSlice(title)};
    return duplicateCString(tfd::tinyfd_openFileDialog(caption.data(), "", filters, false));
}
IoString incShowSaveDialog(std::span<const TFD_Filter> filters, IoString filename, IoString title) {
    const compat::SourceCString caption{compat::RetainedStringSlice(title)};
    const compat::SourceCString initial{compat::RetainedStringSlice(filename)};
    return duplicateCString(tfd::tinyfd_saveFileDialog(caption.data(), initial.data(), filters));
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
        } catch (const compat::ThrowableException& error) {
            const auto message = error.msg.view();
            if (message.size() < 11) throw compat::ngStaticError<compat::ArraySliceError>(0, 11, message.size(),
                compat::RetainedString::fromStaticLiteral("nijigenerate/io/package.d"), 200);
            if (message.substr(0, 11) == "unsupported") errorDialog([name] {
                return compat::RetainedString(formatOne(core::translation::ngTranslateRetained(
                    compat::RetainedString::fromStaticLiteral("%s is not supported")), name));
            });
            else errorDialog([&error] {
                return error.msg;
            });
        }
        compat::ngRequireObject(project::incActivePuppet())->rescanNodes();
    }
}
IoString incGetKeepLayerFolder() {
    return core::settings::incSettingsCanGet("KeepLayerFolder") ?
        core::settings::incSettingsGet<IoString>("KeepLayerFolder")
            : IoString(compat::RetainedString::fromStaticLiteral("NotPreserve"));
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
}
