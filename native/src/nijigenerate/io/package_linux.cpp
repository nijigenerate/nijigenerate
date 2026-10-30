/*
    Copyright (c) 2020-2023, Inochi2D Project
    Copyright (c) 2024, nijigenerate Project
    Distributed under the 2-Clause BSD License, see LICENSE file.
    Linux translation of the independently authored io/package.d.
*/
#include "package.hpp"
#include "imageexport_case.hpp"
#include <nijigenerate/platform/portal_filechooser.hpp>
#include <nijigenerate/actions/node_hierarchy_actions.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/core/window.hpp>
#include <nijigenerate/compat/exception_message.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/uri_exception.hpp>
#include <nijigenerate/compat/phobos_unicode_exception.hpp>
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
template<std::size_t N> [[noreturn]] void uriError(const char (&message)[N], std::size_t line) {
    throw compat::URIException(compat::RetainedString::fromStaticLiteral(message),
        compat::RetainedString::fromStaticLiteral("std/uri.d"), line);
}
int hex(unsigned char value, std::size_t sourceLine) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    uriError("Expected two hexadecimal digits after '%'", sourceLine);
}
unsigned char escapedByte(std::string_view input, std::size_t offset, std::size_t hexLine = 228) {
    if (offset >= input.size() || input.size() - offset < 3)
        uriError("Unexpected end of URI", 226);
    if (input[offset] != '%')
        uriError("Expected: '%'", 260);
    const auto high = hex(input[offset + 1], hexLine);
    const auto low = hex(input[offset + 2], hexLine);
    return static_cast<unsigned char>((high << 4) | low);
}
void appendUriCodePoint(std::string& output, std::uint32_t value) {
    if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
        compat::UTFException error(value > 0x10ffff
                ? compat::RetainedString::fromStaticLiteral("Encoding an invalid code point in UTF-8")
                : compat::RetainedString::fromStaticLiteral("Encoding a surrogate code point in UTF-8"),
            compat::RetainedString::fromStaticLiteral("std/utf.d"), 2311);
        const std::array<std::uint32_t, 1> sequence{value};
        error.setSequence(sequence);
        throw error;
    }
    const auto append = [&](char byte) { allocateBytes([&] { output.push_back(byte); }); };
    if (value < 0x80) append(static_cast<char>(value));
    else if (value < 0x800) {
        append(static_cast<char>(0xc0 | (value >> 6)));
        append(static_cast<char>(0x80 | (value & 0x3f)));
    } else if (value < 0x10000) {
        append(static_cast<char>(0xe0 | (value >> 12)));
        append(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
        append(static_cast<char>(0x80 | (value & 0x3f)));
    } else {
        append(static_cast<char>(0xf0 | (value >> 18)));
        append(static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
        append(static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
        append(static_cast<char>(0x80 | (value & 0x3f)));
    }
}
std::string decodeUri(std::string_view input) {
    // Phobos std.uri parses code units into dchars before UTF-8 encoding.
    // Copyright D Language Foundation 2000-2009; BSL-1.0 (phobos-uri.txt).
    compat::Slice<std::uint32_t> values;
    values.resize(input.size(), 0xffff);
    std::size_t written = 0;
    for (std::size_t i = 0; i < input.size();) {
        if (input[i] != '%') {
            values[written++] = static_cast<unsigned char>(input[i++]);
            continue;
        }
        const auto start = i;
        const auto first = escapedByte(input, i);
        i += 3;
        std::uint32_t scalar = first;
        if (first & 0x80) {
            unsigned count = 1;
            for (;; ++count) {
                if (count > 4) uriError("UTF-32 code point size too large", 242);
                if (((first << count) & 0x80) == 0) {
                    if (count == 1) uriError("UTF-32 code point size too small", 246);
                    break;
                }
            }
            scalar = first & ((1u << (7 - count)) - 1);
            if (input.size() - i < 3 * (count - 1)) uriError("UTF-32 unaligned String", 255);
            for (unsigned index = 1; index < count; ++index) {
                const auto byte = escapedByte(input, i, 262);
                if ((byte & 0xc0) != 0x80) uriError("Incorrect UTF-32 multi-byte sequence", 265);
                scalar = (scalar << 6) | (byte & 0x3f);
                i += 3;
            }
            if (scalar > 0x10ffff) uriError("Unknown UTF-32 code point", 270);
        }
        if (scalar < 0x80 &&
                std::string_view(";/?:@&=+$,#").find(static_cast<char>(scalar)) != std::string_view::npos) {
            for (auto byte : input.substr(start, i - start)) values[written++] = static_cast<unsigned char>(byte);
        } else values[written++] = scalar;
    }
    const auto decoded = values.slice(0, written).duplicate();
    std::string output;
    for (auto value : decoded) appendUriCodePoint(output, value);
    return output;
}
IoString portalPath(std::optional<std::string> value, bool decode) {
    if (!value) return {};
    // Keep original uriFromPromise's %20 replacement before the seven-byte
    // prefix slice. Do not infer a different URI scheme or join multiple URIs.
    for (std::size_t i = 0; (i = value->find("%20", i)) != std::string::npos;) {
        value->replace(i, 3, " "); ++i;
    }
    if (value->size() < 7) throw compat::ngStaticError<compat::ArraySliceError>(7, value->size(), value->size(),
        compat::RetainedString::fromStaticLiteral("nijigenerate/io/package.d"), 74);
    auto path = allocateBytes([&] { return value->substr(7); });
    if (decode) path = decodeUri(path);
    return path.empty() ? IoString{} : IoString(std::move(path));
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
    try {
        return portalPath(platform::portal::ngChoosePortalFile(core::ngGetWindowPtr(), title.view(),
            filters, false, multiple), true);
    } catch (...) {
        const compat::SourceCString caption{compat::RetainedStringSlice(title)};
        return duplicateCString(tfd::tinyfd_openFileDialog(caption.data(), "", filters, multiple));
    }
}
IoString incShowOpenFolderDialog(IoString title) {
    try {
        return portalPath(platform::portal::ngChoosePortalFile(core::ngGetWindowPtr(), title.view(),
            {}, false, false, true), true);
    } catch (...) {
        const compat::SourceCString caption{compat::RetainedStringSlice(title)};
        return duplicateCString(tfd::tinyfd_selectFolderDialog(caption.data(), nullptr));
    }
}
IoString incShowOpenDialog(std::span<const TFD_Filter> filters, IoString title) {
    try {
        return portalPath(platform::portal::ngChoosePortalFile(core::ngGetWindowPtr(), title.view(), filters, false), true);
    } catch (...) {
        const compat::SourceCString caption{compat::RetainedStringSlice(title)};
        return duplicateCString(tfd::tinyfd_openFileDialog(caption.data(), "", filters, false));
    }
}
IoString incShowSaveDialog(std::span<const TFD_Filter> filters, IoString filename, IoString title) {
    try {
        // Source SaveFile options do not receive fname; successful SaveFile also
        // deliberately omits the outer std.uri.decode used by the open paths.
        return portalPath(platform::portal::ngChoosePortalFile(core::ngGetWindowPtr(), title.view(), filters, true), false);
    } catch (...) {
        const compat::SourceCString caption{compat::RetainedStringSlice(title)};
        const compat::SourceCString initial{compat::RetainedStringSlice(filename)};
        return duplicateCString(tfd::tinyfd_saveFileDialog(caption.data(), initial.data(), filters));
    }
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
