#pragma once
#include <windows.h>
#include <objbase.h>
#include <algorithm>
#include <cstddef>
#include <cwchar>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nijigenerate::platform::tinyfiledialogs::process_detail {
class Handle {
    HANDLE value_ = INVALID_HANDLE_VALUE;
public:
    Handle() = default;
    explicit Handle(HANDLE value) : value_(value) {}
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : value_(std::exchange(other.value_, INVALID_HANDLE_VALUE)) {}
    ~Handle() { reset(); }
    void reset() {
        if (valid()) CloseHandle(value_);
        value_ = INVALID_HANDLE_VALUE;
    }
    bool valid() const { return value_ && value_ != INVALID_HANDLE_VALUE; }
    HANDLE get() const { return value_; }
};
class TemporaryFile {
    std::wstring path_;
    Handle file_;
    bool created_ = false;
    static std::wstring createPath(std::wstring_view suffix) {
        const auto length = GetTempPathW(0, nullptr);
        if (!length) return {};
        std::wstring directory(length, L'\0');
        const auto count = GetTempPathW(length, directory.data());
        if (!count || count >= length) return {};
        directory.resize(count);
        GUID id{};
        if (FAILED(CoCreateGuid(&id))) return {};
        wchar_t text[40]{};
        if (!StringFromGUID2(id, text, 40)) return {};
        return directory + L"tinyfd-" + text + std::wstring(suffix);
    }
public:
    explicit TemporaryFile(std::wstring_view suffix = L".tmp") : path_(createPath(suffix)),
        file_(path_.empty() ? INVALID_HANDLE_VALUE :
        CreateFileW(path_.c_str(), GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY, nullptr)), created_(file_.valid()) {}
    ~TemporaryFile() {
        file_.reset();
        if (created_) DeleteFileW(path_.c_str());
    }
    bool valid() const { return file_.valid(); }
    void close() { file_.reset(); }
    HANDLE get() const { return file_.get(); }
    const std::wstring& path() const { return path_; }
    bool writeBytes(const void* data, std::size_t size) {
        auto* bytes = static_cast<const char*>(data);
        while (size) {
            DWORD written = 0;
            const auto count = static_cast<DWORD>(std::min(size, std::size_t{0x40000000}));
            if (!WriteFile(get(), bytes, count, &written, nullptr) || !written) return false;
            bytes += written;
            size -= written;
        }
        return true;
    }
    bool writeText(std::wstring_view text) {
        static_assert(sizeof(wchar_t) == 2);
        const wchar_t bom = 0xfeff;
        return writeBytes(&bom, sizeof(bom)) && writeBytes(text.data(), text.size() * sizeof(wchar_t));
    }
    std::string readBytes() const {
        const Handle reader(file_.valid() ? INVALID_HANDLE_VALUE : CreateFileW(path_.c_str(),
            GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        const auto input = file_.valid() ? get() : reader.get();
        if (!input || input == INVALID_HANDLE_VALUE) return {};
        LARGE_INTEGER zero{};
        if (!SetFilePointerEx(input, zero, nullptr, FILE_BEGIN)) return {};
        std::string result;
        char buffer[1024];
        for (;;) {
            DWORD count = 0;
            if (!ReadFile(input, buffer, sizeof(buffer), &count, nullptr) || !count) return result;
            result.append(buffer, count);
        }
    }
};
inline std::wstring quote(std::wstring_view argument) {
    std::wstring result(1, L'"');
    std::size_t slashes = 0;
    for (const auto value : argument) {
        if (value == L'\\') { ++slashes; continue; }
        result.append(value == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        result.push_back(value);
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    result.push_back(L'"');
    return result;
}
using Environment = std::vector<std::pair<std::wstring, std::wstring>>;
inline std::wstring encodedCommand(std::wstring_view script) {
    static_assert(sizeof(wchar_t) == 2);
    constexpr wchar_t alphabet[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const auto* bytes = reinterpret_cast<const unsigned char*>(script.data());
    const auto length = script.size() * sizeof(wchar_t);
    std::wstring result;
    for (std::size_t i = 0; i < length; i += 3) {
        const unsigned bits = (unsigned(bytes[i]) << 16) |
            (i + 1 < length ? unsigned(bytes[i + 1]) << 8 : 0) |
            (i + 2 < length ? unsigned(bytes[i + 2]) : 0);
        result.push_back(alphabet[(bits >> 18) & 63]);
        result.push_back(alphabet[(bits >> 12) & 63]);
        result.push_back(i + 1 < length ? alphabet[(bits >> 6) & 63] : L'=');
        result.push_back(i + 2 < length ? alphabet[bits & 63] : L'=');
    }
    return result;
}
inline bool environment(const Environment& replacements, std::vector<wchar_t>& output) {
    auto* original = GetEnvironmentStringsW();
    if (!original) return false;
    const auto release = [](wchar_t* value) { FreeEnvironmentStringsW(value); };
    std::unique_ptr<wchar_t, decltype(release)> owner(original, release);
    std::vector<std::wstring> values;
    for (const wchar_t* value = original; *value; value += wcslen(value) + 1) {
        const std::wstring_view entry(value);
        const auto separator = entry.find(L'=', entry.front() == L'=' ? 1 : 0);
        const auto key = entry.substr(0, separator);
        const bool replaced = std::any_of(replacements.begin(), replacements.end(), [&](const auto& replacement) {
            return CompareStringOrdinal(key.data(), static_cast<int>(key.size()), replacement.first.data(),
                static_cast<int>(replacement.first.size()), TRUE) == CSTR_EQUAL;
        });
        if (!replaced) values.emplace_back(entry);
    }
    for (const auto& [key, value] : replacements) values.push_back(key + L"=" + value);
    std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
        return CompareStringOrdinal(a.data(), static_cast<int>(a.size()), b.data(),
            static_cast<int>(b.size()), TRUE) == CSTR_LESS_THAN;
    });
    for (const auto& value : values) {
        output.insert(output.end(), value.begin(), value.end());
        output.push_back(L'\0');
    }
    if (output.empty()) output.push_back(L'\0');
    output.push_back(L'\0');
    return true;
}
inline Handle inherit(HANDLE value, DWORD access, bool required = false) {
    HANDLE duplicate = nullptr;
    if (value && value != INVALID_HANDLE_VALUE && DuplicateHandle(GetCurrentProcess(), value,
            GetCurrentProcess(), &duplicate, 0, TRUE, DUPLICATE_SAME_ACCESS)) return Handle(duplicate);
    if (required) return {};
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    return Handle(CreateFileW(L"NUL", access, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
}
struct ForegroundWindow { DWORD process; HWND window = nullptr; };
inline BOOL CALLBACK findWindow(HWND window, LPARAM data) {
    auto& request = *reinterpret_cast<ForegroundWindow*>(data);
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process != request.process) return TRUE;
    wchar_t title[64]{};
    GetWindowTextW(window, title, 64);
    if (wcscmp(title, L"tinyfiledialogsTopWindow")) return TRUE;
    request.window = window;
    return FALSE;
}
struct Result { bool started = false; DWORD status = static_cast<DWORD>(-1); std::string output; };
inline Result run(const std::vector<std::wstring>& arguments, const Environment& replacements = {},
        bool hidden = false, const wchar_t* title = nullptr, bool captureErrors = true) {
    Result result;
    if (arguments.empty()) return result;
    std::wstring command;
    for (const auto& argument : arguments) {
        if (!command.empty()) command.push_back(L' ');
        command += quote(argument);
    }
    std::vector<wchar_t> variables;
    if (!replacements.empty() && !environment(replacements, variables)) return result;
    TemporaryFile errors;
    if (!errors.valid()) return result;
    const auto input = inherit(GetStdHandle(STD_INPUT_HANDLE), GENERIC_READ);
    const auto output = inherit(GetStdHandle(STD_OUTPUT_HANDLE), GENERIC_WRITE);
    const auto error = inherit(captureErrors ? errors.get() : GetStdHandle(STD_ERROR_HANDLE),
        GENERIC_WRITE, captureErrors);
    if (!input.valid() || !output.valid() || !error.valid()) return result;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | (hidden ? STARTF_USESHOWWINDOW : 0);
    startup.wShowWindow = SW_HIDE;
    startup.hStdInput = input.get();
    startup.hStdOutput = output.get();
    startup.hStdError = error.get();
    PROCESS_INFORMATION information{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
            CREATE_UNICODE_ENVIRONMENT | (hidden ? CREATE_NEW_CONSOLE : 0),
            variables.empty() ? nullptr : variables.data(), nullptr, &startup, &information)) return result;
    const Handle process(information.hProcess), thread(information.hThread);
    result.started = true;
    if (title) {
        WaitForInputIdle(process.get(), INFINITE);
        ForegroundWindow request{information.dwProcessId};
        while (!request.window && WaitForSingleObject(process.get(), 10) == WAIT_TIMEOUT)
            EnumWindows(&findWindow, reinterpret_cast<LPARAM>(&request));
        if (request.window) {
            SetWindowPos(request.window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            SetWindowTextW(request.window, title);
        }
    }
    if (WaitForSingleObject(process.get(), INFINITE) == WAIT_OBJECT_0)
        GetExitCodeProcess(process.get(), &result.status);
    if (captureErrors) result.output = errors.readBytes();
    return result;
}
} // namespace nijigenerate::platform::tinyfiledialogs::process_detail
