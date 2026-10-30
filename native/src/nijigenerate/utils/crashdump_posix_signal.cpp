#include "crashdump.hpp"
#include "crashdump_posix_signal_internal.hpp"
#include "crashdump_posix_io_internal.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string_view>
#include <sys/types.h>
#include <unistd.h>
#if defined(__linux__) || defined(__APPLE__)
#include <execinfo.h>
#endif

namespace nijigenerate::utils::crashdump::detail {
namespace {
int signalExitCode(int signal) noexcept {
    static_assert(sizeof(int) == sizeof(std::int32_t));
    return std::bit_cast<std::int32_t>(std::uint32_t{128} + static_cast<std::uint32_t>(signal));
}
} // namespace

void writeNativeCrashDumpFromSignal(int signal) {
    int fd = -1;
    if (nativeCrashDumpPathLen > 0) {
        fd = ::open(reinterpret_cast<const char*>(nativeCrashDumpPath.data()),
                    O_WRONLY | O_CREAT | O_TRUNC, static_cast<mode_t>(384));
    }
    if (fd >= 0) {
        writeAll(fd, "=== nijigenerate native crashdump ===\n");
        writeAll(fd, "signal: ");
        writeInt(fd, signal);
        writeAll(fd, "\npid: ");
        writeInt(fd, ::getpid());
        writeAll(fd, "\npath: ");
        const auto pathLength = nativeCrashDumpPathLen;
        if (pathLength > nativeCrashDumpPath.size()) throw compat::ngStaticError<compat::ArraySliceError>(
            0, pathLength, nativeCrashDumpPath.size(),
            compat::RetainedString::fromStaticLiteral("source/nijigenerate/utils/crashdump.d"), 360);
        writeAll(fd, std::string_view(reinterpret_cast<const char*>(nativeCrashDumpPath.data()),
                                      pathLength));
        writeAll(fd, "\n\n=== Backtrace ===\n");
#if defined(__linux__) || defined(__APPLE__)
        void* frames[nativeBacktraceMaxFrames]{};
        const auto count = ::backtrace(frames, nativeBacktraceMaxFrames);
        ::backtrace_symbols_fd(frames, count, fd);
#else
        writeAll(fd, "backtrace unavailable on this platform\n");
#endif
        writeAll(fd, "\n\nSymbolication hint:\n");
        writeAll(fd, "  atos -o <nijigenerate-binary> -arch arm64 <address>\n");
        (void)::close(fd);
    }
}

void notifyNativeCrashUserAfterSignal() noexcept {
    if (nativeCrashExePathLen == 0) return;
    if (::fork() != 0) return;
    auto* executable = reinterpret_cast<char*>(nativeCrashExePath.data());
    auto* path = reinterpret_cast<char*>(nativeCrashDumpPath.data());
    static constexpr char notificationArgument[] = "--crash-notify";
    char* arguments[] = {executable, const_cast<char*>(notificationArgument), path, nullptr};
    (void)::execv(executable, arguments);
    ::_exit(127);
}

extern "C" void nativeCrashSignalHandler(int signal, siginfo_t*, void*) {
    if (nativeCrashHandlerActive) ::_exit(signalExitCode(signal));
    nativeCrashHandlerActive = 1;
    writeNativeCrashDumpFromSignal(signal);
    notifyNativeCrashUserAfterSignal();
    (void)::raise(signal);
    ::_exit(signalExitCode(signal));
}

void installNativeCrashHandlerFor(int signal) {
    struct sigaction action;
    std::memset(&action, 0, sizeof(action));
    action.sa_sigaction = &nativeCrashSignalHandler;
    action.sa_flags = SA_SIGINFO | SA_RESETHAND | SA_ONSTACK;
    (void)::sigemptyset(&action.sa_mask);
    (void)::sigaction(signal, &action, nullptr);
}

} // namespace nijigenerate::utils::crashdump::detail

namespace nijigenerate::utils::crashdump {

void installNativeCrashDumpThreadHandler() {
    // Match the imported druntime constant, including when glibc's C macro is dynamic.
#if defined(__GLIBC__)
    constexpr std::size_t sourceSignalStackSize = 8192;
#elif defined(__APPLE__)
    constexpr std::size_t sourceSignalStackSize = 131072;
#else
#error Map the selected druntime SIGSTKSZ constant before accepting this platform.
#endif
    if (nativeSignalStack == nullptr) {
        nativeSignalStack = std::malloc(sourceSignalStackSize);
        if (nativeSignalStack != nullptr) {
            stack_t stack{};
            stack.ss_sp = nativeSignalStack;
            stack.ss_size = sourceSignalStackSize;
            stack.ss_flags = 0;
            (void)::sigaltstack(&stack, nullptr);
        }
    }
}

} // namespace nijigenerate::utils::crashdump
