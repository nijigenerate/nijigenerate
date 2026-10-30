// Phobos executable-path behavior: see native/port/licenses/phobos-crash-path.txt.
#include "crashdump.hpp"
#include "crashdump_posix_internal.hpp"
#include "crashdump_posix_signal_internal.hpp"
#include "crashdump_file_error.hpp"
#include <nijigenerate/compat/phobos_temp_cstring.hpp>
#include <nijigenerate/compat/array_range_error.hpp>
#include <nijigenerate/compat/phobos_stdio.hpp>
#include <array>
#include <bit>
#include <cerrno>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace nijigenerate::utils::crashdump {
namespace {

compat::RetainedString currentExecutablePath() {
    constexpr auto initialChar = std::bit_cast<char>(std::uint8_t{0xff});
#if defined(__APPLE__)
    std::uint32_t size = 0;
    (void)::_NSGetExecutablePath(nullptr, &size);
    auto buffer = detail::allocateCrashText([&] { return std::vector<char>(size, initialChar); });
    (void)::_NSGetExecutablePath(buffer.data(), &size);
    std::unique_ptr<char, decltype(&std::free)> absolute(::realpath(buffer.data(), nullptr), &std::free);
    if (!absolute) throw providers::mcp::stdio::ErrnoException(compat::RetainedString{}, errno,
        compat::RetainedString::fromStaticLiteral("std/file.d"), 3496);
    return detail::allocateCrashText([&] { return compat::RetainedString(std::string(absolute.get())); });
#elif defined(__linux__)
    static constexpr char path[] = "/proc/self/exe";
    std::array<char, 2048> buffer;
    buffer.fill(initialChar);
    const compat::phobos::TempCStringBuffer<char> nativePath{std::string_view(path)};
    auto size = ::readlink(nativePath.c_str(), buffer.data(), buffer.size());
    if (size == -1)
        detail::failCrashFile(compat::RetainedString::fromStaticLiteral(path),
            static_cast<std::uint32_t>(errno), 3330);
    if (size <= static_cast<ssize_t>(buffer.size() - 6)) {
        const auto extent = static_cast<std::size_t>(size);
        if (extent > buffer.size()) throw compat::ngStaticError<compat::ArraySliceError>(
            0, extent, buffer.size(), compat::RetainedString::fromStaticLiteral("std/file.d"), 3333);
        return detail::allocateCrashText([&] {
            return compat::RetainedString(std::string_view(buffer.data(), extent));
        });
    }
    auto dynamicBuffer = detail::allocateCrashText([&] {
        return std::allocate_shared<std::vector<char>>(
            nicxlive::core::d_runtime::SourceAllocator<std::vector<char>>{}, buffer.size() * 3 / 2, initialChar);
    });
    for (int attempt = 0; attempt < 10; ++attempt) {
        size = ::readlink(nativePath.c_str(), dynamicBuffer->data(), dynamicBuffer->size());
        if (size == -1)
            detail::failCrashFile(compat::RetainedString::fromStaticLiteral(path),
                static_cast<std::uint32_t>(errno), 3343);
        if (static_cast<std::size_t>(size) <= dynamicBuffer->size() - 6) {
            const auto extent = static_cast<std::size_t>(size);
            const auto bytes = dynamicBuffer->data();
            dynamicBuffer->resize(extent);
            return compat::RetainedString::retainedView(dynamicBuffer, bytes, extent);
        }
        detail::allocateCrashText([&] {
            dynamicBuffer->resize(dynamicBuffer->size() * 3 / 2, initialChar);
        });
    }
    compat::FileException error(compat::RetainedString::fromStaticLiteral(path),
        compat::RetainedString::fromStaticLiteral("Path is too long to read."));
    error.file = compat::RetainedString::fromStaticLiteral("std/file.d");
    error.line = 3356;
    throw error;
#else
#error Map the actual druntime thisExePath provider before accepting this platform.
#endif
}

} // namespace

void installNativeCrashDumpHandler() {
    {
        const auto executablePath = currentExecutablePath();
        detail::copyNativeCrashDumpPath(executablePath.view());
    }
    nativeCrashExePath = nativeCrashDumpPath;
    nativeCrashExePathLen = nativeCrashDumpPathLen;
    mkdirCrashDumpDir();
    const auto path = genCrashDumpPath(compat::RetainedString::fromStaticLiteral("nijigenerate-native-crashdump"));
    detail::copyNativeCrashDumpPath(path.view());
    installNativeCrashDumpThreadHandler();
    detail::installNativeCrashHandlerFor(SIGSEGV);
}

} // namespace nijigenerate::utils::crashdump
