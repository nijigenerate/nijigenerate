#include <nijigenerate/panels/agent_panel.hpp>
#include <nijigenerate/compat/json_stringify.hpp>
#include <nijigenerate/core/translation.hpp>
#include <nijigenerate/core/typed_format.hpp>
#include <nijigenerate/compat/unicode_strip.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/phobos_unicode_cursor.hpp>
#include <nijigenerate/widgets/button.hpp>
#include <nijigenerate/widgets/inputtext.hpp>
#include <nijigenerate/utils/crashdump.hpp>
#include <nijigenerate/core/settings.hpp>
#include <nijigenerate/core/thread_context.hpp>
#include <nijigenerate/compat/utf8.hpp>
#include <nijigenerate/compat/phobos_file_exception.hpp>
#include <nijigenerate/compat/phobos_file_write.hpp>
#include <nijigenerate/compat/phobos_file_directory.hpp>
#include <nijigenerate/compat/phobos_file_exists.hpp>
#include <nijigenerate/compat/phobos_temp_cstring.hpp>
#include <nijigenerate/compat/thread_exception.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/platform/environment.hpp>
#include <compat/source_allocator.hpp>
#include <algorithm>
#include <concepts>
#include <functional>
#include <span>
#include <cerrno>
#include <array>
#include <new>
#include <source_location>
#include <utility>
#include <vector>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace nijigenerate::panels::agent {
namespace {
template<std::size_t N> AgentString literal(const char (&text)[N]) {
    return compat::RetainedString::fromStaticLiteral(text);
}

template<class Array> decltype(auto) agentElement(Array& values, std::size_t index, std::size_t line) {
    const auto length = values.size();
    if (index >= length) throw compat::ngStaticError<compat::ArrayIndexError>(index, length,
        compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/agent.d"), line);
    return values[index];
}

template<class Array> auto agentSlice(Array& values, std::size_t first, std::size_t last, std::size_t line) {
    const auto length = values.size();
    if (first > last || last > length)
        throw compat::ngStaticError<compat::ArraySliceError>(first, last, length,
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/panels/agent.d"), line);
    return values.slice(first, last);
}
// Use only for the primitive byte/string backing operations below. Calls into
// source callbacks, formatters and OS providers remain outside this boundary.
template<class Operation> decltype(auto) textBacking(Operation&& operation,
    std::source_location source = std::source_location::current()) {
    try {
        return std::forward<Operation>(operation)();
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(source);
    }
}

AgentString owned(std::string text) {
    return textBacking([&]() -> AgentString { return compat::RetainedString(std::move(text)); });
}
std::string copyBytes(std::string_view bytes) {
    return textBacking([&] { return std::string(bytes); });
}
std::string spaces(std::size_t count) {
    return textBacking([&] { return std::string(count, ' '); });
}
void appendBytes(std::string& target, std::string_view bytes) {
    textBacking([&] { target.append(bytes); });
}

// core.internal.utf.decode used by rt.aApplycd2. Copyright Digital Mars 2003-2016.
// Authors: Walter Bright, Sean Kelly.
// BSL-1.0 (phobos-agent-path.txt). The foreach caller guarantees offset < size.
char32_t decodeAgentForeachUtf8(std::string_view bytes, std::size_t& offset) {
    const auto start = offset;
    const auto invalid = [start]() -> char32_t {
        throw compat::UnicodeException(
            compat::RetainedString::fromStaticLiteral("invalid UTF-8 sequence"), start,
            compat::RetainedString::fromStaticLiteral("core/internal/utf.d"), 294);
    };
    const auto first = static_cast<std::uint8_t>(bytes[start]);
    if (!(first & 0x80)) {
        offset = start + 1;
        return first;
    }
    unsigned width;
    for (width = 1; ; ++width) {
        if (width > 4) return invalid();
        if (((first << width) & 0x80) == 0) {
            if (width == 1) return invalid();
            break;
        }
    }
    std::uint32_t scalar = first & ((1u << (7 - width)) - 1);
    if (width > bytes.size() - start) return invalid();
    const auto second = static_cast<std::uint8_t>(bytes[start + 1]);
    if ((first & 0xfe) == 0xc0 || (first == 0xe0 && (second & 0xe0) == 0x80)
        || (first == 0xf0 && (second & 0xf0) == 0x80)
        || (first == 0xf8 && (second & 0xf8) == 0x80)
        || (first == 0xfc && (second & 0xfc) == 0x80)) return invalid();
    for (unsigned i = 1; i < width; ++i) {
        const auto unit = static_cast<std::uint8_t>(bytes[start + i]);
        if ((unit & 0xc0) != 0x80) return invalid();
        scalar = (scalar << 6) | (unit & 0x3f);
    }
    if ((scalar >= 0xd800 && scalar <= 0xdfff) || scalar > 0x10ffff) return invalid();
    offset = start + width;
    return static_cast<char32_t>(scalar);
}

constexpr bool agentWhitespace(std::uint32_t scalar) {
    return (scalar >= 9 && scalar <= 13) || scalar == 0x20 || scalar == 0x85 || scalar == 0xa0
        || scalar == 0x1680 || (scalar >= 0x2000 && scalar <= 0x200a) || scalar == 0x2028
        || scalar == 0x2029 || scalar == 0x202f || scalar == 0x205f || scalar == 0x3000;
}

// std.string.stripRight byte path, D Language Foundation 2007-, BSL-1.0.
// Complete the canonical prefix/suffix stripper with the authored reverse
// bit-pattern handling, which also accepts overlong whitespace encodings.
std::string_view stripAgentText(std::string_view input) {
    input = compat::ngStripUnicode(input);
    auto end = input.size();
    while (end) {
        const auto last = static_cast<std::uint8_t>(input[end - 1]);
        if (last <= 0x7f) {
            if (!agentWhitespace(last)) break;
            --end;
            continue;
        }
        if (end < 2 || (last & 0xc0) != 0x80) break;
        const auto low = last & 0x3f;
        const auto second = static_cast<std::uint8_t>(input[end - 2]);
        if ((second & 0xe0) == 0xc0) {
            if (!agentWhitespace(low + ((second & 0x1f) << 6))) break;
            end -= 2;
            continue;
        }
        if (end < 3 || (second & 0xc0) != 0x80) break;
        const auto third = static_cast<std::uint8_t>(input[end - 3]);
        if ((third & 0xf0) != 0xe0
            || !agentWhitespace(low + ((second & 0x3f) << 6) + ((third & 0x0f) << 12))) break;
        end -= 3;
    }
    return input.substr(0, end);
}

template<std::integral T> AgentString decimalText(T value) {
    return owned(textBacking([&] { return std::to_string(value); }));
}

// Byte-array concatenation follows core.internal.array.concatenation,
// Digital Mars 2000-2019, BSL-1.0; license text in phobos-agent-path.txt.
AgentString concatenate(std::initializer_list<std::string_view> parts) {
    std::size_t length = 0;
    for (auto part : parts) length += part.size();
    if (!length) return {};
    // D array concatenation allocates the full extent before copying inputs.
    return textBacking([&]() -> AgentString {
        std::string result(length, '\0');
        std::size_t offset = 0;
        for (auto part : parts) {
            if (!part.empty()) std::copy(part.begin(), part.end(), result.data() + offset);
            offset += part.size();
        }
        return compat::RetainedString(std::move(result));
    });
}

// Same selected array-concatenation operation for the real closed JSONValue
// payload: allocate the full extent before copying any input elements.
compat::Slice<AgentJson> concatenateAgentJsonArrays(
    std::initializer_list<compat::ConstSlice<AgentJson>> parts) {
    std::size_t length = 0;
    for (const auto& part : parts) length += part.size();
    if (!length) return {};
    compat::Slice<AgentJson> result;
    // Slice.resize grows via append; adopt the actual full-size backing instead.
    // JSONValue default construction is closed, noexcept and invokes no user code.
    try {
        result = compat::Slice<AgentJson>(std::vector<AgentJson>(length));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    std::size_t offset = 0;
    for (const auto& part : parts)
        for (const auto& value : part) result[offset++] = value;
    return result;
}

// The selected std.array.join cheap-iteration branch for actual char strings.
// Copyright Andrei Alexandrescu 2008- and Jonathan M Davis 2011-, BSL-1.0.
AgentString joinAgentStrings(compat::ConstSlice<AgentString> parts, AgentString separator) {
    if (parts.empty()) return {};
    std::size_t length = 0;
    for (const auto& part : parts) length += part.size();
    length += (parts.size() - 1) * separator.size();
    if (!length) return {};
    return textBacking([&]() -> AgentString {
        std::string result(length, '\0');
        std::size_t offset = 0;
        bool first = true;
        const auto copy = [&](std::string_view bytes) {
            if (!bytes.empty()) std::copy(bytes.begin(), bytes.end(), result.data() + offset);
            offset += bytes.size();
        };
        for (const auto& part : parts) {
            if (!first) copy(separator.view());
            first = false;
            copy(part.view());
        }
        return compat::RetainedString(std::move(result));
    });
}

AgentString formatted(std::string_view pattern,
    std::span<const core::translation::TypedFormatArgument> arguments) {
    std::string result;
    const auto writer = textBacking([&] {
        return core::translation::TypedFormatSink([&](std::string_view bytes) { appendBytes(result, bytes); });
    });
    const auto consumed = core::translation::detail::formatTypedTo(writer, pattern, arguments);
    if (consumed != arguments.size()) {
        const auto message = concatenate({"Orphan format arguments: args[", decimalText(consumed).view(), "..",
            decimalText(arguments.size()).view(), "]"});
        core::translation::detail::formatFailure(copyBytes(message.view()), "std/format/package.d", 785);
    }
    return result.empty() ? AgentString{} : owned(std::move(result));
}

template<class T, class... Args> std::shared_ptr<T> allocateAgentObject(Args&&... args) {
    return std::allocate_shared<T>(nicxlive::core::d_runtime::SourceAllocator<T>{},
        std::forward<Args>(args)...);
}
template<std::size_t N> AgentString translated(const char (&text)[N]) {
    return core::translation::ngTranslateRetained(compat::RetainedString::fromStaticLiteral(text));
}

void lockQueueMutex(std::shared_ptr<nicxlive::core::d_runtime::SourceMutex> mutex) {
    const auto receiver = compat::ngRequireObject(mutex);
    receiver->lock();
}

// Required two-segment std.path operations, following the permitted Phobos
// byte-path rules. Copyright (c) 2000-2014 the Phobos std.path authors.
// BSL-1.0: native/port/licenses/phobos-agent-path.txt.
bool pathSeparator(char value) noexcept {
    return value == '/'
#ifdef _WIN32
        || value == '\\'
#endif
        ;
}
bool absolutePath(std::string_view path) noexcept {
#ifdef _WIN32
    if (path.size() < 3) return false;
    return (path[1] == ':' && pathSeparator(path[2]))
        || (pathSeparator(path[0]) && pathSeparator(path[1]) && !pathSeparator(path[2]));
#else
    return !path.empty() && pathSeparator(path[0]);
#endif
}
std::size_t rootLength(std::string_view path) noexcept {
    if (path.empty()) return 0;
#ifdef _WIN32
    if (pathSeparator(path[0])) {
        if (path.size() >= 3 && pathSeparator(path[1]) && !pathSeparator(path[2])) {
            std::size_t serverEnd = 3;
            while (serverEnd < path.size() && !pathSeparator(path[serverEnd])) ++serverEnd;
            auto share = serverEnd;
            while (share < path.size() && pathSeparator(path[share])) ++share;
            if (share == path.size()) return serverEnd;
            while (share < path.size() && !pathSeparator(path[share])) ++share;
            return share;
        }
        return 1;
    }
    return path.size() >= 3 && path[1] == ':' && pathSeparator(path[2]) ? 3 : 0;
#else
    return pathSeparator(path[0]) ? 1 : 0;
#endif
}
AgentString buildAgentPath(AgentString base, AgentString leaf) {
    const auto capacity = base.size() + 1 + leaf.size() + 1;
    auto buffer = textBacking([&] { return std::make_shared<std::string>(capacity, static_cast<char>(0xff)); });
    std::size_t position = 0;
    for (const auto segment : {base, leaf}) {
        if (segment.empty()) continue;
        const auto second = segment.view();
        const std::string_view first(buffer->data(), position);
        auto prefix = position;
        bool separator = false;
        if (prefix) {
            if (pathSeparator(second[0]) || absolutePath(second)) {
                prefix = 0;
#ifdef _WIN32
                if (!absolutePath(second)) {
                    prefix = rootLength(first);
                    if (prefix && pathSeparator(first[prefix - 1])) --prefix;
                }
#endif
            } else separator = !pathSeparator(first[prefix - 1]);
        }
        std::size_t index = 0;
        const auto put = [&](char value) {
            if (index >= buffer->size()) throw compat::ngStaticError<compat::ArrayIndexError>(
                index, buffer->size(), compat::RetainedString::fromStaticLiteral("std/path.d"), 1477);
            (*buffer)[index++] = value;
        };
        for (std::size_t i = 0; i < prefix; ++i) put(first[i]);
        if (separator) {
#ifdef _WIN32
            put('\\');
#else
            put('/');
#endif
        }
        for (const char value : second) put(value);
        position = index;
    }
    return compat::RetainedString::retainedView(buffer, buffer->data(), position);
}

#ifdef _WIN32
AgentString widePath(std::span<const wchar_t> units) {
    compat::phobos::detail::UnicodeCursor<std::span<const wchar_t>, wchar_t> cursor(units);
    std::string bytes;
    textBacking([&] { bytes.reserve(units.size()); });
    while (!cursor.empty()) {
        const auto scalar = cursor.front();
        std::array<char, 4> encoded;
        std::size_t count;
        if (scalar < 0x80) {
            encoded[0] = static_cast<char>(scalar);
            count = 1;
        } else if (scalar < 0x800) {
            encoded[0] = static_cast<char>(0xc0 | (scalar >> 6));
            encoded[1] = static_cast<char>(0x80 | (scalar & 0x3f));
            count = 2;
        } else if (scalar < 0x10000) {
            encoded[0] = static_cast<char>(0xe0 | (scalar >> 12));
            encoded[1] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3f));
            encoded[2] = static_cast<char>(0x80 | (scalar & 0x3f));
            count = 3;
        } else {
            encoded[0] = static_cast<char>(0xf0 | (scalar >> 18));
            encoded[1] = static_cast<char>(0x80 | ((scalar >> 12) & 0x3f));
            encoded[2] = static_cast<char>(0x80 | ((scalar >> 6) & 0x3f));
            encoded[3] = static_cast<char>(0x80 | (scalar & 0x3f));
            count = 4;
        }
        for (std::size_t i = 0; i < count; ++i) appendBytes(bytes, {encoded.data() + i, 1});
        cursor.popFront();
    }
    return bytes.empty() ? AgentString{} : owned(std::move(bytes));
}
#endif

AgentString currentDirectory() {
    return owned(compat::phobos::ngGetCurrentDirectory());
}

AgentString temporaryDirectory() {
    // The actual source body caches the selected path and falls back to getcwd
    // plus its platform separator; the prose docs also contain an older "." note.
    // https://raw.githubusercontent.com/dlang/phobos/master/std/file.d
    thread_local AgentString cached;
    if (!cached.isNull()) return cached;
    auto addSeparator = [](AgentString path) {
#ifdef _WIN32
        constexpr char separator = '\\';
#else
        constexpr char separator = '/';
#endif
        if (path.empty() || path.view().back() != separator)
            return concatenate({path.view(), std::string_view(&separator, 1)});
        return path;
    };
#ifdef _WIN32
    std::array<wchar_t, MAX_PATH + 2> buffer;
    buffer.fill(static_cast<wchar_t>(0xffff));
    auto count = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (count > buffer.size()) throw compat::ngStaticError<compat::ArraySliceError>(0, count, buffer.size(),
        compat::RetainedString::fromStaticLiteral("std/file.d"), 5382);
    if (count) cached = widePath({buffer.data(), count});
#else
    AgentString selected;
    for (auto key : {"TMPDIR", "TEMP", "TMP"}) {
        const auto value = platform::ngGetEnvironmentVariableRetained(key);
        if (!value.isNull() && !value.empty() && compat::phobos::ngFileExists(value.view())) {
            selected = value;
            break;
        }
    }
    if (selected.isNull()) {
        for (auto path : {"/tmp", "/var/tmp", "/usr/tmp"}) {
            if (compat::phobos::ngFileExists(path)) {
                selected = compat::RetainedString::fromStaticView(path);
                break;
            }
        }
    }
    if (!selected.isNull()) cached = addSeparator(selected);
#endif
    if (cached.isNull()) cached = addSeparator(currentDirectory());
    return cached;
}
}

AgentPanel::Strings AgentPanel::splitLines(AgentString text) {
    Strings result;
    std::size_t start = 0;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text.view()[i] == '\n') {
            result.append(text.slice(start, i));
            start = i + 1;
        }
    }
    result.append(text.slice(start, text.size()));
    return result;
}

AgentPanel::Strings AgentPanel::expandLines(ConstStrings source) {
    Strings result;
    for (auto line : source) {
        std::size_t start = 0;
        for (std::size_t i = 0; i < line.size(); ++i) {
            if (line.view()[i] == '\n') {
                result.append(line.slice(start, i));
                start = i + 1;
            }
        }
        result.append(line.slice(start, line.size()));
    }
    return result;
}

AgentString AgentPanel::shrinkLine(AgentString text, std::size_t limit) {
    if (text.size() <= limit) return text;
    const auto head = text.slice(0, limit);
    const auto rest = decimalText(text.size() - limit);
    return concatenate({head.view(), " ... (truncated ", rest.view(), " chars)"});
}
AgentString AgentPanel::shrinkLineChat(AgentString text) { return shrinkLine(text, kMaxChatLineChars); }
AgentString AgentPanel::shrinkLineLog(AgentString text) { return shrinkLine(text, kMaxLogLineChars); }

AgentString AgentPanel::deriveUpdateKind(AgentJson update) {
    if (update.is_object() && update.sourceObjectRef().if_contains("sessionUpdate"))
        return update.sourceIndexValue("sessionUpdate").as_string();
    return literal("session/update");
}

AgentString AgentPanel::deriveToolId(AgentJson update) {
    if (update.is_object()) {
        auto value = update.sourceObjectRef().if_contains("toolCallId");
        if (value && value->is_string()) return value->as_string();
    }
    return literal("");
}

AgentString AgentPanel::extractTextFromContent(AgentJson update) {
    if (!update.is_object()) return literal("");
    AgentJson content;
    if (auto value = update.sourceObjectRef().if_contains("content")) content = *value;
    if (content.is_object() && content.sourceObjectRef().if_contains("text"))
        return content.sourceIndexValue("text").as_string();
    if (content.is_array() && !content.as_array().empty()) {
        AgentString result;
        for (auto item : content.sourceArrayValue()) {
            if (!item.is_object() || !item.sourceObjectRef().if_contains("text")) continue;
            auto text = item.sourceIndexValue("text").as_string();
            if (!text.empty()) result = concatenate({result.view(), text.view()});
            auto annotations = item.sourceObjectRef().if_contains("annotations");
            if (annotations && annotations->is_array()) {
                for (auto annotation : annotations->sourceArrayValue()) {
                    if (annotation.is_object() && annotation.sourceObjectRef().if_contains("title")) {
                        const auto title = annotation.sourceIndexValue("title").as_string();
                        const auto annotationText = concatenate({" (", title.view(), ")"});
                        result = concatenate({result.view(), annotationText.view()});
                    }
                }
            }
        }
        return result;
    }
    return literal("");
}

AgentString AgentPanel::fallbackText(AgentJson update) {
    if (update.is_object()) {
        auto message = update.sourceObjectRef().if_contains("message");
        if (message && message->is_string()) return message->as_string();
        if (auto output = update.sourceObjectRef().if_contains("rawOutput"))
            return compat::ngJsonStringify(*output);
    }
    return compat::ngJsonStringify(update);
}

AgentString AgentPanel::jsonScalarText(AgentJson object, AgentString key) {
    if (!object.is_object()) return literal("");
    auto value = object.sourceObjectRef().if_contains(key.view());
    if (!value) return literal("");
    if (value->is_string()) return value->as_string();
    if (value->is_int64() || value->is_uint64() || value->is_double())
        return compat::ngJsonStringify(*value);
    if (value->is_bool()) return value->as_bool() ? literal("true") : literal("false");
    return literal("");
}

AgentString AgentPanel::firstScalar(AgentJson object, Strings keys) {
    for (auto key : keys) {
        auto value = jsonScalarText(object, key);
        if (!value.empty()) return value;
    }
    return literal("");
}

AgentString AgentPanel::permissionOptionId(AgentJson option) {
    if (!option.is_object()) return literal("");
    for (auto key : {"optionId", "id"}) {
        auto value = option.sourceObjectRef().if_contains(key);
        if (value && value->is_string()) return value->as_string();
    }
    return literal("");
}

AgentString AgentPanel::permissionOptionLabel(AgentJson option) {
    if (option.is_object()) {
        const Strings keys{literal("name"), literal("label"), literal("title"), literal("kind"),
            literal("action"), literal("optionId"), literal("id")};
        for (auto key : keys) {
            auto value = option.sourceObjectRef().if_contains(key.view());
            if (value && value->is_string() && !value->as_string().empty()) return value->as_string();
        }
    }
    return literal("Option");
}

AgentPanel::Jsons AgentPanel::permissionOptions(AgentJson params) {
    if (!params.is_object()) return {};
    auto options = params.sourceObjectRef().if_contains("options");
    if (!options || !options->is_array()) return {};
    return options->as_array().duplicate();
}

AgentPanel::Strings AgentPanel::parseCommandLine(AgentString command) {
    enum class State { normal, single, doubleQuote };
    State state = State::normal;
    Strings result;
    std::string current;
    for (char ch : command.view()) {
        switch (state) {
        case State::normal:
            if (ch == ' ' || ch == '\t') {
                if (!current.empty()) { result.append(owned(std::move(current))); current.clear(); }
            } else if (ch == '"') state = State::doubleQuote;
            else if (ch == '\'') state = State::single;
            else appendBytes(current, std::string_view(&ch, 1));
            break;
        case State::single:
            if (ch == '\'') state = State::normal;
            else appendBytes(current, std::string_view(&ch, 1));
            break;
        case State::doubleQuote:
            if (ch == '"') state = State::normal;
            else appendBytes(current, std::string_view(&ch, 1));
            break;
        }
    }
    if (!current.empty()) result.append(owned(std::move(current)));
    return result;
}

void AgentPanel::clearState() {
    convoBlocks.resize(0);
    pendingBuf = {};
    pendingNewline = false;
    hasPendingRole = false;
    convoContentAdded = false;
    toolBlockIndex = {};
    clearAgentLog();
}

void AgentPanel::ensureBlock(AgentString role, AgentString title) {
    if (convoBlocks.empty() || !(convoBlocks[convoBlocks.size() - 1].role == role)) {
        LogBlock block;
        block.role = role;
        block.title = title;
        block.ver = 0;
        convoBlocks.append(block);
    } else if (!title.empty()) {
        convoBlocks[convoBlocks.size() - 1].title = title;
    }
}

void AgentPanel::pushChatLine(AgentString role, AgentString line, AgentJson payload,
    AgentString title, bool payloadPresent) {
    ensureBlock(role, title);
    // Keep the actual slice storage containing this block through front slicing.
    auto blocks = convoBlocks;
    auto block = &agentElement(blocks, blocks.size() - 1, 913);
    auto index = block->lines.size();
    block->lines.append(line);
    block->payloads.append(payloadPresent ? payload : AgentJson{});
    block->dirtyFrom = std::min(block->dirtyFrom, index);
    ++block->ver;
    convoContentAdded = true;
    std::size_t total = 0;
    for (auto current : convoBlocks) total += current.lines.size();
    if (total > kMaxLines) {
        auto toDrop = total - kMaxLines;
        std::size_t bi = 0;
        while (toDrop > 0 && bi < convoBlocks.size()) {
            auto count = agentElement(convoBlocks, bi, 928).lines.size();
            if (count <= toDrop) {
                toDrop -= count;
                convoBlocks = agentSlice(convoBlocks, bi + 1, convoBlocks.size(), 931);
                bi = 0;
            } else {
                auto& first = agentElement(convoBlocks, bi, 934);
                first.lines = agentSlice(first.lines, toDrop, first.lines.size(), 934).duplicate();
                // The original condition deliberately observes the last block.
                if (block->payloads.size() == block->lines.size() + toDrop)
                    first.payloads = agentSlice(first.payloads, toDrop, first.payloads.size(), 936).duplicate();
                first.dirtyFrom = 0;
                first.cache.dataVer = std::numeric_limits<std::uint32_t>::max();
                toDrop = 0;
            }
        }
    }
}

void AgentPanel::appendToLastLine(AgentString role, AgentString fragment,
    AgentJson payload, bool payloadPresent) {
    ensureBlock(role);
    auto& block = convoBlocks[convoBlocks.size() - 1];
    if (block.lines.empty()) {
        block.lines.append(fragment);
        block.payloads.append(payloadPresent ? payload : AgentJson{});
    } else {
        auto index = block.lines.size() - 1;
        block.lines[index] = concatenate({block.lines[index].view(), fragment.view()});
        if (payloadPresent && block.payloads.size() == block.lines.size())
            block.payloads[block.payloads.size() - 1] = payload;
        else if (payloadPresent && block.payloads.size() == block.lines.size() - 1)
            block.payloads.append(payload);
        block.dirtyFrom = std::min(block.dirtyFrom, index);
    }
    ++block.ver;
    convoContentAdded = true;
}

AgentPanel::LogBlock* AgentPanel::findToolBlock(AgentString role) {
    for (std::size_t i = convoBlocks.size(); i > 0; --i)
        if (convoBlocks[i - 1].role == role) return &convoBlocks[i - 1];
    return nullptr;
}

void AgentPanel::handleToolUpdate(AgentString role, AgentString title, AgentString text,
    AgentJson payload, bool isInitial, AgentString status, AgentString toolId) {
    flushPending();
    LogBlock* block = nullptr;
    if (!toolId.empty()) {
        auto found = toolBlockIndex.find(toolId);
        if (found != toolBlockIndex.end()) {
            if (found->second < convoBlocks.size()) block = &convoBlocks[found->second];
            else toolBlockIndex.erase(toolId);
        }
    }
    if (!block && toolId.empty()) block = findToolBlock(role);
    if (!block) {
        LogBlock next;
        next.role = role;
        next.title = title;
        next.toolId = toolId;
        if (!text.empty()) next.lines.append(text);
        next.payloadMerged = payload;
        next.hasPayloadMerged = !payload.is_null();
        next.status = status;
        next.ver = 1;
        convoBlocks.append(next);
        if (!toolId.empty()) toolBlockIndex[toolId] = convoBlocks.size() - 1;
        convoContentAdded = true;
        return;
    }
    if (!title.empty()) block->title = title;
    if (!text.empty()) {
        if (block->lines.empty()) block->lines.append(text);
        else if (isInitial) block->lines[block->lines.size() - 1] = text;
        else {
            auto& last = block->lines[block->lines.size() - 1];
            last = concatenate({last.view(), text.view()});
        }
    }
    if (!payload.is_null()) {
        block->payloadMerged = block->hasPayloadMerged ? mergeJson(block->payloadMerged, payload) : payload;
        block->hasPayloadMerged = true;
    }
    if (!status.empty()) block->status = status;
    ++block->ver;
    convoContentAdded = true;
}

void AgentPanel::handlePlanUpdate(AgentString text, AgentJson payload, AgentString status) {
    flushPending();
    if (!hasPlan) { hasPlan = true; planView = {}; }
    if (!text.empty()) planView.lines = splitLines(text);
    if (!payload.is_null())
        planView.payload = !planView.payload.is_null() ? mergeJson(planView.payload, payload) : payload;
    if (!status.empty()) planView.status = status;
    ++planView.ver;
}

void AgentPanel::enqueueUserText() {
    if (userText.empty()) return;
    enqueueCommand(concatenate({"msg:", userText.view()}));
    promptPending = true;
    userText = literal("");
    ++userTextInputSerial;
}

void AgentPanel::enqueueCommand(AgentString command) {
    lockQueueMutex(qMutex);
    cmdQueue.append(command);
    compat::ngRequireObject(qMutex)->unlock();
    signalWake();
}

void AgentPanel::pushResult(AgentString kind, bool isError, AgentString message) {
    lockQueueMutex(qMutex);
    resultQueue.append(ACPResult{kind, isError, message});
    compat::ngRequireObject(qMutex)->unlock();
}

void AgentPanel::signalWake() { compat::ngRequireObject(qCond)->notifyAll(); }

void AgentPanel::drainResults() {
    lockQueueMutex(qMutex);
    auto results = resultQueue.duplicate();
    resultQueue.resize(0);
    compat::ngRequireObject(qMutex)->unlock();
    for (auto result : results) {
        auto kind = result.kind.view();
        if (kind == "log" && !result.isError) { log(result.message); continue; }
        if (result.isError) {
            statusText = result.message;
            log(result.message);
            if (kind == "init") initInFlight = false;
        } else if (kind == "init") {
            statusText = literal("Initialized");
            log(concatenate({"initialize: ", result.message.view()}));
            initInFlight = false;
        } else if (kind == "ping") {
            statusText = literal("Ping ok");
            log(literal("ping ok"));
        } else if (kind == "promptDone") promptPending = false;
        else log(result.message);
    }
}

AgentString AgentPanel::deriveRoleFromKind(AgentString kind, AgentString toolId) {
    auto value = kind.view();
    if (value == "agent_message_chunk") return translated("Assistant");
    if (value == "agent_thought_chunk") return translated("Assistant (thinking)");
    if (value == "user_message_chunk") return translated("You (echo)");
    if (value == "plan") return translated("Plan");
    if (value == "error") return translated("Error");
    if (value == "tool_call" || value == "tool_call_update") {
        if (toolId.empty()) return translated("Tool");
        auto pattern = translated("Tool#%s");
        const std::array arguments{core::translation::ngFormatArgument(toolId)};
        return formatted(pattern.view(), arguments);
    }
    if (value == "available_commands_update") return translated("Command list");
    if (value == "current_mode_update") return translated("Mode");
    return kind;
}

void AgentPanel::updateUsage(AgentJson update) {
    usagePayload = update;
    AgentJson source = update;
    if (update.is_object()) {
        auto usage = update.sourceObjectRef().if_contains("usage");
        if (usage && usage->is_object()) source = *usage;
    }
    auto input = firstScalar(source, {literal("inputTokens"), literal("input_tokens"),
        literal("promptTokens"), literal("prompt_tokens")});
    auto output = firstScalar(source, {literal("outputTokens"), literal("output_tokens"),
        literal("completionTokens"), literal("completion_tokens")});
    auto total = firstScalar(source, {literal("totalTokens"), literal("total_tokens")});
    auto cached = firstScalar(source, {literal("cachedInputTokens"), literal("cached_input_tokens"),
        literal("cacheReadInputTokens"), literal("cache_read_input_tokens")});
    Strings parts;
    if (!input.empty()) parts.append(concatenate({"in ", input.view()}));
    if (!output.empty()) parts.append(concatenate({"out ", output.view()}));
    if (!total.empty()) parts.append(concatenate({"total ", total.view()}));
    if (!cached.empty()) parts.append(concatenate({"cached ", cached.view()}));
    usageText = parts.empty() ? AgentString(compat::ngJsonStringify(update))
        : joinAgentStrings(parts, literal(", "));
}

void AgentPanel::handleSessionUpdate(AgentJson update) {
    auto kind = deriveUpdateKind(update);
    if (kind.view() == "usage_update") { updateUsage(update); return; }
    auto toolId = deriveToolId(update);
    auto role = deriveRoleFromKind(kind, toolId);
    AgentString status, title;
    if (update.is_object()) {
        auto value = update.sourceObjectRef().if_contains("status");
        if (value && value->is_string()) status = value->as_string();
        value = update.sourceObjectRef().if_contains("title");
        if (value && value->is_string()) title = value->as_string();
    }
    auto text = extractTextFromContent(update);
    bool isTool = kind.view() == "tool_call" || kind.view() == "tool_call_update";
    bool isPlan = kind.view() == "plan";
    if (isTool) {
        if (text.empty()) {
            auto message = update.sourceObjectRef().if_contains("message");
            if (message && message->is_string()) text = message->as_string();
        }
        if (text.empty()) text = literal("tool update");
        if (!title.empty()) text = concatenate({title.view(), " ", text.view()});
        if (!status.empty()) text = concatenate({text.view(), " (status: ", status.view(), ")"});
        if (!toolId.empty()) text = concatenate({"[", toolId.view(), "] ", text.view()});
        handleToolUpdate(role, title, text, update, kind.view() == "tool_call", status, toolId);
        return;
    }
    if (isPlan) { handlePlanUpdate(text, update, status); return; }
    if (text.empty()) text = fallbackText(update);
    appendStream(role, text, update, title, false, false);
}

AgentPanel::TrimView AgentPanel::trimBlankEdges(ConstStrings lines) {
    std::size_t start = 0, end = lines.size();
    while (start < end && stripAgentText(lines[start].view()).empty()) ++start;
    while (end > start && stripAgentText(lines[end - 1].view()).empty()) --end;
    return {start, end, start > 0 || end < lines.size()};
}

AgentString AgentPanel::shortToolTitle(AgentString title) {
    auto index = title.view().find('\n');
    if (index != std::string_view::npos) return concatenate({title.slice(0, index).view(), " ..."});
    return title;
}

AgentPanel::RoleKind AgentPanel::roleKind(LogBlock block) {
    auto role = block.role.view();
    if (role == "You") return RoleKind::You;
    if (role.starts_with("Tool")) return RoleKind::Tool;
    if (role == "Assistant") return RoleKind::AssistantFinal;
    if (role.starts_with("Assistant")) return RoleKind::AssistantThinking;
    if (role == "Error") return RoleKind::Error;
    return RoleKind::Other;
}

AgentJson AgentPanel::mergeJson(AgentJson destination, AgentJson source) {
    if (!destination.is_object() || !source.is_object()) return source;
    auto result = destination.sourceObjectRef().duplicate();
    for (const auto& member : source.sourceObjectRef()) {
        const auto key = member.name;
        const auto keyText = key.view();
        auto value = member.value();
        auto previous = result.if_contains(keyText);
        if (keyText == "contents" && previous) {
            auto old = *previous;
            Jsons merged;
            if (old.is_array() && value.is_array())
                merged = concatenateAgentJsonArrays({old.sourceArrayValue(), value.sourceArrayValue()});
            else if (old.is_array())
                merged = concatenateAgentJsonArrays({old.sourceArrayValue(), Jsons{value}});
            else if (value.is_array())
                merged = concatenateAgentJsonArrays({Jsons{old}, value.sourceArrayValue()});
            else merged = Jsons{old, value};
            result[key] = AgentJson(merged);
        } else if (keyText == "content" && previous) {
            auto old = *previous;
            AgentJson* oldText = nullptr;
            AgentJson* newText = nullptr;
            if (old.is_object() && value.is_object()) {
                oldText = old.sourceObjectRef().if_contains("text");
                if (oldText) newText = value.sourceObjectRef().if_contains("text");
            }
            if (oldText && newText && oldText->is_string() && newText->is_string()) {
                // The source shallow JSON copy deliberately shares this object.
                auto merged = old;
                const auto buildText = [&] {
                    const auto left = old.sourceIndexValue("text").as_string();
                    const auto right = value.sourceIndexValue("text").as_string();
                    const auto joined = concatenate({left.view(), right.view()});
                    return AgentJson(core::settings::SettingsString(joined));
                };
                // Bind the property receiver's actual AA cell before its argument.
                merged.sourceIndexAssign(literal("text"),
                    [&](AgentJson& text) { text = buildText(); }, buildText);
                result[key] = merged;
            } else {
                // Source content appends the entire value, even an array.
                const auto merged = old.is_array()
                    ? concatenateAgentJsonArrays({old.sourceArrayValue(), Jsons{value}})
                    : Jsons{old, value};
                result[key] = AgentJson(merged);
            }
        } else result[key] = value;
    }
    return AgentJson(result);
}

ImU32 AgentPanel::statusColor(AgentString status) {
    ImVec4 color;
    auto text = status.view();
    if (text == "in_progress") color = ImVec4(0, .4f, .8f, 1);
    else if (text == "completed") color = ImVec4(0, .75f, 0, 1);
    else if (text == "failed") color = ImVec4(.9f, 0, 0, 1);
    else if (text == "queued") color = ImVec4(.9f, .5f, 0, 1);
    else if (text == "cancelled") color = ImVec4(.5f, .5f, .5f, 1);
    else color = ImVec4(.6f, .6f, .6f, 1);
    return ImGui::GetColorU32(color);
}

void AgentPanel::showPermissionOptionTooltip(AgentString label, AgentString displayLabel) {
    if (label == displayLabel || !ImGui::IsItemHovered()) return;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 45);
    ImGui::TextUnformatted(label.data(), label.data() + label.size());
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

bool AgentPanel::permissionOptionButton(AgentString label, AgentString optionId) {
    auto displayLabel = ellipsizePermissionOptionLabel(label);
    auto buttonLabel = concatenate({displayLabel.view(), "##permission-option-", optionId.view()});
    const compat::SourceCString cLabel(buttonLabel);
    bool clicked = ImGui::Button(cLabel.data());
    showPermissionOptionTooltip(label, displayLabel);
    return clicked;
}

void AgentPanel::permissionOptionDisabledText(AgentString label) {
    auto displayLabel = ellipsizePermissionOptionLabel(label);
    const compat::SourceCString cLabel(displayLabel);
    ImGui::TextDisabled(cLabel.data());
    showPermissionOptionTooltip(label, displayLabel);
}

void AgentPanel::renderAssistantFinal(LogBlock& block, float width) {
    auto trim = trimBlankEdges(block.lines);
    auto view = block.lines.slice(trim.start, trim.end);
    if (trim.trimmed) {
        LineHeightCache temporary;
        renderClippedLines(view, width, block.ver, temporary);
    } else {
        renderClippedLines(view, width, block.ver, block.cache, false, {}, nullptr, block.dirtyFrom);
        block.dirtyFrom = SIZE_MAX;
    }
}

void AgentPanel::renderAssistantThinking(LogBlock& block, float width) {
    AgentString thinkingLabel;
    std::size_t thinkingFirstIdx = 0;
    bool found = false;
    for (std::size_t i = 0; i < block.lines.size(); ++i) {
        auto line = block.lines[i];
        if (!stripAgentText(line.view()).empty()) {
            thinkingLabel = line; thinkingFirstIdx = i; found = true; break;
        }
    }
    if (!found && !block.lines.empty()) thinkingLabel = block.lines[0];
    if (!found && thinkingLabel.empty()) thinkingLabel = block.role;
    auto label = concatenate({thinkingLabel.view(), "##blk",
        decimalText(reinterpret_cast<std::uintptr_t>(&block)).view()});
    const compat::SourceCString cLabel(label);
    if (ImGui::TreeNodeEx(cLabel.data(), ImGuiTreeNodeFlags_None)) {
        auto startIdx = thinkingFirstIdx + 1;
        if (startIdx < block.lines.size()) {
            auto trim = trimBlankEdges(block.lines.slice(startIdx, block.lines.size()));
            auto view = block.lines.slice(startIdx + trim.start, startIdx + trim.end);
            LineHeightCache temporary;
            renderClippedLines(view, width, block.ver, temporary);
        }
        ImGui::TreePop();
    }
}

void AgentPanel::renderError(LogBlock& block, float width, bool isLast) {
    ImVec4 red(.9f, .1f, .1f, 1);
    auto label = concatenate({block.role.view(), "##blk",
        decimalText(reinterpret_cast<std::uintptr_t>(&block)).view()});
    const compat::SourceCString cLabel(label);
    auto flags = isLast ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
    if (ImGui::TreeNodeEx(cLabel.data(), flags)) {
        auto trim = trimBlankEdges(block.lines);
        auto view = block.lines.slice(trim.start, trim.end);
        if (trim.trimmed) {
            LineHeightCache temporary;
            renderClippedLines(view, width, block.ver, temporary, false, {}, &red);
        } else {
            renderClippedLines(view, width, block.ver, block.cache, false, {}, &red, block.dirtyFrom);
            block.dirtyFrom = SIZE_MAX;
        }
        ImGui::TreePop();
    }
}

void AgentPanel::renderOther(LogBlock& block, float width, bool isLast) {
    auto label = concatenate({block.role.view(), "##blk",
        decimalText(reinterpret_cast<std::uintptr_t>(&block)).view()});
    const compat::SourceCString cLabel(label);
    auto flags = isLast ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
    if (ImGui::TreeNodeEx(cLabel.data(), flags)) {
        auto trim = trimBlankEdges(block.lines);
        auto view = block.lines.slice(trim.start, trim.end);
        if (trim.trimmed) {
            LineHeightCache temporary;
            renderClippedLines(view, width, block.ver, temporary);
        } else {
            renderClippedLines(view, width, block.ver, block.cache, false, {}, nullptr, block.dirtyFrom);
            block.dirtyFrom = SIZE_MAX;
        }
        ImGui::TreePop();
    }
}

void AgentPanel::renderTool(LogBlock& block, float width, bool isLast) {
    auto color = ImGui::ColorConvertU32ToFloat4(statusColor(block.status));
    const auto title = block.title.empty() ? block.role : shortToolTitle(block.title);
    auto labelVisible = concatenate({"tool: ", title.view()});
    auto labelId = concatenate({"##blk", decimalText(reinterpret_cast<std::uintptr_t>(&block)).view()});
    const compat::SourceCString id(labelId);
    auto flags = isLast ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None;
    bool open = ImGui::TreeNodeEx(id.data(), flags);
    float labelSpacing = ImGui::GetTreeNodeToLabelSpacing();
    ImGui::SameLine(0, labelSpacing);
    ImGui::TextColored(color, "\xef\x82\xbe");
    ImGui::SameLine(0, 4);
    const compat::SourceCString label(labelVisible);
    ImGui::TextUnformatted(label.data());
    if (open) {
        if (block.hasPayloadMerged) {
            Strings keys{literal("rawInput"), literal("rawOutput"), literal("content"), literal("contents")};
            bool shown = renderJsonFiltered(block.payloadMerged, width, keys);
            if (!shown) renderJsonTree(block.payloadMerged, {}, width, {}, 0, {}, true);
        }
        ImGui::TreePop();
    }
}

AgentString AgentPanel::ellipsizePermissionOptionLabel(AgentString label) {
    std::size_t count = 0, offset = 0;
    while (offset < label.size()) {
        auto next = offset;
        (void)decodeAgentForeachUtf8(label.view(), next);
        if (count >= permissionOptionLabelMaxChars) return concatenate({label.slice(0, offset).view(), "..."});
        ++count;
        offset = next;
    }
    return label;
}

float AgentPanel::measureConversationLines(ConstStrings lines, float wrapWidth,
    bool prefixFirstOnly, AgentString prefix) {
    auto& style = ImGui::GetStyle();
    const auto padPrefix = spaces(prefix.size());
    auto displayLine = [&](std::size_t i) -> AgentString {
        if (prefix.empty()) return agentElement(lines, i, 300);
        const auto leading = prefixFirstOnly && i != 0 ? std::string_view(padPrefix) : prefix.view();
        return concatenate({leading, agentElement(lines, i, prefixFirstOnly ? 302 : 304).view()});
    };
    float height = 0;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto sourceLine = displayLine(i);
        auto display = sourceLine.empty() ? AgentString{} : owned(copyBytes(sourceLine.view()));
        if (display.size() > kMaxDrawCharsPerLine)
            display = concatenate({display.slice(0, kMaxDrawCharsPerLine).view(), " ... (display truncated)"});
        const compat::SourceCString text(display);
        auto size = ImGui::CalcTextSize(text.data(), nullptr, true, wrapWidth);
        height += size.y + style.ItemSpacing.y;
    }
    return height;
}

void AgentPanel::renderClippedLines(ConstStrings lines, float wrapWidth, std::uint32_t dataVer,
    LineHeightCache& cache, bool prefixFirstOnly, AgentString prefix,
    const ImVec4* optColor, std::size_t dirtyFrom, std::size_t startIdx) {
    auto available = ImGui::GetContentRegionAvail();
    float wrapW = wrapWidth > 0 ? wrapWidth : available.x;
    float cursorX = ImGui::GetCursorPosX(), baseY = ImGui::GetCursorPosY();
    ImGui::PushTextWrapPos(cursorX + wrapW);
    auto& style = ImGui::GetStyle();
    const float top = ImGui::GetScrollY(), bottom = top + ImGui::GetWindowHeight();
    const float relTop = top - baseY, relBottom = bottom - baseY;
    const auto padding = spaces(prefix.size());
    auto displayLine = [&](std::size_t i) -> AgentString {
        if (prefix.empty()) return agentElement(lines, i, 175);
        const auto leading = prefixFirstOnly && i != 0 ? std::string_view(padding) : prefix.view();
        return concatenate({leading, agentElement(lines, i, prefixFirstOnly ? 177 : 179).view()});
    };
    auto drawableText = [&](AgentString line) {
        if (line.size() > kMaxDrawCharsPerLine)
            return concatenate({line.slice(0, kMaxDrawCharsPerLine).view(), " ... (display truncated)"});
        return line;
    };
    bool wrapChanged = cache.wrapW != wrapW;
    bool lengthChanged = cache.heights.size() != lines.size();
    bool versionChanged = cache.dataVer != dataVer;
    auto appendMeasured = [&](std::size_t i) {
        auto line = drawableText(displayLine(i));
        const compat::SourceCString text(line);
        auto size = ImGui::CalcTextSize(text.data(), nullptr, true, wrapW);
        float height = size.y + style.ItemSpacing.y;
        cache.heights.append(height);
        cache.lens.append(line.size());
        cache.totalHeight += height;
    };
    if (wrapChanged || lengthChanged) {
        cache.heights.resize(0); cache.lens.resize(0); cache.totalHeight = 0;
        for (std::size_t i = 0; i < lines.size(); ++i) appendMeasured(i);
        cache.wrapW = wrapW; cache.dataVer = dataVer;
    } else if (versionChanged) {
        // Preserve the source branches even though the outer length check
        // normally routes changed lengths through the full recomputation.
        if (lengthChanged && lines.size() < cache.heights.size()) {
            cache.heights.resize(0); cache.lens.resize(0); cache.totalHeight = 0;
            for (std::size_t i = 0; i < lines.size(); ++i) appendMeasured(i);
        } else {
            if (lengthChanged && lines.size() > cache.heights.size()) {
                auto start = cache.heights.size();
                cache.heights.resize(lines.size(), std::numeric_limits<float>::quiet_NaN());
                cache.lens.resize(lines.size());
                for (std::size_t i = start; i < lines.size(); ++i) {
                    auto line = drawableText(displayLine(i));
                    const compat::SourceCString text(line);
                    float height = ImGui::CalcTextSize(text.data(), nullptr, true, wrapW).y + style.ItemSpacing.y;
                    agentElement(cache.heights, i, 238) = height;
                    agentElement(cache.lens, i, 239) = line.size();
                    cache.totalHeight += height;
                }
            }
            if (dirtyFrom != SIZE_MAX && dirtyFrom < lines.size()) {
                for (std::size_t i = dirtyFrom; i < lines.size(); ++i) {
                    auto line = drawableText(displayLine(i));
                    const compat::SourceCString text(line);
                    float height = ImGui::CalcTextSize(text.data(), nullptr, true, wrapW).y + style.ItemSpacing.y;
                    float old = i < cache.heights.size() ? agentElement(cache.heights, i, 251) : 0;
                    agentElement(cache.heights, i, 252) = height;
                    agentElement(cache.lens, i, 253) = line.size();
                    cache.totalHeight += height - old;
                }
            }
        }
        cache.wrapW = wrapW; cache.dataVer = dataVer;
    }
    float prefixHeight = 0;
    if (startIdx > 0 && startIdx <= cache.heights.size())
        for (std::size_t i = 0; i < startIdx; ++i) prefixHeight += agentElement(cache.heights, i, 266);
    float y = 0;
    auto visibleLines = agentSlice(lines, startIdx, lines.size(), 269);
    for (std::size_t offset = 0; offset < visibleLines.size(); ++offset) {
        auto i = startIdx + offset;
        float height = agentElement(cache.heights, i, 271);
        bool visible = prefixHeight + y + height >= relTop && prefixHeight + y <= relBottom;
        if (visible) {
            ImGui::SetCursorPosY(baseY + prefixHeight + y);
            auto draw = drawableText(displayLine(i));
            if (draw.size() > kMaxDrawCharsPerLine)
                draw = concatenate({draw.slice(0, kMaxDrawCharsPerLine).view(), " ... (display truncated)"});
            const compat::SourceCString text(draw);
            if (optColor) ImGui::TextColored(*optColor, text.data());
            else ImGui::TextUnformatted(text.data());
        }
        y += height;
        if (prefixHeight + y > relBottom && !visible) break;
    }
    ImGui::SetCursorPosY(baseY + cache.totalHeight);
    ImGui::PopTextWrapPos();
}

void AgentPanel::renderYou(LogBlock& block, float width, ImVec4 color) {
    auto trim = trimBlankEdges(block.lines);
    auto view = block.lines.slice(trim.start, trim.end);
    auto& style = ImGui::GetStyle();
    const float padX = 10, padY = 7;
    float contentW = width - style.ScrollbarSize - padX * 2 - style.ItemSpacing.x;
    if (contentW < 48) contentW = 48;
    auto prefix = literal("\xef\x83\x93: ");
    float contentH = measureConversationLines(view, contentW, true, prefix);
    float bubbleW = contentW + padX * 2, bubbleH = contentH + padY * 2;
    auto start = ImGui::GetCursorPos();
    auto screen = ImGui::GetCursorScreenPos();
    auto drawList = ImGui::GetWindowDrawList();
    ImVec4 background(.12f, .32f, .17f, .38f), border(.42f, .72f, .40f, .80f);
    auto end = ImVec2(screen.x + bubbleW, screen.y + bubbleH);
    drawList->AddRectFilled(screen, end, ImGui::GetColorU32(background), 6);
    drawList->AddRect(screen, end, ImGui::GetColorU32(border), 6);
    ImGui::SetCursorPos(ImVec2(start.x + padX, start.y + padY));
    if (trim.trimmed) {
        LineHeightCache temporary;
        renderClippedLines(view, contentW, block.ver, temporary, true, prefix, &color);
    } else {
        renderClippedLines(view, contentW, block.ver, block.cache, true, prefix, &color, block.dirtyFrom);
        block.dirtyFrom = SIZE_MAX;
    }
    ImGui::SetCursorPos(ImVec2(start.x, start.y + bubbleH + style.ItemSpacing.y));
}

void AgentPanel::renderPermissionRequest() {
    if (permQueue.empty()) return;
    auto request = agentElement(permQueue, 0, 1265);
    auto reason = request.reason.empty() ? literal("(no reason)") : request.reason;
    auto options = permissionOptions(request.params);
    auto pattern = translated("Permission requested: %s");
    const std::array arguments{core::translation::ngFormatArgument(reason)};
    const compat::SourceCString text(formatted(pattern.view(), arguments));
    ImGui::Text(text.data());
    if (request.id.empty()) {
        const compat::SourceCString warning(translated("Cannot respond: missing request id"));
        ImGui::TextColored(ImVec4(1, .3f, .3f, 1), warning.data());
    }
    if (!options.empty()) {
        for (std::size_t i = 0; i < options.size(); ++i) {
            const auto option = options[i];
            auto optionId = permissionOptionId(option);
            auto label = permissionOptionLabel(option);
            if (optionId.empty()) { permissionOptionDisabledText(label); continue; }
            if (i > 0) ImGui::SameLine();
            if (permissionOptionButton(label, optionId)) {
                enqueueCommand(concatenate({"perm:", request.id.view(), ":", optionId.view()}));
                permQueue = agentSlice(permQueue, 1, permQueue.size(), 1283);
                promptPending = true;
            }
        }
    } else {
        const compat::SourceCString allow(translated("Allow"));
        if (ImGui::Button(allow.data())) {
            enqueueCommand(concatenate({"perm:", request.id.view(), ":allow-once"}));
            permQueue = agentSlice(permQueue, 1, permQueue.size(), 1290);
            promptPending = true;
        }
        ImGui::SameLine();
        const compat::SourceCString deny(translated("Deny"));
        if (ImGui::Button(deny.data())) {
            enqueueCommand(concatenate({"perm:", request.id.view()}));
            permQueue = agentSlice(permQueue, 1, permQueue.size(), 1296);
            promptPending = true;
        }
    }
}

void AgentPanel::onUpdate() {
    if (!ImGui::GetCurrentContext()) return;
    // ImGui's native fixed-size Colors array is present whenever its real
    // context exists; no surrogate style or context is manufactured here.
    auto& style = ImGui::GetStyle();
    auto statusPattern = translated("Status: %s");
    const std::array statusArgs{core::translation::ngFormatArgument(statusText)};
    const compat::SourceCString status(formatted(statusPattern.view(), statusArgs));
    ImGui::TextUnformatted(status.data());
    if (!usageText.empty()) {
        ImGui::SameLine();
        const compat::SourceCString usage(concatenate({"Usage: ", usageText.view()}));
        ImGui::TextDisabled(usage.data());
    }
    ImGui::SameLine();
    auto available = ImGui::GetContentRegionAvail();
    float buttonSize = 24, totalWidth = style.ItemSpacing.x + buttonSize;
    float offsetX = available.x > totalWidth ? available.x - totalWidth : 0;
    ImGui::Dummy(ImVec2(offsetX, 1));
    ImGui::SameLine();
    if (widgets::ngButtonColored("\xee\x89\xa6", ImVec2(buttonSize, buttonSize))) {
        stopAgent(); statusText = literal("Initializing..."); clearState(); startAgent();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        const compat::SourceCString label(translated("New session"));
        ImGui::TextUnformatted(label.data());
        ImGui::EndTooltip();
    }
    auto now = Clock::now();
    if (!workerRunning && !initInFlight && now - lastStartAttempt > std::chrono::seconds(1)) startAgent();
    ImGui::Separator();
    drainResults();
    auto logAvailable = ImGui::GetContentRegionAvail();
    float inputHeight = ImGui::GetFrameHeightWithSpacing() * 2 + ImGui::GetStyle().ItemSpacing.y * 2
        + ImGui::GetFrameHeightWithSpacing();
    float planHeight = 0, permissionHeight = 0;
    if (hasPlan && !planView.lines.empty())
        planHeight = ImGui::GetTextLineHeightWithSpacing() * planView.lines.size() + ImGui::GetStyle().ItemSpacing.y * 2;
    if (!permQueue.empty())
        permissionHeight = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetFrameHeightWithSpacing()
            + ImGui::GetStyle().ItemSpacing.y * 3;
    ImVec2 logSize(logAvailable.x, logAvailable.y - inputHeight - planHeight - permissionHeight);
    if (logSize.y < 160) logSize.y = logAvailable.y * .65f;
    flushPending();
    if (ImGui::BeginChild("AgentConversation", logSize, true, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        auto& conversationStyle = ImGui::GetStyle();
        auto blocks = convoBlocks;
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            auto& block = blocks[i];
            bool last = i == convoBlocks.size() - 1;
            switch (roleKind(block)) {
            case RoleKind::You: renderYou(block, logSize.x, conversationStyle.Colors[0]); break;
            case RoleKind::Tool: renderTool(block, logSize.x, last); break;
            case RoleKind::AssistantThinking: renderAssistantThinking(block, logSize.x); break;
            case RoleKind::AssistantFinal: renderAssistantFinal(block, logSize.x); break;
            case RoleKind::Error: renderError(block, logSize.x, last); break;
            case RoleKind::Other: renderOther(block, logSize.x, last); break;
            }
        }
    }
    ImGui::EndChild();
    convoContentAdded = false;
    if (hasPlan && !planView.lines.empty()) {
        ImGui::Separator();
        const compat::SourceCString label(translated("Plan"));
        ImGui::Text(label.data());
        auto color = ImGui::ColorConvertU32ToFloat4(statusColor(planView.status.empty() ? literal("in_progress") : planView.status));
        auto lines = planView.lines;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const std::array args{core::translation::ngFormatArgument(i + 1), core::translation::ngFormatArgument(lines[i])};
            const compat::SourceCString line(formatted("%d. %s", args));
            ImGui::TextColored(color, line.data());
        }
        ImGui::Separator();
    }
    ImGui::Separator();
    renderPermissionRequest();
    if (!permQueue.empty()) ImGui::Separator();
    const compat::SourceCString messageLabel(translated("User message:"));
    ImGui::Text(messageLabel.data());
    float sendWidth = 80, textWidth = logAvailable.x - sendWidth - ImGui::GetStyle().ItemSpacing.x;
    if (textWidth < 160) textWidth = logAvailable.x * .75f;
    bool awaitingAgent = promptPending || !permQueue.empty();
    const std::array inputArgs{core::translation::ngFormatArgument(userTextInputSerial)};
    auto inputId = formatted("##acp_user_text_%s", inputArgs);
    bool submit = widgets::ngInputText(inputId, textWidth, userText,
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_NoUndoRedo);
    if (submit && !userText.empty() && !awaitingAgent) enqueueUserText();
    ImGui::SameLine();
    const compat::SourceCString buttonLabel(awaitingAgent ? translated("\xee\xa2\x88") : translated("\xee\x85\xa3"));
    if (widgets::ngButtonColored(buttonLabel.data(), ImVec2(sendWidth, 0))) {
        if (awaitingAgent) enqueueCommand(literal("cancel"));
        else if (!userText.empty()) enqueueUserText();
    }
}

void AgentPanel::workerLoop(Strings command, AgentString workingDirectory) {
    using api::acp::client::ACPClient;
    std::shared_ptr<ACPClient> client;
    try {
        client = ACPClient::ngCreate(command, workingDirectory.empty() ? AgentString{} : workingDirectory);
        const auto self = shared_from_this();
        client->setLogger([self](AgentString message) { self->pushResult(literal("log"), false, message); });
        client->setCancelCheck([self] { return !self->workerRunning; });
        client->setSendMode(literal("line"));
        const auto pid = client->getPid().toString();
        pushResult(literal("log"), false, concatenate({"spawned pid: ", pid}));
    } catch (const compat::ThrowableException& error) {
        pushResult(literal("init"), true, concatenate({"spawn failed: ", error.msg.view()}));
        return;
    }
    while (workerRunning) {
        lockQueueMutex(qMutex);
        auto commands = cmdQueue.duplicate();
        cmdQueue.resize(0);
        compat::ngRequireObject(qMutex)->unlock();
        for (auto command : commands) {
            auto value = command.view();
            if (value == "init") {
                client->initializeAsync();
                initInFlight = true;
                initStart = Clock::now();
            } else if (value == "ping") {
                try { client->ping(); pushResult(literal("ping"), false, literal("")); }
                catch (const compat::ThrowableException& error) { pushResult(literal("ping"), true, error.msg); }
            } else if (value.starts_with("msg:")) {
                auto text = command.slice(4, command.size());
                try {
                    appendStream(literal("You"), text);
                    client->sendPrompt(text);
                    pushResult(literal("log"), false, concatenate({"prompt: ", text.view()}));
                } catch (const compat::ThrowableException& error) {
                    pushResult(literal("log"), true, concatenate({"send failed: ", error.msg.view()}));
                }
            } else if (value == "cancel") {
                try {
                    client->cancelPrompt();
                    pushResult(literal("promptDone"), false, literal("cancel sent"));
                } catch (const compat::ThrowableException& error) {
                    pushResult(literal("log"), true, concatenate({"cancel failed: ", error.msg.view()}));
                }
            } else if (value.starts_with("perm:")) {
                Strings parts;
                std::size_t start = 0;
                for (std::size_t i = 0; i < command.size(); ++i) {
                    if (value[i] == ':') { parts.append(command.slice(start, i)); start = i + 1; }
                }
                parts.append(command.slice(start, command.size()));
                if (parts.size() >= 3) {
                    auto id = parts[1], option = parts[2];
                    try {
                        client->sendPermissionResponse(id, option, false);
                        pushResult(literal("log"), false, concatenate({"permission selected option=",
                            option.view(), " id=", id.view()}));
                    } catch (const compat::ThrowableException& error) {
                        pushResult(literal("log"), true, concatenate({"permission response failed: ", error.msg.view()}));
                    }
                } else if (parts.size() >= 2) {
                    auto id = parts[1];
                    try {
                        client->sendPermissionResponse(id, literal(""), true);
                        pushResult(literal("log"), false, concatenate({"permission cancelled id=", id.view()}));
                    } catch (const compat::ThrowableException& error) {
                        pushResult(literal("log"), true, concatenate({"permission response failed: ", error.msg.view()}));
                    }
                }
            }
        }
        for (auto result : client->takeInbound()) {
            if (initInFlight) {
                initInFlight = false;
                if (!result.error.empty()) pushResult(literal("init"), true, result.error);
                else pushResult(literal("init"), false, compat::ngJsonStringify(result.result));
                continue;
            }
            if (!result.error.empty()) { pushResult(literal("log"), true, result.error); continue; }
            handleInbound(result.result);
        }
        if (initInFlight && client->pollInitialize()) {
            initInFlight = false;
            if (!client->getInitError().empty()) pushResult(literal("init"), true, client->getInitError());
            else pushResult(literal("init"), false, compat::ngJsonStringify(client->getInitResult()));
        }
        lockQueueMutex(qMutex);
        if (cmdQueue.empty() && workerRunning) {
            const auto condition = compat::ngRequireObject(qCond);
            condition->wait(nicxlive::core::d_runtime::SourceDuration(std::chrono::milliseconds(100)));
        }
        compat::ngRequireObject(qMutex)->unlock();
    }
    if (client) client->close();
}

void AgentPanel::stopWorker() {
    if (worker) {
        workerRunning = false;
        signalWake();
        const auto thread = compat::ngRequireObject(worker);
        try {
            if (thread->thread.joinable()) {
                thread->thread.join();
            }
            else if (!thread->failure)
                throw compat::ThreadException(compat::RetainedString::fromStaticLiteral("Error joining thread"));
        }
        catch (const std::system_error&) {
            throw compat::ThreadException(compat::RetainedString::fromStaticLiteral("Error joining thread"));
        }
        // A failed entry is retained by the real thread owner and rethrown by
        // join. In that case the source's later worker=null is not evaluated.
        // https://dlang.org/phobos/core_thread_threadbase.html#ThreadBase.join
        if (thread->failure) std::rethrow_exception(thread->failure);
        worker.reset();
    }
}

void AgentPanel::pingAgent() { enqueueCommand(literal("ping")); }

void AgentPanel::stopAgent() {
    stopWorker();
    statusText = literal("Stopped");
    clearState();
}

AgentString AgentPanel::safeDecode(compat::ConstSlice<std::uint8_t> bytes) {
    // std.utf.decode leaves its index unchanged on malformed input:
    // https://dlang.org/phobos/std_utf.html#decode
    if (bytes.empty()) return {};
    std::string result;
    std::size_t index = 0;
    while (index < bytes.size()) {
        try {
            auto remaining = bytes.slice(index, bytes.size());
            compat::phobos::detail::UnicodeCursor<compat::ConstSlice<std::uint8_t>, char> cursor(remaining);
            const auto scalar = cursor.front();
            const std::size_t width = scalar < 0x80 ? 1 : scalar < 0x800 ? 2 : scalar < 0x10000 ? 3 : 4;
            index += width;
            // Strictly validated UTF-8 has a unique encoding for this scalar.
            // Snapshot before the output allocation can invoke a memory handler.
            std::array<char, 4> encoded;
            std::copy_n(reinterpret_cast<const char*>(remaining.data()), width, encoded.data());
            appendBytes(result, std::string_view(encoded.data(), width));
        } catch (const compat::UTFException&) {
            appendBytes(result, "\xef\xbf\xbd");
            ++index;
        }
    }
    return owned(std::move(result));
}

void AgentPanel::appendStream(AgentString role, AgentString text, AgentJson payload,
    AgentString title, bool payloadPresent, bool forceNewLine) {
    if (hasPendingRole && !(role == pendingRole)) flushPending();
    pendingRole = role;
    hasPendingRole = true;
    // Actual byte-array concatenation: compute the full extent, allocate, then
    // copy both inputs. Digital Mars 2000-2019, BSL-1.0, as reproduced in
    // core.internal.array.concatenation; license text in phobos-agent-path.txt.
    const auto previous = pendingBuf;
    const auto incoming = text.view();
    const auto count = previous.size() + incoming.size();
    auto combined = [&] {
        if (!count) return compat::Slice<std::uint8_t>{};
        try {
            return compat::Slice<std::uint8_t>(std::vector<std::uint8_t>(count));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::length_error&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
    }();
    for (std::size_t i = 0; i < previous.size(); ++i) combined[i] = previous[i];
    for (std::size_t i = 0; i < incoming.size(); ++i)
        combined[previous.size() + i] = static_cast<std::uint8_t>(incoming[i]);
    std::size_t start = 0;
    bool startNewLine = pendingNewline;
    pendingNewline = false;
    for (;;) {
        std::size_t index = start;
        while (index < combined.size() && combined[index] != '\n') ++index;
        if (index == combined.size()) {
            auto remaining = combined.slice(start, combined.size());
            if (!remaining.empty()) {
                auto decoded = safeDecode(remaining);
                bool newLine = forceNewLine || startNewLine || start > 0 || pendingNewline;
                if (newLine) pushChatLine(role, decoded, payload, title, payloadPresent);
                else appendToLastLine(role, decoded, payload, payloadPresent);
            }
            pendingBuf = {};
            hasPendingRole = false;
            return;
        }
        auto decoded = safeDecode(combined.slice(start, index));
        if (!startNewLine && start == 0 && !convoBlocks.empty()
            && convoBlocks[convoBlocks.size() - 1].role == role
            && !convoBlocks[convoBlocks.size() - 1].lines.empty())
            appendToLastLine(role, decoded, payload, payloadPresent);
        else pushChatLine(role, decoded, payload, title, payloadPresent);
        start = index + 1;
        startNewLine = false;
        if (start == combined.size()) {
            pendingNewline = true;
            pendingBuf = {};
            hasPendingRole = true;
            return;
        }
    }
}

void AgentPanel::flushPending() {
    if (!hasPendingRole || (pendingBuf.empty() && !pendingNewline)) {
        hasPendingRole = false; pendingBuf = {}; pendingNewline = false; return;
    }
    if (pendingNewline && pendingBuf.empty()) {
        pendingNewline = false; hasPendingRole = false; return;
    }
    auto decoded = safeDecode(pendingBuf);
    if (pendingNewline) pushChatLine(pendingRole, decoded);
    else appendToLastLine(pendingRole, decoded);
    pendingBuf = {};
    hasPendingRole = false;
    pendingNewline = false;
}

void AgentPanel::showPermissionToolCall(AgentJson params, AgentString permissionId, AgentString reason) {
    if (!params.is_object()) return;
    if (!params.sourceObjectRef().if_contains("toolCall")) return;
    auto call = params.sourceIndexValue("toolCall");
    if (!call.is_object()) return;
    auto update = call.sourceObjectRef().duplicate();
    update[literal("sessionUpdate")] = AgentJson(core::settings::SettingsString(literal("tool_call")));
    auto status = update.if_contains("status");
    if (!status || !status->is_string())
        update[literal("status")] = AgentJson(core::settings::SettingsString(literal("pending")));
    auto id = update.if_contains("toolCallId");
    if (!id || !id->is_string() || id->as_string().empty())
        update[literal("toolCallId")] = AgentJson(permissionId.empty()
            ? core::settings::SettingsString(literal("permission"))
            : core::settings::SettingsString(concatenate({"permission:", permissionId.view()})));
    auto title = update.if_contains("title");
    if (!title || !title->is_string() || title->as_string().empty()) {
        auto name = reason.empty() ? translated("Permission requested") : reason;
        update[literal("title")] = AgentJson(core::settings::SettingsString(name));
    }
    handleSessionUpdate(AgentJson(update));
}

void AgentPanel::handleInbound(AgentJson object) {
    if (object.is_object() && object.sourceObjectRef().if_contains("method")) {
        auto method = object.sourceIndexValue("method").as_string();
        auto m = method.view();
        bool permission = m == "session/request_permission" || m == "request_permission"
            || m == "request_permissions" || m == "request_prermissions";
        if (permission) {
            AgentString id;
            if (auto value = object.sourceObjectRef().if_contains("id"))
                id = compat::ngJsonStringify(*value);
            auto paramsValue = object.sourceObjectRef().if_contains("params");
            if (id.empty() && paramsValue && paramsValue->is_object()) {
                for (auto key : {"id", "requestId", "request_id"}) {
                    if (auto value = paramsValue->sourceObjectRef().if_contains(key)) {
                        id = compat::ngJsonStringify(*value); break;
                    }
                }
            }
            AgentString reason;
            if (paramsValue && paramsValue->is_object()) {
                auto value = paramsValue->sourceObjectRef().if_contains("reason");
                if (value && value->is_string()) reason = value->as_string();
            }
            auto params = object.sourceIndexValue("params");
            showPermissionToolCall(params, id, reason);
            permQueue.append(PermissionRequest{id, reason, params, false, false});
            permChanged = true;
            pushResult(literal("log"), false, compat::ngJsonStringify(object));
            return;
        }
        if (m == "session/update") {
            auto params = object.sourceIndexValue("params");
            if (auto update = params.sourceObjectRef().if_contains("update")) handleSessionUpdate(*update);
        }
        pushResult(literal("log"), false, compat::ngJsonStringify(object));
        return;
    }
    if (object.is_object() && object.sourceObjectRef().if_contains("id")) {
        flushPending();
        promptPending = false;
        pushResult(literal("log"), false, compat::ngJsonStringify(object));
        return;
    }
    pushResult(literal("log"), false, compat::ngJsonStringify(object));
}

AgentString AgentPanel::jsonScalar(AgentJson value) {
    if (value.is_string()) {
        const auto text = value.as_string();
        return concatenate({"\"", text.view(), "\""});
    }
    if (value.is_null()) return literal("null");
    if (value.is_bool()) return value.as_bool() ? literal("true") : literal("false");
    if (value.is_int64()) return decimalText(value.as_int64());
    if (value.is_uint64()) return decimalText(value.as_uint64());
    if (value.is_double()) {
        const std::array arguments{core::translation::ngFormatArgument(value.as_double())};
        return formatted("%s", arguments);
    }
    return compat::ngJsonStringify(value);
}

bool AgentPanel::renderJsonFiltered(AgentJson value, float wrapWidth, Strings focusKeys) {
    bool shown = false;
    if (value.is_object()) {
        std::size_t index = 0;
        for (const auto& member : value.sourceObjectRef()) {
            const auto key = member.name;
            const auto child = member.value();
            if (renderJsonTree(child, key, wrapWidth, {}, index++, focusKeys)) shown = true;
        }
    } else if (value.is_array()) {
        const auto children = value.sourceArrayValue();
        for (std::size_t i = 0; i < children.size(); ++i) {
            const auto child = children[i];
            if (renderJsonTree(child, concatenate({"[", decimalText(i).view(), "]"}),
                wrapWidth, {}, i, focusKeys)) shown = true;
        }
    }
    return shown;
}

bool AgentPanel::renderJsonTree(AgentJson value, AgentString label, float wrapWidth,
    AgentString path, std::size_t siblingIdx, Strings focusKeys, bool allowAllBelow) {
    const auto nextLabel = [](AgentString value, AgentString parent) {
        const auto hashedPath = parent.empty() ? value
            : concatenate({parent.view(), "/", value.view()});
        // CRC-32/ISO-HDLC, followed by the original four little-endian digest
        // bytes rendered as a byte-array string. This is an ID, not std::hash.
        // https://dlang.org/phobos/std_digest_crc.html#crc32Of
        // https://dlang.org/phobos/std_conv.html#to
        std::uint32_t crc = 0xffffffffu;
        for (unsigned char byte : hashedPath.view()) {
            crc ^= byte;
            for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
        }
        crc = ~crc;
        const auto digest = textBacking([&] {
            std::string bytes = "[";
            for (unsigned i = 0; i < 4; ++i) {
                if (i) bytes += ", ";
                bytes += std::to_string((crc >> (8 * i)) & 0xffu);
            }
            bytes += "]";
            return bytes;
        });
        const auto visible = value.empty() ? literal("(root)") : value;
        return concatenate({visible.view(), "##json", digest});
    };
    auto id = nextLabel(concatenate({label.view(), "#", decimalText(siblingIdx).view()}), path);
    auto nextPath = path.empty() ? concatenate({label.view(), "#", decimalText(siblingIdx).view()})
        : concatenate({path.view(), "/", label.view(), "#", decimalText(siblingIdx).view()});
    bool hasFocus = !focusKeys.isNull() && !focusKeys.empty();
    auto keyMatch = [&](std::string_view key) {
        if (!hasFocus) return true;
        for (auto focus : focusKeys) if (key == focus.view()) return true;
        return false;
    };
    const auto containsKey = [&](auto&& self, AgentJson candidate) -> bool {
        if (candidate.is_object()) {
            for (const auto& member : candidate.sourceObjectRef()) {
                const auto key = member.name;
                const auto child = member.value();
                if (keyMatch(key.view()) || self(self, child)) return true;
            }
        } else if (candidate.is_array()) {
            for (auto child : candidate.sourceArrayValue()) if (self(self, child)) return true;
        }
        return false;
    };
    if (value.is_object()) {
        bool matchedHere = keyMatch(label.view());
        if (!allowAllBelow && hasFocus && !matchedHere && !containsKey(containsKey, value)) return false;
        const compat::SourceCString cId(id);
        if (ImGui::TreeNodeEx(cId.data(), ImGuiTreeNodeFlags_None)) {
            std::size_t index = 0;
            for (const auto& member : value.sourceObjectRef()) {
                const auto key = member.name;
                const auto child = member.value();
                bool childMatched = keyMatch(key.view());
                bool childAllow = allowAllBelow || matchedHere || childMatched;
                if (allowAllBelow || matchedHere || childMatched || !hasFocus || containsKey(containsKey, child))
                    renderJsonTree(child, key, wrapWidth, nextPath, index, focusKeys, childAllow);
                ++index;
            }
            ImGui::TreePop();
        }
        return true;
    }
    if (value.is_array()) {
        if (!allowAllBelow && hasFocus && !containsKey(containsKey, value)) return false;
        const compat::SourceCString cId(id);
        if (ImGui::TreeNodeEx(cId.data(), ImGuiTreeNodeFlags_None)) {
            const auto children = value.sourceArrayValue();
            for (std::size_t i = 0; i < children.size(); ++i) {
                auto child = children[i];
                if (allowAllBelow || !hasFocus || containsKey(containsKey, child))
                    renderJsonTree(child, concatenate({"[", decimalText(i).view(), "]"}), wrapWidth,
                        nextPath, i, focusKeys, allowAllBelow);
            }
            ImGui::TreePop();
        }
        return true;
    }
    if (allowAllBelow || !hasFocus || keyMatch(label.view())) {
        auto scalar = jsonScalar(value);
        auto line = label.empty() ? scalar : concatenate({label.view(), ": ", scalar.view()});
        LineHeightCache temporary;
        renderClippedLines(Strings{line}, wrapWidth, 0, temporary);
        return true;
    }
    return false;
}

AgentPanel::AgentPanel()
    : Panel(compat::RetainedString::fromStaticLiteral("Agent"),
        translated("Agent").retainedString(), false) {}

AgentPanel::~AgentPanel() { stopWorker(); }

void AgentPanel::prepareSourceClassInfo() {
    Panel::prepareSourceClassInfo();
    static const bool registered = [] {
        (void)nicxlive::core::d_runtime::ngObjectTemplateArgumentName<AgentPanel>();
        nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(AgentPanel),
            compat::RetainedString::fromStaticLiteral(
                nicxlive::core::d_runtime::SourceObjectTypeNames<AgentPanel>::className));
        return true;
    }();
    (void)registered;
}

void AgentPanel::onInit() {
    qMutex = allocateAgentObject<nicxlive::core::d_runtime::SourceMutex>();
    qCond = allocateAgentObject<nicxlive::core::d_runtime::SourceCondition>(qMutex);
    lastStartAttempt = Clock::now();
}

void AgentPanel::log(AgentString message) { appendAgentLog(shrinkLineLog(message)); }

AgentString AgentPanel::getAgentLogPath() {
    if (agentLogPath.empty()) {
        auto directory = temporaryDirectory();
        agentLogPath = buildAgentPath(directory, literal("nijigenerate-agent.log"));
    }
    return agentLogPath;
}

void AgentPanel::appendAgentLog(AgentString line) {
    auto name = getAgentLogPath();
    const auto bytes = concatenate({line.view(), "\n"});
    // std.file.append delegates to writeImpl(..., true), std/file.d789-898.
    // D Language Foundation 2007-2011, BSL-1.0; phobos-agent-path.txt.
    const auto fail = [&](std::size_t sourceLine) {
#ifdef _WIN32
        const auto code = static_cast<std::uint32_t>(GetLastError());
#else
        const auto code = static_cast<std::uint32_t>(errno);
#endif
        compat::FileException error(name.retainedString(), code);
        error.file = compat::RetainedString::fromStaticLiteral("std/file.d");
        error.line = sourceLine;
        throw error;
    };
#ifdef _WIN32
    const compat::phobos::TempCStringBuffer<wchar_t> path(name.view());
    const auto handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (handle == INVALID_HANDLE_VALUE) fail(873);
    if (SetFilePointer(handle, 0, nullptr, FILE_END) == INVALID_SET_FILE_POINTER)
        fail(874);
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        auto count = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - offset, std::size_t{1} << 30));
        DWORD actual;
        WriteFile(handle, bytes.data() + offset, count, &actual, nullptr);
        if (actual != count) break;
        offset += actual;
    }
    // The source short-circuits close after seek/partial-write failure. It has
    // no scope(exit) close here; adding one would change its failure behavior.
    if (offset != bytes.size() || !CloseHandle(handle))
        fail(898);
#else
    const compat::phobos::TempCStringBuffer<char> path(name.view());
    const auto descriptor = ::open(path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0666);
    if (descriptor == -1) fail(840);
    try {
        std::size_t sum = 0;
        while (sum != bytes.size()) {
            const auto count = (std::min<std::size_t>)(bytes.size() - sum, std::size_t{1} << 30);
            const auto written = ::write(descriptor, bytes.data() + sum, count);
            if (written != static_cast<decltype(written)>(count)) break;
            sum += static_cast<std::size_t>(written);
        }
        if (sum != bytes.size()) fail(854);
    } catch (...) {
        (void)::close(descriptor);
        throw;
    }
    if (::close(descriptor) != 0) fail(856);
#endif
}

void AgentPanel::clearAgentLog() {
    auto name = getAgentLogPath();
    compat::phobos::ngFileWrite(name, std::string_view{});
}

void AgentPanel::startAgent() {
    stopAgent();
    convoBlocks.resize(0);
    pendingBuf = {};
    pendingNewline = false;
    hasPendingRole = false;
    auto command = parseCommandLine(core::settings::incSettingsGet<core::settings::SettingsString>(literal("ACP.Command")));
    AgentString workingDirectory = core::settings::incSettingsGet<core::settings::SettingsString>(literal("ACP.Workingdir"));
    if (command.empty()) {
        statusText = literal("Command is empty");
        log(statusText);
        lastStartAttempt = Clock::now();
        return;
    }
    if (!absolutePath(command[0].view())) {
        auto base = workingDirectory.empty() ? currentDirectory() : workingDirectory;
        command[0] = buildAgentPath(base, command[0]);
    }
    if (!compat::phobos::ngFileExists(command[0].view())) {
        statusText = literal("Executable not found");
        log(concatenate({"not found: ", command[0].view()}));
        lastStartAttempt = Clock::now();
        return;
    }
    try {
        workerRunning = true;
        auto commandCopy = command.duplicate();
        auto directoryCopy = workingDirectory;
        const auto self = shared_from_this();
        auto thread = allocateAgentObject<NativeWorker>();
        worker = thread;
        try {
            thread->thread = std::thread([self, thread, commandCopy, directoryCopy] {
                try {
                    core::thread_context::ManagedThreadScope managed;
                    utils::crashdump::installNativeCrashDumpThreadHandler();
                    self->workerLoop(commandCopy, directoryCopy);
                } catch (...) {
                    thread->failure = std::current_exception();
                }
            });
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        } catch (const std::system_error&) {
            compat::ngOnThreadError(compat::RetainedString::fromStaticLiteral("Error creating thread"));
        }
        enqueueCommand(literal("init"));
        initInFlight = true;
        statusText = literal("Initializing...");
        log(literal("worker started"));
        autoStartDone = true;
    } catch (const compat::ThrowableException& error) {
        statusText = literal("Start failed");
        const auto text = error.toString();
        log(concatenate({"error: ", text.view()}));
        workerRunning = false;
        lastStartAttempt = Clock::now();
    }
}

} // namespace nijigenerate::panels::agent
