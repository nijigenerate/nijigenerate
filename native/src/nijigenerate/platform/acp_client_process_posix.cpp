// Used std.process.pipeProcess/spawnProcess(Config.none) closure, Phobos BSL-1.0.
// See port/licenses/phobos-videoexport.txt. This supplies source, not an executed process.
#include <nijigenerate/api/acp/client_process.hpp>
#include <nijigenerate/compat/phobos_process_exception.hpp>
#include <nijigenerate/compat/phobos_file_exception.hpp>
#include <nijigenerate/compat/source_c_string.hpp>
#include <nijigenerate/compat/thread_exception.hpp>
#include <nijigenerate/api/acp/detail/client_storage.hpp>
#include <compat/source_allocator.hpp>
#include <compat/range_error.hpp>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <string>
#include <utility>
#include <system_error>
#include <array>
#include <algorithm>
#include <sys/select.h>
#ifdef __APPLE__
#include <crt_externs.h>
#else
extern char** environ;
#endif

namespace nijigenerate::api::acp::client::detail {
namespace {
using providers::mcp::stdio::SharedFile;
using providers::mcp::stdio::StdioException;
using String = compat::RetainedStringSlice;
[[noreturn]] void processFailure(std::string_view message, std::size_t line, int error = 0) {
    const auto system = error ? compat::file_exception_detail::systemMessage(error) : compat::RetainedString{};
    const auto retained = error ? concatenateClientText({message, " (", system.view(), ")"})
        : retainClientText(copyClientText(message));
    throw compat::ProcessException(retained, line);
}
struct Descriptor {
    int value = -1;
    ~Descriptor() { if (value >= 0) ::close(value); }
    int take() noexcept { return std::exchange(value, -1); }
};
struct Pipe {
    SharedFile read, write;
};
Pipe makePipe() {
    int descriptors[2];
    if (::pipe(descriptors) != 0) throw StdioException(compat::RetainedString::fromStaticLiteral("Unable to create pipe"));
    Descriptor read{descriptors[0]}, write{descriptors[1]};
    auto* readFile = ::fdopen(read.value, "r");
    if (!readFile) throw StdioException(compat::RetainedString::fromStaticLiteral("Cannot open read end of pipe"));
    read.take();
    SharedFile readOwner = ngOwnClientFile(readFile);
    auto* writeFile = ::fdopen(write.value, "w");
    if (!writeFile) throw StdioException(compat::RetainedString::fromStaticLiteral("Cannot open write end of pipe"));
    write.take();
    return {readOwner, ngOwnClientFile(writeFile)};
}
std::string executablePath(String argument) {
    auto text = copyClientText(argument.view());
    if (text.find('/') != std::string::npos) return text;
    const auto* path = std::getenv("PATH");
    if (path && *path) {
        std::string_view directories(path);
        for (std::size_t first = 0;;) {
            auto last = directories.find(':', first);
            auto directory = directories.substr(first, last == std::string_view::npos ? last : last - first);
            auto candidate = directory.empty() ? copyClientText(text)
                : concatenateClientBytes({directory, "/", text});
            if (::access(candidate.c_str(), X_OK) == 0) return candidate;
            if (last == std::string_view::npos) break;
            first = last + 1;
        }
    }
    processFailure(concatenateClientBytes({"Executable file not found: ", text}), 898);
}
enum class ChildError : unsigned char { none, exec, chdir, getrlimit, doubleFork, malloc, preExec, closefdsDup2 };
[[noreturn]] void abortChild(int pipe, ChildError kind, int error) noexcept {
    ::write(pipe, &kind, sizeof(kind));
    ::write(pipe, &error, sizeof(error));
    ::close(pipe);
    ::_exit(1);
}
}

struct Pid::State { pid_t id = -1; };
std::int32_t Pid::processID() const { return state ? static_cast<std::int32_t>(state->id) : -1; }
void ngJoinClientThread(std::thread& thread) {
    try { thread.join(); }
    catch (const std::system_error&) {
        compat::ngThrowThreadException(compat::RetainedString::fromStaticLiteral("Unable to join thread"),
            compat::RetainedString::fromStaticLiteral("core/thread/osthread.d"), 617);
    }
}
ProcessPipes ngLaunchClient(compat::Slice<String> command, String cwd) {
    // pipeProcess creates all three real pipes before spawnProcess checks args.
    auto input = makePipe();
    auto output = makePipe();
    auto error = makePipe();
    if (command.empty()) throw ::nicxlive::core::d_runtime::RangeError(
        compat::RetainedString::fromStaticLiteral("std/process.d"), 892);
    const auto program = executablePath(command[0]);
    std::vector<compat::SourceCString> arguments;
    allocateClientStorage([&] { arguments.reserve(command.size()); });
    arguments.emplace_back(retainClientText(copyClientText(program)));
    for (std::size_t i = 1; i < command.size(); ++i) arguments.emplace_back(command[i]);
    std::vector<char*> argv;
    allocateClientStorage([&] { argv.reserve(arguments.size() + 1); });
    for (const auto& argument : arguments) argv.push_back(const_cast<char*>(argument.data()));
    argv.push_back(nullptr);
    Descriptor directory;
    if (!cwd.empty()) {
        compat::SourceCString path(cwd);
        directory.value = ::open(path.data(), O_RDONLY);
        if (directory.value < 0) processFailure("Failed to open working directory", 921, errno);
        struct stat info{};
        if (::fstat(directory.value, &info) < 0) processFailure("Failed to stat working directory", 924, errno);
        if (!S_ISDIR(info.st_mode)) processFailure(concatenateClientBytes({"Not a directory: ", cwd.view()}), 926);
    }
    const auto stdinFD = static_cast<int>(input.read.nativeHandle());
    const auto stdoutFD = static_cast<int>(output.write.nativeHandle());
    const auto stderrFD = static_cast<int>(error.write.nativeHandle());
    int forkPipe[2];
    if (::pipe(forkPipe) != 0) processFailure("Could not create pipe to check startup of child", 944, errno);
    Descriptor statusRead{forkPipe[0]}, statusWrite{forkPipe[1]};
    ::fcntl(statusWrite.value, F_SETFD, FD_CLOEXEC);
#ifdef __APPLE__
    auto environment = *_NSGetEnviron();
#else
    auto environment = environ;
#endif
    const auto id = ::fork();
    if (id < 0) {
        ::close(statusWrite.take());
        processFailure("Failed to spawn new process", 983, errno);
    }
    if (id == 0) {
        ::close(statusRead.value);
        int report = statusWrite.value;
        if (directory.value >= 0) {
            if (::fchdir(directory.value) < 0) abortChild(report, ChildError::chdir, errno);
            ::close(directory.value);
        }
        ::dup2(stdinFD, STDIN_FILENO);
        ::dup2(stdoutFD, STDOUT_FILENO);
        ::dup2(stderrFD, STDERR_FILENO);
        ::fcntl(STDIN_FILENO, F_SETFD, 0);
        ::fcntl(STDOUT_FILENO, F_SETFD, 0);
        ::fcntl(STDERR_FILENO, F_SETFD, 0);
        if (::dup2(report, 3) < 0) abortChild(report, ChildError::closefdsDup2, errno);
        report = 3;
        ::fcntl(report, F_SETFD, FD_CLOEXEC);
        rlimit limits{};
        if (::getrlimit(RLIMIT_NOFILE, &limits) != 0) abortChild(report, ChildError::getrlimit, errno);
        // Config.none closes every inherited nonstandard descriptor. No allocation
        // or C++ destruction occurs in this post-fork child before exec/_exit.
        const auto maximum = static_cast<int>(limits.rlim_cur);
        for (int fd = 4; fd < maximum; ++fd) ::close(fd);
        ::execve(argv[0], argv.data(), environment);
        abortChild(report, ChildError::exec, errno);
    }
    ::close(statusWrite.take());
    ChildError kind = ChildError::none;
    auto count = ::read(statusRead.value, &kind, sizeof(kind));
    const auto statusError = errno;
    if (count < 0) processFailure("Could not read from pipe to get child status", 1264, statusError);
    if (kind != ChildError::none) {
        int number = 0;
        count = ::read(statusRead.value, &number, sizeof(number));
        const auto message = allocateClientStorage([&] {
            std::string text;
            switch (kind) {
            case ChildError::chdir: text = "Failed to set working directory"; break;
            case ChildError::getrlimit: text = "getrlimit failed"; break;
            case ChildError::exec: text = "Failed to execute '" + program + "'"; break;
            case ChildError::closefdsDup2: text = "Failed to close inherited file descriptors"; break;
            case ChildError::doubleFork: text = "Failed to fork twice"; break;
            case ChildError::malloc: text = "Failed to allocate memory"; break;
            case ChildError::preExec: text = "Failed to execute preExecFunction or preExecDelegate"; break;
            case ChildError::none: break;
            }
            return text;
        });
        processFailure(std::move(message), count == sizeof(number) ? 1302 : 1303,
            count == sizeof(number) ? number : 0);
    }
    input.read.close();
    output.write.close();
    error.write.close();
    ProcessPipes result;
    result.pid.state = std::allocate_shared<Pid::State>(nicxlive::core::d_runtime::SourceAllocator<Pid::State>{});
    result.pid.state->id = id;
    result.input = input.write; result.output = output.read; result.error = error.read;
    return result;
}
String ngCurrentDirectory() {
#ifdef __sun
    auto* bytes = ::getcwd(nullptr, 4096);
#else
    auto* bytes = ::getcwd(nullptr, 0);
#endif
    if (!bytes) throw compat::FileException(compat::RetainedString::fromStaticLiteral("cannot get cwd"));
    struct Release { char* bytes; ~Release() { std::free(bytes); } } release{bytes};
    return retainClientText(copyClientText(bytes));
}
int ngClientStdoutFd(const SharedFile& file) {
    if (!file.isOpen()) throw compat::ThrowableException(
        duplicateClientText("Attempting to call fileno() on an unopened file"),
        compat::RetainedString::fromStaticLiteral("std/stdio.d"), 2318);
    return static_cast<int>(file.nativeHandle());
}
bool ngReadClientChunk(const SharedFile& file, String& chunk) {
    if (!file.isOpen()) return false;
    const auto fd = ngClientStdoutFd(file);
    if (fd < 0) return false;
    fd_set descriptors;
    FD_ZERO(&descriptors);
    FD_SET(fd, &descriptors);
    timeval timeout{};
    if (::select(fd + 1, &descriptors, nullptr, nullptr, &timeout) <= 0) return false;
    std::array<std::uint8_t, 4096> buffer;
    const auto read = ::read(fd, buffer.data(), buffer.size());
    if (read <= 0) return false;
    compat::Slice<std::uint8_t> bytes;
    bytes.resize(static_cast<std::size_t>(read));
    std::copy_n(buffer.data(), static_cast<std::size_t>(read), bytes.data());
    chunk = compat::RetainedString::retainedView(bytes.retentionOwner(),
        reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return true;
}
}
