// Required Windows paths from Phobos std.process/std.stdio (BSL-1.0).
// Attribution and license: native/port/licenses/phobos-videoexport.txt.
#include "videoexport_process.hpp"
#include <nijigenerate/compat/phobos_execute_shell.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/phobos_process_exception.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/scope_exit.hpp>
#include "nijigenerate/core/i18n_case.hpp"
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#include <bit>
#include <cerrno>
#include <cstring>
#include <array>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace nijigenerate::io::videoexport::detail {
namespace {
std::wstring wide(std::string_view value) {
    // std.internal.cstring.tempCStringW uses byUTF with replacement enabled.
    core::i18n::detail::LocaleCaseCursor cursor{value};
    std::wstring result;
    while (!cursor.empty()) {
        auto scalar = cursor.take();
        if (scalar < 0x10000)
            allocateVideoStorage([&] { result.push_back(static_cast<wchar_t>(scalar)); });
        else {
            scalar -= 0x10000;
            allocateVideoStorage([&] { result.push_back(static_cast<wchar_t>(0xd800 + (scalar >> 10))); });
            allocateVideoStorage([&] { result.push_back(static_cast<wchar_t>(0xdc00 + (scalar & 0x3ff))); });
        }
    }
    return result;
}
std::string systemMessage(DWORD code) {
    wchar_t* buffer = nullptr;
    const auto length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    if (!length) return allocateVideoStorage([&] { return "Error " + std::to_string(code); });
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
    auto output = allocateVideoStorage([&] { return std::string(static_cast<std::size_t>(count), '\0'); });
    WideCharToMultiByte(CP_UTF8, 0, buffer + first, last - first, output.data(), count, nullptr, nullptr);
    return output;
}
[[noreturn]] void processError(std::string_view prefix, std::size_t sourceLine) {
    const auto code = GetLastError();
    auto system = systemMessage(code);
    const auto message = allocateVideoStorage([&] {
        return compat::RetainedString(prefix.empty() ? std::move(system)
            : std::string(prefix) + " (" + system + ")");
    });
    throw compat::ProcessException(message, sourceLine);
}
[[noreturn]] void stdioLastError(std::string_view prefix) {
    const auto code = GetLastError();
    const auto system = systemMessage(code);
    const auto message = allocateVideoStorage([&] {
        return compat::RetainedString(std::string(prefix) + " (" + system + ")");
    });
    providers::mcp::stdio::StdioException error(message, 0);
    error.file = compat::RetainedString::fromStaticLiteral("std/stdio.d");
    error.line = 5309;
    throw error;
}
[[noreturn]] void raiseErrno(int code, VideoString message, std::size_t sourceLine) {
    providers::mcp::stdio::ErrnoException error(std::move(message), code);
    error.file = compat::RetainedString::fromStaticLiteral("std/stdio.d");
    error.line = sourceLine;
    throw error;
}
[[noreturn]] void errnoError(const char* message = nullptr, std::size_t sourceLine = 1029) {
    const auto retained = message ? allocateVideoStorage([&] {
        return *message ? VideoString(std::string(message)) : VideoString{};
    }) : VideoString::fromStaticLiteral("Enforcement failed");
    const int code = errno;
    raiseErrno(code, retained, sourceLine);
}
[[noreturn]] void errnoError(std::string message, std::size_t sourceLine) {
    const auto retained = allocateVideoStorage([&] {
        return message.empty() ? VideoString{} : VideoString(std::string(message));
    });
    const int code = errno;
    raiseErrno(code, retained, sourceLine);
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
struct Stream {
    std::FILE* file = nullptr;
    ~Stream() { if (file) std::fclose(file); }
    std::FILE* take() { return std::exchange(file, nullptr); }
};
void attach(HANDLE handle, const char* mode, int flags, Stream& stream) {
    const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle), flags);
    if (fd < 0) { const int code = errno; CloseHandle(handle); errno = code;
        errnoError("Cannot open Windows HANDLE", 886); }
    stream.file = _fdopen(fd, mode);
    if (!stream.file) { const int code = errno; _close(fd); errno = code; errnoError(nullptr, 834); }
}
void pipe(Stream& read, Stream& write) {
    HANDLE first = nullptr, second = nullptr;
    if (!CreatePipe(&first, &second, nullptr, 0)) stdioLastError("Error creating pipe");
    try { attach(first, "r", _O_RDONLY, read); }
    catch (...) { CloseHandle(second); throw; }
    attach(second, "a", _O_APPEND, write);
}
HANDLE inherited(std::FILE* file, DWORD standard, const char* which) {
    HANDLE handle = nullptr;
    if (file && _fileno(file) >= 0) handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(file)));
    if (!handle || handle == INVALID_HANDLE_VALUE || handle == reinterpret_cast<HANDLE>(-2))
        handle = GetStdHandle(standard);
    DWORD flags = 0;
    if (GetHandleInformation(handle, &flags) && !(flags & HANDLE_FLAG_INHERIT)) {
        if (!SetHandleInformation(handle, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT)) {
            stdioLastError(allocateVideoStorage([&] {
                return std::string("Failed to make ") + which + " stream inheritable by child process";
            }));
        }
    }
    return handle;
}
void flush(std::FILE* stream) {
    if (!stream) throw compat::ThrowableException(
        compat::RetainedString::fromStaticLiteral("Attempting to flush() in an unopened file"),
        compat::RetainedString::fromStaticLiteral("std/stdio.d"), 1028);
    if (std::fflush(stream) != 0) errnoError();
}
}
ProcessPipes::~ProcessPipes() {
    if (input) std::fclose(input);
    if (output) std::fclose(output);
    if (process) CloseHandle(process);
}
void ProcessPipes::closeInput() {
    auto file = std::exchange(input, nullptr);
    if (file && std::fclose(file) != 0) {
        errnoError(allocateVideoStorage([&] { return "Could not close file `" + inputName + "'"; }), 693);
    }
}
bool ProcessPipes::inputError() const { return !input || std::ferror(input); }
void ProcessPipes::writeFrame(std::span<const std::uint8_t> bytes) {
    if (!input) compat::ngFailNullAccess(false);
    const int fd = _fileno(input);
    const int oldMode = _setmode(fd, _O_BINARY);
    if (oldMode != _O_BINARY) {
        _setmode(fd, oldMode);
        flush(input);
        _setmode(fd, _O_BINARY);
    }
    const auto restore = [&] {
        if (oldMode != _O_BINARY) { flush(input); _setmode(fd, oldMode); }
    };
    compat::ngRunWithScopeExit([&] {
        auto written = std::fwrite(bytes.data(), 1, bytes.size(), input);
        if (written == static_cast<std::size_t>(-1)) written = 0;
        if (written != bytes.size()) {
            errnoError(allocateVideoStorage([&] {
                return "Wrote " + std::to_string(written) + " instead of " +
                    std::to_string(bytes.size()) + " objects of type ubyte to file `" + inputName + "'";
            }), 1214);
        }
    }, restore);
    flush(input);
}
bool ProcessPipes::poll() {
    if (terminated) return true;
    if (!process) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Called tryWait on a null Pid."),
        compat::RetainedString::fromStaticLiteral("std/process.d"), 2708);
    DWORD code = 0;
    if (!GetExitCodeProcess(process, &code)) processError({}, 2468);
    exitCode = std::bit_cast<std::int32_t>(code);
    if (code == STILL_ACTIVE) return false;
    CloseHandle(process);
    process = nullptr;
    terminated = true;
    return true;
}
std::int32_t ProcessPipes::wait() {
    if (terminated) return exitCode;
    if (!process) throw compat::ngStaticError<compat::AssertError>(
        compat::RetainedString::fromStaticLiteral("Called wait on a null Pid."),
        compat::RetainedString::fromStaticLiteral("std/process.d"), 2577);
    if (WaitForSingleObject(process, INFINITE) != WAIT_OBJECT_0) processError("Wait failed.", 2464);
    DWORD code = 0;
    if (!GetExitCodeProcess(process, &code)) processError({}, 2468);
    exitCode = std::bit_cast<std::int32_t>(code);
    CloseHandle(process);
    process = nullptr;
    terminated = true;
    return exitCode;
}
static std::shared_ptr<ProcessPipes> launchCommand(compat::Slice<VideoString> arguments,
    bool captureOutput, bool suppressConsole, const VideoString* shellCommand = nullptr) {
    if (arguments.empty()) {
        // The source explicitly allocates RangeError("Command line is empty"):
        // that sole argument is `file`, while msg remains "Range violation".
        throw compat::RangeError(compat::RetainedString::fromStaticLiteral("Command line is empty"), 1426);
    }
    std::string command;
    for (std::size_t index = 0; !shellCommand && index < arguments.size(); ++index) {
        if (index) allocateVideoStorage([&] { command.push_back(' '); });
        const auto quoted = quote(arguments[index].view());
        allocateVideoStorage([&] { command += quoted; });
    }
    Stream childRead, parentWrite, parentRead, childWrite;
    if (captureOutput) pipe(parentRead, childWrite);
    else pipe(childRead, parentWrite);
    if (shellCommand) allocateVideoStorage([&] {
        // pipeShell creates its File ends before spawnShell forms this line.
        command = std::string("\"cmd.exe\" /C \"") + std::string(shellCommand->view()) + '"';
    });
    auto result = std::allocate_shared<ProcessPipes>(
        nicxlive::core::d_runtime::SourceAllocator<ProcessPipes>{});
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.hStdInput = inherited(captureOutput ? stdin : childRead.file, STD_INPUT_HANDLE, "stdin");
    startup.hStdOutput = inherited(captureOutput ? childWrite.file : stdout, STD_OUTPUT_HANDLE, "stdout");
    startup.hStdError = inherited(captureOutput ? childWrite.file : stderr, STD_ERROR_HANDLE, "stderr");
    const auto valid = [](HANDLE value) { return value && value != INVALID_HANDLE_VALUE; };
    if (valid(startup.hStdInput) || valid(startup.hStdOutput) || valid(startup.hStdError))
        startup.dwFlags = STARTF_USESTDHANDLES;
    auto wideCommand = wide(command);
    PROCESS_INFORMATION process{};
    const DWORD flags = CREATE_UNICODE_ENVIRONMENT | (suppressConsole ? CREATE_NO_WINDOW : 0);
    if (!CreateProcessW(nullptr, wideCommand.data(), nullptr, nullptr, TRUE, flags,
            nullptr, nullptr, &startup, &process)) {
        processError(allocateVideoStorage([&] {
            return "Failed to spawn process \"" + std::string(arguments[0].view()) + "\"";
        }), 1487);
    }
    CloseHandle(process.hThread);
    result->process = process.hProcess;
    if (parentWrite.file) {
        const auto handle = _get_osfhandle(_fileno(parentWrite.file));
        char name[64]{};
        std::snprintf(name, sizeof(name), "HANDLE(%llX)", static_cast<unsigned long long>(handle));
        allocateVideoStorage([&] { result->inputName = name; });
    }
    result->input = parentWrite.take();
    result->output = parentRead.take();
    return result;
}
std::shared_ptr<ProcessPipes> launch(compat::Slice<VideoString> arguments,
    bool captureOutput, bool suppressConsole) {
    return launchCommand(std::move(arguments), captureOutput, suppressConsole);
}
static std::pair<std::int32_t, std::string> capture(std::shared_ptr<ProcessPipes> process) {
    const int descriptor = _fileno(process->output);
    std::string output;
    const auto readChunk = [&](compat::Slice<std::uint8_t> buffer) {
        const int oldMode = _setmode(descriptor, _O_BINARY);
        compat::Slice<std::uint8_t> result;
        compat::ngRunWithScopeExit([&] {
            const auto count = std::fread(buffer.data(), 1, buffer.size(), process->output);
            if (count < buffer.size() && std::ferror(process->output)) errnoError(nullptr, 1116);
            result = buffer.slice(0, count);
        }, [&] { _setmode(descriptor, oldMode); });
        return result;
    };
    {
        compat::Slice<std::uint8_t> buffer;
        buffer.resize(4096);
        auto chunk = readChunk(buffer);
        while (!chunk.empty()) {
            allocateVideoStorage([&] {
                output.append(reinterpret_cast<const char*>(chunk.data()), chunk.size());
            });
            chunk = readChunk(chunk);
        }
    }
    {
        compat::Slice<std::uint8_t> buffer;
        buffer.resize(4096);
        auto chunk = readChunk(buffer);
        while (!chunk.empty()) chunk = readChunk(chunk);
    }
    return {process->wait(), std::move(output)};
}
std::pair<std::int32_t, std::string> execute(compat::Slice<VideoString> arguments) {
    return capture(launch(std::move(arguments), true, false));
}
compat::phobos::ExecuteShellResult executeShell(VideoString command) {
    // std.process.spawnShell uses CMD's first/last quote rule, not argv quoting.
    auto result = capture(launchCommand({VideoString::fromStaticLiteral("cmd.exe")}, true, false, &command));
    auto output = allocateVideoStorage([&] {
        return result.second.empty() ? VideoString{} : VideoString(std::move(result.second));
    });
    return {result.first, std::move(output)};
}
}

namespace nijigenerate::compat::phobos {
ExecuteShellResult ngExecuteShell(RetainedString command) {
    return ::nijigenerate::io::videoexport::detail::executeShell(std::move(command));
}
}
