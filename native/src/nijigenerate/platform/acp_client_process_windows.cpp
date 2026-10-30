// Windows process provider adapted from current Phobos std.process. BSL-1.0.
// License: native/port/licenses/phobos-videoexport.txt.
#include <nijigenerate/api/acp/client_process.hpp>
#if !defined(_WIN32)
#error "The ACP Windows process provider requires a Windows target"
#endif
#include <nijigenerate/core/i18n_case.hpp>
#include <nijigenerate/compat/phobos_process_exception.hpp>
#include <nijigenerate/compat/phobos_file_exception.hpp>
#include <nijigenerate/compat/thread_exception.hpp>
#include <nijigenerate/api/acp/detail/client_storage.hpp>
#include <compat/source_allocator.hpp>
#include <compat/range_error.hpp>
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <bit>
#include <array>
#include <span>
#include <vector>
#include <algorithm>
namespace nijigenerate::api::acp::client::detail {
namespace {
std::wstring wide(std::string_view value) {
    core::i18n::detail::LocaleCaseCursor cursor{value};
    std::wstring result;
    while (!cursor.empty()) {
        auto scalar = cursor.take();
        if (scalar < 0x10000)
            allocateClientStorage([&] { result.push_back(static_cast<wchar_t>(scalar)); });
        else {
            scalar -= 0x10000;
            allocateClientStorage([&] { result.push_back(static_cast<wchar_t>(0xd800 + (scalar >> 10))); });
            allocateClientStorage([&] { result.push_back(static_cast<wchar_t>(0xdc00 + (scalar & 0x3ff))); });
        }
    }
    return result;
}
std::string systemMessage(DWORD code) {
    wchar_t* buffer = nullptr;
    const auto length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    if (!length) return allocateClientStorage([&] { return "Error " + std::to_string(code); });
    const auto release = [](wchar_t* value) { LocalFree(value); };
    const std::unique_ptr<wchar_t, decltype(release)> owner(buffer, release);
    const auto white = [](wchar_t c) {
        return (c >= 9 && c <= 13) || c == 32 || c == 0x85 || c == 0xa0 || c == 0x1680 ||
            (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f ||
            c == 0x205f || c == 0x3000;
    };
    DWORD first = 0, last = length;
    while (first < last && white(buffer[first])) ++first;
    while (last > first && white(buffer[last - 1])) --last;
    const int count = WideCharToMultiByte(CP_UTF8, 0, buffer + first, last - first, nullptr, 0, nullptr, nullptr);
    auto output = allocateClientStorage([&] { return std::string(static_cast<std::size_t>(count), '\0'); });
    WideCharToMultiByte(CP_UTF8, 0, buffer + first, last - first, output.data(), count, nullptr, nullptr);
    return output;
}
[[noreturn]] void processError(std::string_view prefix, std::size_t line) {
    const auto code = GetLastError();
    const auto system = systemMessage(code);
    const auto message = prefix.empty() ? retainClientText(copyClientText(system))
        : concatenateClientText({prefix, " (", system, ")"});
    throw compat::ProcessException(message, line);
}
[[noreturn]] void errnoError(std::string_view message = "Enforcement failed") {
    const auto code = errno;
    const auto retained = retainClientText(copyClientText(message));
    throw providers::mcp::stdio::ErrnoException(retained, code);
}
std::string quote(std::string_view argument) try {
    bool escaping = true;
    bool needed = argument.empty() || (argument.back() >= '0' && argument.back() <= '9');
    std::size_t size = argument.size() + 2;
    for (auto it = argument.rbegin(); it != argument.rend(); ++it) {
        const char value = *it;
        if (value == '"') { needed = true; escaping = true; ++size; }
        else if (value == '\\') { if (escaping) ++size; }
        else { if (value == ' ' || value == '\t') needed = true; escaping = false; }
    }
    if (!needed) return std::string(argument);
    std::string result(size, '\0');
    auto position = size;
    result[--position] = '"';
    escaping = true;
    for (auto it = argument.rbegin(); it != argument.rend(); ++it) {
        const char value = *it;
        if (value == '"') escaping = true;
        else if (value != '\\') escaping = false;
        result[--position] = value;
        if (escaping) result[--position] = '\\';
    }
    result[--position] = '"';
    return result;
} catch (const std::bad_alloc&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
} catch (const std::length_error&) {
    throw compat::ngStaticError<compat::OutOfMemoryError>(std::source_location::current());
}
using SharedFile = providers::mcp::stdio::SharedFile;
SharedFile attach(HANDLE handle, const char* mode, int flags) {
    const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle), flags);
    if (fd < 0) {
        const auto initial = errno;
        CloseHandle(handle);
        errno = initial;
        const auto message = duplicateClientText("Cannot open Windows HANDLE");
        const auto failure = errno;
        providers::mcp::stdio::ErrnoException error(message, failure);
        error.file = compat::RetainedString::fromStaticLiteral("std/stdio.d"); error.line = 886;
        throw error;
    }
    // windowsHandleOpen evaluates its formatted filename before fdopen.
    std::string filename;
    try {
        char name[64]{};
        std::snprintf(name, sizeof(name), "HANDLE(%llX)",
            static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(handle)));
        filename = copyClientText(name);
    } catch (...) {
        _close(fd);
        throw;
    }
    auto* file = _fdopen(fd, mode);
    if (!file) {
        const auto failure = errno;
        _close(fd);
        providers::mcp::stdio::ErrnoException error(
            compat::RetainedString::fromStaticLiteral("Enforcement failed"), failure);
        error.file = compat::RetainedString::fromStaticLiteral("std/stdio.d"); error.line = 834;
        throw error;
    }
    std::unique_ptr<std::FILE, decltype(&std::fclose)> pending(file, &std::fclose);
    return ngOwnClientFile(pending.release(), std::move(filename));
}
void pipe(SharedFile& read, SharedFile& write) {
    HANDLE first = nullptr, second = nullptr;
    if (!CreatePipe(&first, &second, nullptr, 0)) {
        const auto system = systemMessage(GetLastError());
        throw providers::mcp::stdio::StdioException(
            concatenateClientText({"Error creating pipe (", system, ")"}), 0);
    }
    try {
        try { read = attach(first, "r", _O_RDONLY); }
        catch (...) { CloseHandle(second); throw; }
        write = attach(second, "a", _O_APPEND);
    } catch (const compat::ThrowableException& error) {
        const auto message = error.msg;
        throw providers::mcp::stdio::StdioException(
            concatenateClientText({"Error attaching pipe (", message.view(), ")"}), 0);
    }
}
HANDLE inherited(const SharedFile& file, DWORD standard, const char* which) {
    HANDLE handle = file.isOpen() ? reinterpret_cast<HANDLE>(file.nativeHandle()) : nullptr;
    if (!handle || handle == INVALID_HANDLE_VALUE || handle == reinterpret_cast<HANDLE>(-2))
        handle = GetStdHandle(standard);
    DWORD flags = 0;
    if (GetHandleInformation(handle, &flags) && !(flags & HANDLE_FLAG_INHERIT)) {
        if (!SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
            const auto prefix = concatenateClientBytes({"Failed to make ", which,
                " stream inheritable by child process ("});
            const auto system = systemMessage(GetLastError());
            throw providers::mcp::stdio::StdioException(concatenateClientText({prefix, system, ")"}), 0);
        }
    }
    return handle;
}
}
struct Pid::State {
    HANDLE handle = nullptr;
    DWORD id = 0;
    ~State() { if (handle) CloseHandle(handle); }
};
std::int32_t Pid::processID() const { return state ? std::bit_cast<std::int32_t>(state->id) : -1; }
void ngJoinClientThread(std::thread& thread) {
    // The original Windows Thread.join has no self-join shortcut. In particular,
    // close from the reader callback waits on that same thread indefinitely.
    if (thread.get_id() == std::this_thread::get_id()) {
        if (WaitForSingleObject(thread.native_handle(), INFINITE) != WAIT_OBJECT_0)
            compat::ngThrowThreadException(compat::RetainedString::fromStaticLiteral("Unable to join thread"),
                compat::RetainedString::fromStaticLiteral("core/thread/osthread.d"), 606);
    }
    try { thread.join(); }
    catch (const std::system_error&) {
        compat::ngThrowThreadException(compat::RetainedString::fromStaticLiteral("Unable to join thread"),
            compat::RetainedString::fromStaticLiteral("core/thread/osthread.d"), 606);
    }
}
ProcessPipes ngLaunchClient(compat::Slice<compat::RetainedStringSlice> command,
    compat::RetainedStringSlice cwd) {
    std::string line;
    for (const auto& argument : command) {
        if (!line.empty()) appendClientText(line, " ");
        const auto quoted = quote(argument.view());
        appendClientText(line, quoted);
    }
    SharedFile childInput, parentInput, parentOutput, childOutput, parentError, childError;
    pipe(childInput, parentInput);
    pipe(parentOutput, childOutput);
    pipe(parentError, childError);
    if (line.empty()) throw ::nicxlive::core::d_runtime::RangeError(
        compat::RetainedString::fromStaticLiteral("Command line is empty"), 1426);
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.hStdInput = inherited(childInput, STD_INPUT_HANDLE, "stdin");
    startup.hStdOutput = inherited(childOutput, STD_OUTPUT_HANDLE, "stdout");
    startup.hStdError = inherited(childError, STD_ERROR_HANDLE, "stderr");
    startup.dwFlags = STARTF_USESTDHANDLES;
    auto wideLine = wide(line), directory = wide(cwd.view());
    PROCESS_INFORMATION info{};
    if (!CreateProcessW(nullptr, wideLine.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT,
        nullptr, cwd.empty() ? nullptr : directory.c_str(), &startup, &info))
        processError(concatenateClientBytes({"Failed to spawn process \"", command[0].view(), "\""}), 1487);
    childInput.close();
    childOutput.close();
    childError.close();
    CloseHandle(info.hThread);
    ProcessPipes result;
    result.pid.state = std::allocate_shared<Pid::State>(nicxlive::core::d_runtime::SourceAllocator<Pid::State>{});
    result.pid.state->handle = info.hProcess; result.pid.state->id = info.dwProcessId;
    result.input = parentInput; result.output = parentOutput; result.error = parentError;
    return result;
}
compat::RetainedStringSlice ngCurrentDirectory() {
    std::array<wchar_t, 4096> stack;
    const auto count = GetCurrentDirectoryW(static_cast<DWORD>(stack.size()), stack.data());
    if (!count) throw compat::FileException(compat::RetainedString::fromStaticLiteral("getcwd"));
    std::vector<wchar_t> dynamic;
    std::span<const wchar_t> input;
    if (count < stack.size()) input = {stack.data(), count};
    else {
        allocateClientStorage([&] { dynamic.resize(count); });
        const auto written = GetCurrentDirectoryW(count, dynamic.data());
        if (!written || written >= count)
            throw compat::FileException(compat::RetainedString::fromStaticLiteral("getcwd"));
        input = {dynamic.data(), written};
    }
    // std.conv wchar[] -> string preserves the real UTF exception boundary.
    using Cursor = compat::phobos::detail::UnicodeCursor<std::span<const wchar_t>, wchar_t>;
    Cursor cursor(input);
    std::string result;
    while (!cursor.empty()) {
        const auto scalar = cursor.front();
        allocateClientStorage([&] { compat::file_exception_detail::appendScalar(result, scalar); });
        cursor.popFront();
    }
    return retainClientText(std::move(result));
}
int ngClientStdoutFd(const providers::mcp::stdio::SharedFile&) { return -1; }

bool ngReadClientChunk(const SharedFile& file, compat::RetainedStringSlice& chunk) {
    if (!file.isOpen()) return false;
    // Source windowsHandle calls fileno, whose enforce rereads current state.
    if (!file.isOpen()) throw compat::ThrowableException(
        duplicateClientText("Attempting to call fileno() on an unopened file"),
        compat::RetainedString::fromStaticLiteral("std/stdio.d"), 2318);
    const auto handle = reinterpret_cast<HANDLE>(file.nativeHandle());
    if (!handle || handle == INVALID_HANDLE_VALUE) return false;
    DWORD available = 0;
    if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr) || !available) return false;
    std::array<std::uint8_t, 4096> buffer;
    const DWORD count = available < buffer.size() ? available : static_cast<DWORD>(buffer.size());
    DWORD read = 0;
    if (!ReadFile(handle, buffer.data(), count, &read, nullptr) || !read) return false;
    compat::Slice<std::uint8_t> bytes;
    bytes.resize(read);
    std::copy_n(buffer.data(), read, bytes.data());
    chunk = compat::RetainedString::retainedView(bytes.retentionOwner(),
        reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}

}
