#pragma once
// Required current Phobos std.path/std.file/std.json behavior for this source.
// std.path copyright (c) 2000-2014 Lars Tandle Kyllingstad, Walter Bright,
// Grzegorz Adam Hankiewicz, Thomas Kuehne and Andrei Alexandrescu.
// std.file copyright The D Language Foundation 2007-2011;
// std.json copyright Jeremie Pelletier 2008-2009, D Language Foundation 2007-.
// Boost Software License 1.0: port/licenses/phobos-crash-path.txt,
// phobos-psd-file.txt and phobos-rig-json.txt.
#include "../framework.hpp"
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/exception_message.hpp>
#include <nijigenerate/core/settings_json_string.hpp>
#include <nijigenerate/compat/phobos_file_exception.hpp>
#include <nijigenerate/compat/out_of_memory_error.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include <compat/object_metadata.hpp>
#include <filesystem>
#include <new>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <charconv>

namespace nijigenerate::autorig::framework::detail {
inline Text copiedMessage(std::string_view message) {
    try {
        return Text(message);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class T, std::size_t N> void registerClass(const char (&name)[N]) {
    static const bool registered = [&] {
        try {
            nicxlive::core::d_runtime::ngRegisterObjectClassName(typeid(T), Text::fromStaticLiteral(name));
        } catch (const std::bad_alloc&) {
            throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
        }
        return true;
    }();
    (void)registered;
}
// Inherited object.Object defaults, Digital Mars 2000-2011, BSL-1.0.
// Full notice: nicxlive/core/licenses/druntime-associative-array.txt.
template<class T> std::size_t objectHash(T* object) noexcept {
    const auto address = reinterpret_cast<std::size_t>(dynamic_cast<const void*>(object));
    return address ^ (address >> 4);
}
template<class T> bool objectEquals(T* object, const std::shared_ptr<T>& other) {
    return other && dynamic_cast<const void*>(object) == dynamic_cast<const void*>(other.get());
}
[[noreturn]] inline int objectCompare(Text name) {
    Text message;
    try {
        std::string bytes("need opCmp for class ");
        bytes += name.view();
        message = copiedMessage(bytes);
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    throw compat::ThrowableException(message, Text::fromStaticLiteral("object.d"), 195);
}
inline Text str(compat::RetainedStringSlice value) { return value.retainedString(); }
template<class Message> inline Text evaluatedMessage(Message&& message) {
    Text copy;
    try {
        const Text evaluated(message());
        copy = copiedMessage(evaluated.view());
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
    return copy;
}
template<class Message> inline void enforce(bool condition, Message&& message,
        std::source_location origin = std::source_location::current()) {
    if (!condition) throw compat::ThrowableException(
        evaluatedMessage(std::forward<Message>(message)), {}, origin);
}
template<class Message> inline void enforce(bool condition, Message&& message, std::size_t line) {
    if (!condition) throw compat::ThrowableException(
        evaluatedMessage(std::forward<Message>(message)),
        Text::fromStaticLiteral("source/nijigenerate/autorig/framework.d"), line);
}
template<class Map, class Key> decltype(auto) aaAt(Map& values, const Key& key, std::size_t line) {
    const auto found = values.find(key);
    if (found == values.end()) throw compat::ngStaticError<compat::RangeError>(
        Text::fromStaticLiteral("source/nijigenerate/autorig/framework.d"), line);
    return (found->second);
}
inline Text valueKindText(AutoRigValueKind kind) {
    switch (kind) {
    case AutoRigValueKind::FileName: return Text::fromStaticLiteral("FileName");
    case AutoRigValueKind::Path: return Text::fromStaticLiteral("Path");
    case AutoRigValueKind::Json: return Text::fromStaticLiteral("Json");
    case AutoRigValueKind::Blob: return Text::fromStaticLiteral("Blob");
    }
    // std.conv:1083-1115 prints the signed underlying value for unnamed enums.
    char number[12];
    const auto converted = std::to_chars(number, number + sizeof(number), static_cast<std::int32_t>(kind));
    try {
        std::string bytes("cast(AutoRigValueKind)");
        bytes.append(number, static_cast<std::size_t>(converted.ptr - number));
        return Text(std::move(bytes));
    } catch (const std::bad_alloc&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    } catch (const std::length_error&) {
        throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
    }
}
template<class T> compat::Slice<Text> keys(const Table<T>& values) {
    return values.keys();
}
inline bool safeId(std::string_view id) {
    if (id.empty() || id == "." || id == "..") return false;
    for (const unsigned char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
    return true;
}
inline bool safeId(const Text& id) { return safeId(id.view()); }
inline bool safeFileName(std::string_view name) {
    if (name.empty() || name == "." || name == ".." || name.back() == '.' || name.back() == ' ') return false;
    for (const unsigned char c : name)
        if (c < ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|') return false;
    return true;
}
inline bool safeFileName(const Text& name) { return safeFileName(name.view()); }
inline bool hasPort(compat::Slice<AutoRigPortSpec> ports, std::string_view id) {
    for (const auto port : ports) if (port.id.view() == id) return true;
    return false;
}
inline AutoRigValueKind portKind(compat::Slice<AutoRigPortSpec> ports, std::string_view id) {
    for (const auto port : ports) if (port.id.view() == id) return port.kind;
    throw compat::ThrowableException(Text::fromStaticLiteral("Unknown AutoRig port: ") + Text::fromBorrowedView(id),
        Text::fromStaticLiteral("source/nijigenerate/autorig/framework.d"), 1148);
}
inline bool hasPort(compat::Slice<AutoRigPortSpec> ports, const Text& id) { return hasPort(ports, id.view()); }
inline AutoRigValueKind portKind(compat::Slice<AutoRigPortSpec> ports, const Text& id) { return portKind(ports, id.view()); }
inline std::filesystem::path filePath(std::string_view value) {
#ifdef _WIN32
    compat::phobos::detail::UnicodeCursor<std::span<const char>, char> cursor(
        std::span<const char>{value.data(), value.size()});
    std::wstring result;
    while (!cursor.empty()) {
        auto scalar = cursor.front(); cursor.popFront();
        if (scalar <= 0xffff) result.push_back(static_cast<wchar_t>(scalar));
        else {
            scalar -= 0x10000;
            result.push_back(static_cast<wchar_t>(0xd800 + (scalar >> 10)));
            result.push_back(static_cast<wchar_t>(0xdc00 + (scalar & 0x3ff)));
        }
    }
    return std::filesystem::path(std::move(result));
#else
    return std::filesystem::path(std::u8string(value.begin(), value.end()));
#endif
}
inline std::filesystem::path filePath(const Text& value) { return filePath(value.view()); }
inline std::string pathString(const std::filesystem::path& value) {
#ifdef _WIN32
    const auto& units = value.native();
    compat::phobos::detail::UnicodeCursor<std::span<const wchar_t>, wchar_t> cursor(
        std::span<const wchar_t>{units.data(), units.size()});
    std::string result;
    while (!cursor.empty()) {
        compat::file_exception_detail::appendScalar(result, cursor.front()); cursor.popFront();
    }
    return result;
#else
    const auto text = value.u8string(); return std::string(text.begin(), text.end());
#endif
}
inline bool fileExists(const Text& name) {
#ifdef _WIN32
    return GetFileAttributesW(filePath(name).c_str()) != INVALID_FILE_ATTRIBUTES;
#else
    std::error_code error;
    return std::filesystem::exists(std::filesystem::symlink_status(filePath(name), error));
#endif
}
inline bool fileIsDirectory(const Text& name) {
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(filePath(name).c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) throw compat::FileException(name, GetLastError());
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    std::error_code error;
    const auto status = std::filesystem::status(filePath(name), error);
    if (error || status.type() == std::filesystem::file_type::not_found)
        throw compat::FileException(name, static_cast<std::uint32_t>(error ? error.value() : ENOENT));
    return std::filesystem::is_directory(status);
#endif
}
inline bool fileIsSymlink(const Text& name) {
#ifdef _WIN32
    const auto attributes = GetFileAttributesW(filePath(name).c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) throw compat::FileException(name, GetLastError());
    return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    std::error_code error;
    const auto status = std::filesystem::symlink_status(filePath(name), error);
    if (error || status.type() == std::filesystem::file_type::not_found)
        throw compat::FileException(name, static_cast<std::uint32_t>(error ? error.value() : ENOENT));
    return std::filesystem::is_symlink(status);
#endif
}
#ifdef _WIN32
inline void removeDirectoryEntry(const std::filesystem::path& path, DWORD attributes) {
    const Text name(pathString(path));
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) && !(attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        WIN32_FIND_DATAW entry;
        const auto search = path / L"*";
        const auto handle = FindFirstFileW(search.c_str(), &entry);
        if (handle == INVALID_HANDLE_VALUE) {
            const auto error = GetLastError();
            if (error != ERROR_FILE_NOT_FOUND) throw compat::FileException(name, error);
        } else {
            struct Close { HANDLE value; ~Close() { FindClose(value); } } close{handle};
            for (;;) {
                const std::wstring_view child(entry.cFileName);
                if (child != L"." && child != L"..")
                    removeDirectoryEntry(path / entry.cFileName, entry.dwFileAttributes);
                if (!FindNextFileW(handle, &entry)) {
                    const auto error = GetLastError();
                    if (error != ERROR_NO_MORE_FILES) throw compat::FileException(name, error);
                    break;
                }
            }
        }
    }
    const bool removed = attributes & FILE_ATTRIBUTE_DIRECTORY
        ? RemoveDirectoryW(path.c_str()) != 0 : DeleteFileW(path.c_str()) != 0;
    if (!removed) throw compat::FileException(name, GetLastError());
}
#endif
inline void removeDirectoryRecurse(const Text& name) {
#ifdef _WIN32
    const auto path = filePath(name);
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) throw compat::FileException(name, GetLastError());
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
        throw compat::FileException(name, Text::fromStaticLiteral("Not a directory"));
    removeDirectoryEntry(path, attributes);
#else
    if (!fileIsDirectory(name))
        throw compat::FileException(name, Text::fromStaticLiteral("Not a directory"));
    try { std::filesystem::remove_all(filePath(name)); }
    catch (const std::filesystem::filesystem_error& error) {
        throw compat::FileException(name, static_cast<std::uint32_t>(error.code().value()));
    }
#endif
}
inline bool pathSeparator(char value) {
#ifdef _WIN32
    return value == '/' || value == '\\';
#else
    return value == '/';
#endif
}
inline bool uncPath(std::string_view value) {
    return value.size() >= 3 && pathSeparator(value[0]) && pathSeparator(value[1]) &&
        !pathSeparator(value[2]);
}
inline bool absolutePathValue(std::string_view value) {
#ifdef _WIN32
    return (value.size() >= 3 && value[1] == ':' && pathSeparator(value[2])) || uncPath(value);
#else
    return !value.empty() && pathSeparator(value[0]);
#endif
}
inline std::size_t uncRootLength(std::string_view value) {
    std::size_t index = 3;
    while (index < value.size() && !pathSeparator(value[index])) ++index;
    if (index < value.size()) {
        auto next = index;
        do { ++next; } while (next < value.size() && pathSeparator(value[next]));
        if (next < value.size()) {
            do { ++next; } while (next < value.size() && !pathSeparator(value[next]));
            index = next;
        }
    }
    return index;
}
inline std::size_t rootLength(std::string_view value) {
    if (value.empty()) return 0;
#ifdef _WIN32
    if (pathSeparator(value[0])) return uncPath(value) ? uncRootLength(value) : 1;
    return value.size() >= 3 && value[1] == ':' && pathSeparator(value[2]) ? 3 : 0;
#else
    return pathSeparator(value[0]) ? 1 : 0;
#endif
}
// Phobos buildPath skips empty segments and does not normalize dots or decode
// UTF-8. Rooted Windows segments retain the previous drive/share root.
inline std::string joined(std::string_view a, std::string_view b) {
    if (b.empty()) return std::string(a);
    auto length = a.size();
    bool separator = false;
    if (length) {
        if (rootLength(b)) {
            length = absolutePathValue(b) ? 0 : rootLength(a);
            if (length && pathSeparator(a[length - 1])) --length;
        } else if (!pathSeparator(a[length - 1])) separator = true;
    }
    std::string result(a.substr(0, length));
    if (separator) {
#ifdef _WIN32
        result.push_back('\\');
#else
        result.push_back('/');
#endif
    }
    result.append(b);
    return result;
}
inline Text joined(const Text& a, const Text& b) { return Text(joined(a.view(), b.view())); }
inline Text absolutePath(const Text& value) {
    if (value.empty()) return {};
    if (absolutePathValue(value.view())) return value;
    Text base;
#ifdef _WIN32
    std::array<wchar_t, 4096> buffer;
    const auto count = GetCurrentDirectoryW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (!count) throw compat::FileException(Text::fromStaticLiteral("getcwd"), GetLastError());
    if (count < buffer.size()) base = pathString(std::filesystem::path(std::wstring(buffer.data(), count)));
    else {
        std::vector<wchar_t> large(count);
        const auto length = GetCurrentDirectoryW(count, large.data());
        if (!length || length >= count)
            throw compat::FileException(Text::fromStaticLiteral("getcwd"), GetLastError());
        base = pathString(std::filesystem::path(std::wstring(large.data(), length)));
    }
#else
    std::error_code error;
    const auto directory = std::filesystem::current_path(error);
    if (error) throw compat::FileException(Text::fromStaticLiteral("getcwd"),
        static_cast<std::uint32_t>(error.value()));
    base = pathString(directory);
#endif
    enforce(absolutePathValue(base.view()), [&] { return Text(Text::fromStaticLiteral("Base directory must be absolute")); });
    return joined(base, value);
}
inline Text pathBaseName(const Text& path) {
    const auto bytes = path.view();
    std::size_t first = 0, last = bytes.size();
#ifdef _WIN32
    if (bytes.size() >= 2 && bytes[1] == ':') first = 2;
    else if (uncPath(bytes)) first = uncRootLength(bytes);
#endif
    if (first == last) {
#ifdef _WIN32
        if (uncPath(bytes)) return path.subslice(0, 1);
#endif
        return {};
    }
    while (last > first && pathSeparator(bytes[last - 1])) --last;
    if (last == first) return path.subslice(first, first + 1);
    auto begin = last;
    while (begin > first && !pathSeparator(bytes[begin - 1])) --begin;
    return path.subslice(begin, last);
}
// Actual std.json objectNoRef/opIndex: ordered objects are materialized into
// an unordered AA before the key lookup, rather than using a ref accessor.
inline Json jsonIndex(Json value, Text key) {
    return value.sourceIndexValue(key.view());
}
compat::Slice<std::uint8_t> readBytes(std::string_view path);
Json readJson(std::string_view path);
inline compat::Slice<std::uint8_t> readBytes(const Text& path) { return readBytes(path.view()); }
inline Json readJson(const Text& path) { return readJson(path.view()); }
}
