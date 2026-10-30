// Required POSIX paths from Phobos std.process/std.stdio (BSL-1.0).
// Attribution and license: native/port/licenses/phobos-videoexport.txt.
#include "videoexport_process.hpp"
#include <nijigenerate/compat/phobos_execute_shell.hpp>
#include <nijigenerate/compat/throwable.hpp>
#include <nijigenerate/compat/phobos_process_exception.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <nijigenerate/compat/native_access_fault.hpp>
#include <nijigenerate/compat/range_error.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/switch_error.hpp>
#include <array>
#include <bit>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <optional>
#include <poll.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#if defined(__GLIBC__)
#include <dlfcn.h>
#endif

#if defined(__APPLE__)
#include <crt_externs.h>
#else
extern char** environ;
#endif

namespace nijigenerate::io::videoexport::detail {
namespace {
char** processEnvironment() {
#if defined(__APPLE__)
    return *_NSGetEnviron();
#else
    return environ;
#endif
}
[[noreturn]] void processError(int code, const char* operation, std::size_t sourceLine = 1302) {
    const auto* system = std::strerror(code);
    const auto message = allocateVideoStorage([&] {
        return compat::RetainedString(std::string(operation) + " (" + system + ")");
    });
    throw compat::ProcessException(message, sourceLine);
}
[[noreturn]] void stdioError(int code, const char* message) {
    const auto retained = allocateVideoStorage([&] { return compat::RetainedString(message); });
    providers::mcp::stdio::StdioException error(retained, static_cast<std::uint32_t>(code));
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
[[noreturn]] void errnoError(std::string message, std::size_t sourceLine) {
    const auto retained = allocateVideoStorage([&] {
        return message.empty() ? VideoString{} : VideoString(std::string(message));
    });
    const int code = errno;
    raiseErrno(code, retained, sourceLine);
}
[[noreturn]] void errnoError(const char* message, std::size_t sourceLine) {
    const auto retained = message ? allocateVideoStorage([&] {
        return *message ? VideoString(std::string(message)) : VideoString{};
    }) : VideoString::fromStaticLiteral("Enforcement failed");
    const int code = errno;
    raiseErrno(code, retained, sourceLine);
}
struct Descriptor {
    int value = -1;
    ~Descriptor() { if (value >= 0) ::close(value); }
    int take() { return std::exchange(value, -1); }
};
void makePipe(Descriptor& read, Descriptor& write) {
    int descriptors[2];
    if (::pipe(descriptors) != 0) stdioError(errno, "Unable to create pipe");
    read.value = descriptors[0];
    write.value = descriptors[1];
}
std::optional<VideoString> searchPathFor(VideoString executable) {
    // std.process1615-1649: empty PATH has no splitter entries; empty entries
    // inside a nonempty PATH do search the current directory.
    const char* rawPath = std::getenv("PATH");
    if (!rawPath || !*rawPath) return {};
    const auto path = allocateVideoStorage([&] { return std::string(rawPath); });
    for (std::size_t begin = 0;;) {
        const auto end = path.find(':', begin);
        const auto directory = std::string_view(path).substr(begin,
            end == std::string::npos ? path.size() - begin : end - begin);
        auto candidate = allocateVideoStorage([&] {
            std::string value(directory);
            if (!value.empty() && value.back() != '/') value += '/';
            value.append(executable.view());
            return value;
        });
        if (::access(candidate.c_str(), X_OK) == 0)
            return allocateVideoStorage([&] { return VideoString(std::move(candidate)); });
        if (end == std::string::npos) return {};
        begin = end + 1;
    }
}

// The child follows std.process's raw C/POSIX path, including its documented
// malloc/getrlimit/opendir fallback. No C++ managed owner or exception is used.
struct ChildLaunch {
    int input, output, error;
    int reportRead, reportWrite;
    const char* executable;
    char* const* arguments;
    char* const* environment;
};
[[noreturn]] void abortChild(int report, unsigned char kind, int code) noexcept {
    (void)::write(report, &kind, sizeof(kind));
    (void)::write(report, &code, sizeof(code));
    (void)::close(report);
    ::_exit(1);
}
void fallbackCloseFrom(int lowDescriptor, int report) noexcept {
    // std.process.d:1042-1133; preserve the Source cast(int) and range behavior.
    rlimit limit;
    if (::getrlimit(RLIMIT_NOFILE, &limit) != 0) abortChild(report, 3, errno);
    const int maximum = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(limit.rlim_cur));
    DIR* directory = nullptr;
    if (maximum > 128 * 1024) {
        directory = ::opendir("/dev/fd");
        if (!directory) directory = ::opendir("/proc/self/fd");
    }
    if (directory) {
        const int directoryDescriptor = ::dirfd(directory);
        while (auto* entry = ::readdir(directory)) {
            if (entry->d_name[0] == '.') continue;
            const int descriptor = std::atoi(entry->d_name);
            if (descriptor < lowDescriptor || descriptor == directoryDescriptor) continue;
            (void)::close(descriptor);
        }
        (void)::closedir(directory);
        return;
    }
    if (maximum <= 128 * 1024) {
        const int count = std::bit_cast<std::int32_t>(
            static_cast<std::uint32_t>(maximum) - static_cast<std::uint32_t>(lowDescriptor));
        auto* descriptors = static_cast<pollfd*>(std::malloc(sizeof(pollfd) * static_cast<std::size_t>(count)));
        if (!descriptors) abortChild(report, 5, errno);
        for (int i = 0; i < count; ++i) {
            descriptors[i].fd = std::bit_cast<std::int32_t>(
                static_cast<std::uint32_t>(i) + static_cast<std::uint32_t>(lowDescriptor));
            descriptors[i].events = 0;
            descriptors[i].revents = 0;
        }
        if (::poll(descriptors, static_cast<nfds_t>(count), 0) >= 0) {
            for (int i = 0; i < count; ++i) {
                if (!(descriptors[i].revents & POLLNVAL)) (void)::close(descriptors[i].fd);
            }
            // Source retains this C allocation until exec/_exit; do not add free.
            return;
        }
    }
    for (int descriptor = lowDescriptor; descriptor < maximum; ++descriptor) (void)::close(descriptor);
}
void closeFrom(int lowDescriptor, int report) noexcept {
#if defined(__FreeBSD__) || defined(__OpenBSD__)
    ::closefrom(lowDescriptor);
#elif defined(__GLIBC__)
    // Source resolves closefrom in libc instead of assuming its link-time version.
    if (void* library = ::dlopen("libc.so.6", RTLD_LAZY)) {
        (void)::dlerror();
        using CloseFrom = void (*)(int);
        const auto function = reinterpret_cast<CloseFrom>(::dlsym(library, "closefrom"));
        if (!::dlerror()) {
            function(lowDescriptor);
            (void)::dlclose(library);
            return;
        }
        (void)::dlclose(library);
    }
    fallbackCloseFrom(lowDescriptor, report);
#else
    fallbackCloseFrom(lowDescriptor, report);
#endif
}
[[noreturn]] void executeChild(const ChildLaunch* launch) noexcept {
    (void)::close(launch->reportRead);
    int error = launch->error;
    if (error == STDOUT_FILENO) error = ::dup(error);
    (void)::dup2(launch->input, STDIN_FILENO);
    (void)::dup2(launch->output, STDOUT_FILENO);
    (void)::dup2(error, STDERR_FILENO);
    for (int descriptor = STDIN_FILENO; descriptor <= STDERR_FILENO; ++descriptor) {
        const int flags = ::fcntl(descriptor, F_GETFD);
        (void)::fcntl(descriptor, F_SETFD, flags & ~FD_CLOEXEC);
    }
    // Config.none preserves only the three standard streams and the startup
    // error pipe, exactly as std.process1172-1179 does before closefrom(4).
    if (::dup2(launch->reportWrite, 3) == -1) abortChild(launch->reportWrite, 7, errno);
    const int flags = ::fcntl(3, F_GETFD);
    (void)::fcntl(3, F_SETFD, flags | FD_CLOEXEC);
    closeFrom(4, 3);
    ::execve(launch->executable, launch->arguments, launch->environment);
    abortChild(3, 1, errno);
}
void recordStatus(ProcessPipes& process, int status) {
    process.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) :
        (WIFSIGNALED(status) ? -WTERMSIG(status) : 0);
    process.terminated = true;
    process.processId = -1;
}
}
ProcessPipes::~ProcessPipes() {
    if (input) std::fclose(input);
    if (output) std::fclose(output);
}
void ProcessPipes::closeInput() {
    auto* file = std::exchange(input, nullptr);
    if (file && std::fclose(file) != 0) {
        errnoError(allocateVideoStorage([&] { return "Could not close file `" + inputName + "'"; }), 693);
    }
}
bool ProcessPipes::inputError() const { return !input || std::ferror(input); }
void ProcessPipes::writeFrame(std::span<const std::uint8_t> bytes) {
    if (!input) compat::ngFailNullAccess(false);
    auto written = std::fwrite(bytes.data(), 1, bytes.size(), input);
    if (written == static_cast<std::size_t>(-1)) written = 0;
    if (written != bytes.size()) {
        errnoError(allocateVideoStorage([&] {
            return "Wrote " + std::to_string(written) + " instead of " +
                std::to_string(bytes.size()) + " objects of type ubyte to file `" + inputName + "'";
        }), 1214);
    }
    if (std::fflush(input) != 0) errnoError(nullptr, 1029);
}
bool ProcessPipes::poll() {
    if (terminated) return true;
    if (processId < 0) throw compat::ngStaticError<compat::AssertError>(
        VideoString::fromStaticLiteral("Called tryWait on a null Pid."),
        VideoString::fromStaticLiteral("std/process.d"), 2708);
    for (;;) {
        int status = 0;
        const auto result = ::waitpid(static_cast<pid_t>(processId), &status, WNOHANG);
        if (result < 0) {
            if (errno == ECHILD) throw compat::ProcessException(
                VideoString::fromStaticLiteral("Process does not exist or is not a child process."), 2413);
            if (errno != EINTR) throw compat::ngStaticError<compat::AssertError>(VideoString{},
                VideoString::fromStaticLiteral("std/process.d"), 2420);
            continue;
        }
        if (result == 0) return false;
        if (!WIFEXITED(status) && !WIFSIGNALED(status)) return false;
        recordStatus(*this, status);
        return true;
    }
}
std::int32_t ProcessPipes::wait() {
    if (terminated) return exitCode;
    if (processId < 0) throw compat::ngStaticError<compat::AssertError>(
        VideoString::fromStaticLiteral("Called wait on a null Pid."),
        VideoString::fromStaticLiteral("std/process.d"), 2577);
    for (;;) {
        int status = 0;
        const auto result = ::waitpid(static_cast<pid_t>(processId), &status, 0);
        if (result < 0) {
            if (errno == ECHILD) throw compat::ProcessException(
                VideoString::fromStaticLiteral("Process does not exist or is not a child process."), 2413);
            if (errno != EINTR) throw compat::ngStaticError<compat::AssertError>(VideoString{},
                VideoString::fromStaticLiteral("std/process.d"), 2420);
            continue;
        }
        if (!WIFEXITED(status) && !WIFSIGNALED(status)) continue;
        recordStatus(*this, status);
        return exitCode;
    }
}
std::shared_ptr<ProcessPipes> launch(compat::Slice<VideoString> arguments,
    bool captureOutput, bool suppressConsole) {
    (void)suppressConsole; // Config.suppressConsole is Windows-only in the source.
    namespace stdio = providers::mcp::stdio;
    stdio::SharedFile inheritedInput, inheritedOutput, inheritedError;
    if (captureOutput) inheritedInput = stdio::standardInput();

    // Source pipeProcessImpl creates both actual File ends before spawnProcess.
    Descriptor read, write;
    makePipe(read, write);
    auto releaseFile = [](std::FILE* file) { if (file) (void)std::fclose(file); };
    using FileOwner = std::unique_ptr<std::FILE, decltype(releaseFile)>;
    FileOwner readFile(::fdopen(read.value, "r"), releaseFile);
    if (!readFile) stdioError(errno, "Cannot open read end of pipe");
    read.take();
    FileOwner writeFile(::fdopen(write.value, "w"), releaseFile);
    if (!writeFile) stdioError(errno, "Cannot open write end of pipe");
    write.take();
    if (!captureOutput) {
        inheritedOutput = stdio::standardOutput();
        inheritedError = stdio::standardError();
    }

    // std.process891-909 resolves PATH before forming argv; argv[0] is the
    // actual chainPath result, not merely the unresolved original command.
    if (arguments.empty()) throw compat::RangeError(VideoString::fromStaticLiteral("std/process.d"), 891);
    VideoString executable = arguments[0];
    if (executable.view().find('/') == std::string_view::npos) {
        const auto found = searchPathFor(executable);
        if (!found) {
            const auto message = allocateVideoStorage([&] {
                return VideoString("Executable file not found: " + std::string(executable.view()));
            });
            throw compat::ProcessException(message, 898);
        }
        executable = *found;
    }
    std::vector<compat::SourceCString> storage;
    allocateVideoStorage([&] { storage.reserve(arguments.size()); });
    storage.emplace_back(compat::RetainedStringSlice(executable));
    for (std::size_t i = 1; i < arguments.size(); ++i)
        storage.emplace_back(compat::RetainedStringSlice(arguments[i]));
    std::vector<char*> argv;
    allocateVideoStorage([&] { argv.reserve(storage.size() + 1); });
    for (const auto& argument : storage) {
        auto* pointer = const_cast<char*>(argument.data());
        allocateVideoStorage([&] { argv.push_back(pointer); });
    }
    allocateVideoStorage([&] { argv.push_back(nullptr); });
    char** const environment = processEnvironment(); // null source env inherits the real current environment.
    const int input = captureOutput ? static_cast<int>(inheritedInput.nativeHandle()) : ::fileno(readFile.get());
    const int output = captureOutput ? ::fileno(writeFile.get()) : static_cast<int>(inheritedOutput.nativeHandle());
    const int error = captureOutput ? output : static_cast<int>(inheritedError.nativeHandle());

    Descriptor reportRead, reportWrite;
    int descriptors[2];
    if (::pipe(descriptors) != 0)
        processError(errno, "Could not create pipe to check startup of child", 944);
    reportRead.value = descriptors[0]; reportWrite.value = descriptors[1];
    const int flags = ::fcntl(reportWrite.value, F_GETFD);
    (void)::fcntl(reportWrite.value, F_SETFD, flags | FD_CLOEXEC);

    const ChildLaunch child{input, output, error, reportRead.value, reportWrite.value,
        storage[0].data(), argv.data(), environment};
    const auto id = ::fork();
    if (id < 0) {
        (void)::close(reportWrite.take());
        processError(errno, "Failed to spawn new process", 983);
    }
    if (id == 0) executeChild(&child); // noreturn; no C++/SDK owners are touched in the child.

    (void)::close(reportWrite.take());
    unsigned char status = 0; // InternalError : ubyte, std.process858-868.
    const auto statusCount = ::read(reportRead.value, &status, sizeof(status));
    const int readError = errno;
    if (statusCount == -1)
        processError(readError, "Could not read from pipe to get child status", 1264);
    if (status != 0) {
        int childError = 0;
        const auto errorCount = ::read(reportRead.value, &childError, sizeof(childError));
        std::string message;
        if (status == 1) message = allocateVideoStorage([&] {
            return "Failed to execute '" + std::string(executable.view()) + "'";
        });
        else if (status == 3) message = allocateVideoStorage([] { return std::string("getrlimit failed"); });
        else if (status == 5) message = allocateVideoStorage([] { return std::string("Failed to allocate memory"); });
        else if (status == 7) message = allocateVideoStorage([] {
            return std::string("Failed to close inherited file descriptors");
        });
        else throw compat::ngStaticError<compat::SwitchError>(
            VideoString::fromStaticLiteral("No appropriate switch clause found"),
            VideoString::fromStaticLiteral("std/process.d"), 1272);
        if (errorCount == sizeof(childError)) processError(childError, message.c_str(), 1302);
        const auto retained = allocateVideoStorage([&] { return VideoString(std::move(message)); });
        throw compat::ProcessException(retained, 1303);
    }

    // The source closes its nonstandard child File end after successful startup.
    auto& childFile = captureOutput ? writeFile : readFile;
    if (::fileno(childFile.get()) > STDERR_FILENO) {
        if (std::fclose(childFile.release()) != 0)
            errnoError("Could not close file `'", 693);
    }
    auto result = std::allocate_shared<ProcessPipes>(
        nicxlive::core::d_runtime::SourceAllocator<ProcessPipes>{});
    result->processId = id;
    if (captureOutput) result->output = readFile.release();
    else result->input = writeFile.release();
    return result;
}
std::pair<std::int32_t, std::string> execute(compat::Slice<VideoString> arguments) {
    auto process = launch(arguments, true, false);
    std::string output;
    const auto readChunk = [&](compat::Slice<std::uint8_t> buffer) {
        const auto count = std::fread(buffer.data(), 1, buffer.size(), process->output);
        if (count < buffer.size() && std::ferror(process->output)) errnoError(nullptr, 1116);
        return buffer.slice(0, count);
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
} // namespace nijigenerate::io::videoexport::detail

namespace nijigenerate::compat::phobos {
ExecuteShellResult ngExecuteShell(RetainedString command) {
    // std.process.spawnShell forwards exactly these three argv elements.
#if defined(__ANDROID__)
    auto shell = RetainedString::fromStaticLiteral("/system/bin/sh");
#else
    auto shell = RetainedString::fromStaticLiteral("/bin/sh");
#endif
    auto result = ::nijigenerate::io::videoexport::detail::execute(
        {shell, RetainedString::fromStaticLiteral("-c"), std::move(command)});
    auto output = ::nijigenerate::io::videoexport::detail::allocateVideoStorage([&] {
        return result.second.empty() ? RetainedString{} : RetainedString(std::move(result.second));
    });
    return {result.first, std::move(output)};
}
}
