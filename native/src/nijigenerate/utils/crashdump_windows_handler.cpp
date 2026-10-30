#include "crashdump.hpp"
#include "crashdump_windows_internal.hpp"
#include <nijigenerate/compat/array_range_error.hpp>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>

namespace nijigenerate::utils::crashdump {
namespace {
LONG WINAPI nativeCrashExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
    using namespace detail;
    if (nativeCrashHandlerActive) return EXCEPTION_EXECUTE_HANDLER;
    nativeCrashHandlerActive = 1;
    const auto textFile = nativeCrashDumpPathWLen > 0 ? createCrashFile(nativeCrashDumpPathW.data()) : nullptr;
    const auto dumpFile = nativeCrashMiniDumpPathWLen > 0 ? createCrashFile(nativeCrashMiniDumpPathW.data()) : nullptr;
    if (validNativeHandle(textFile)) {
        writeAll(textFile, "=== nijigenerate native crashdump ===\r\n");
        writeAll(textFile, "pid: "); writeInt(textFile, ::GetCurrentProcessId());
        writeAll(textFile, "\r\nthread: "); writeInt(textFile, ::GetCurrentThreadId());
        if (exceptionInfo && exceptionInfo->ExceptionRecord) {
            writeAll(textFile, "\r\nexception code: ");
            writeHex(textFile, exceptionInfo->ExceptionRecord->ExceptionCode);
            writeAll(textFile, "\r\nexception address: ");
            writeHex(textFile, reinterpret_cast<std::size_t>(exceptionInfo->ExceptionRecord->ExceptionAddress));
        }
        writeAll(textFile, "\r\nminidump: ");
        if (nativeCrashMiniDumpPathWLen > 0) {
            const auto pathLength = nativeCrashMiniDumpPathWLen;
            if (pathLength > nativeCrashMiniDumpPathW.size())
                throw compat::ngStaticError<compat::ArraySliceError>(0, pathLength,
                    nativeCrashMiniDumpPathW.size(),
                    compat::RetainedString::fromStaticLiteral("source/nijigenerate/utils/crashdump.d"), 239);
            for (std::size_t i = 0; i < pathLength; ++i) {
                const auto ch = nativeCrashMiniDumpPathW[i];
                const char c = ch < 128 ? static_cast<char>(ch) : '?';
                writeAll(textFile, std::string_view(&c, 1));
            }
        } else writeAll(textFile, "(unavailable)");
        writeAll(textFile, "\r\n\r\nAttach both this text file and the .dmp file when filing an issue.\r\n");
    }
    if (validNativeHandle(dumpFile)) {
        SourceMiniDumpExceptionInformation sourceException;
        sourceException.ThreadId = ::GetCurrentThreadId();
        sourceException.ExceptionPointers = exceptionInfo;
        sourceException.ClientPointers = nativeFalse;
        constexpr auto kind = static_cast<MINIDUMP_TYPE>(MiniDumpNormal | MiniDumpWithDataSegs |
            MiniDumpWithHandleData | MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo);
        // Preserve Source's natural record at the external ABI boundary,
        // including its known mismatch with the packed4 SDK declaration.
        // App code never accesses the record through this adapted pointer.
        ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), dumpFile, kind,
            reinterpret_cast<MINIDUMP_EXCEPTION_INFORMATION*>(&sourceException), nullptr, nullptr);
    }
    if (validNativeHandle(dumpFile)) ::CloseHandle(dumpFile);
    if (validNativeHandle(textFile)) ::CloseHandle(textFile);
    notifyNativeCrashUser();
    return EXCEPTION_EXECUTE_HANDLER;
}
}

void installNativeCrashDumpHandler() {
    mkdirCrashDumpDir();
    const auto textPath = genCrashDumpPath(compat::RetainedString::fromStaticLiteral("nijigenerate-native-crashdump"));
    auto dumpPath = detail::allocateCrashText([&] { return std::string(textPath.view()); });
    // genCrashDumpPath always contributes the final .txt extension itself.
    detail::allocateCrashText([&] { dumpPath.replace(dumpPath.size() - 3, 3, "dmp"); });
    detail::copyNativeCrashPath(nativeCrashDumpPathW, nativeCrashDumpPathWLen, textPath.view());
    detail::copyNativeCrashPath(nativeCrashMiniDumpPathW, nativeCrashMiniDumpPathWLen, dumpPath);
    ::SetUnhandledExceptionFilter(&nativeCrashExceptionHandler);
}
}
